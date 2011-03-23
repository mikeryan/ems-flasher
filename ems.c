#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <arpa/inet.h> /* for htonl */

#include <libusb.h>

/* magic numbers! */
#define EMS_VID 0x4670
#define EMS_PID 0x9394

/* options */
typedef struct _options_t {
    int verbose;
    char *file;
} options_t;

// defaults
options_t opts = {
    .verbose            = 0,
    .file               = NULL,
};

#define EMS_EP_SEND (2 | LIBUSB_ENDPOINT_OUT)
#define EMS_EP_RECV (1 | LIBUSB_ENDPOINT_IN)

enum {
    CMD_READ    = 0xff,
    CMD_WRITE   = 0x57,
};

static struct libusb_device_handle *devh = NULL;
int claimed = 0;

/**
 * Attempt to find the EMS cart by vid/pid.
 *
 * Returns:
 *  0       success
 *  < 0     failure
 */
static int find_ems_device(void) {
    devh = libusb_open_device_with_vid_pid(NULL, EMS_VID, EMS_PID);
    return devh ? 0 : -EIO;
}

/**
 * Init the flasher. Inits libusb and claims the device. Aborts if libusb
 * can't be initialized.
 *
 * Returns:
 *  0       Success
 *  < 0     Failure
 */
int ems_init(void) {
    int r;

    r = libusb_init(NULL);
    if (r < 0) {
        fprintf(stderr, "failed to initialize libusb\n");
        exit(1); // pretty much hosed
    }

    r = find_ems_device();
    if (r < 0) {
        fprintf(stderr, "Could not find/open device, is it plugged in?\n");
        return r;
    }

    r = libusb_claim_interface(devh, 0);
    if (r < 0) {
        fprintf(stderr, "usb_claim_interface error %d\n", r);
        return r;
    }

    claimed = 1;
    return 0;
}

/**
 * Cleanup / release the device. Registered with atexit.
 */
void ems_deinit(void) {
    if (claimed)
        libusb_release_interface(devh, 0);

    libusb_close(devh);
    libusb_exit(NULL);
}

/**
 * Initialize a command buffer. Commands are a 1 byte command code followed by
 * a 4 byte address and a 4 byte value.
 *
 * buf must point to a memory chunk of size >= 9 bytes
 */
static void ems_command_init(
        unsigned char *buf, // buffer to init
        unsigned char cmd,  // command to run
        uint32_t addr,      // address
        uint32_t val        // value
) {
    buf[0] = cmd;
    *(uint32_t *)(buf + 1) = htonl(addr);
    *(uint32_t *)(buf + 5) = htonl(val);
}

/**
 * Read some bytes from the cart.
 *
 * Params:
 *  offset  absolute read address from the cart
 *  buf     buffer to read into (buffer must be at least count bytes)
 *  count   number of bytes to read
 *
 * Returns:
 *  >= 0    number of bytes read (will always == count)
 *  < 0     error sending command or reading data
 */
static int ems_read(uint32_t offset, unsigned char *buf, size_t count) {
    int r, transferred;
    unsigned char cmd_buf[9];

    ems_command_init(cmd_buf, CMD_READ, offset, count);

#ifdef DEBUG
    int i;
    for (i = 0; i < 9; ++i)
        printf("%02x ", cmd_buf[i]);
    printf("\n");
#endif

    // send the read command
    r = libusb_bulk_transfer(devh, EMS_EP_SEND, cmd_buf, sizeof(cmd_buf), &transferred, 0);
    if (r < 0)
        return r;

    // read the data
    r = libusb_bulk_transfer(devh, EMS_EP_RECV, buf, count, &transferred, 0);
    if (r < 0)
        return r;

    return transferred;
}

/**
 * Write to the cartridge.
 *
 * Params:
 *  offset  address to write to
 *  buf     data to write
 *  count   number of bytes out of buf to write
 *
 * Returns:
 *  >= 0    number of bytes written (will always == count)
 *  < 0     error writing data
 */
static int ems_write(uint32_t offset, unsigned char *buf, size_t count) {
    int r, transferred;
    unsigned char *write_buf;

    // thx libusb for having no scatter/gather io
    write_buf = malloc(count + 9);
    if (write_buf == NULL)
        err(1, "malloc");

    // set up the command buffer
    ems_command_init(write_buf, CMD_WRITE, offset, count);
    memcpy(write_buf + 9, buf, count);

    r = libusb_bulk_transfer(devh, EMS_EP_SEND, write_buf, count + 9, &transferred, 0);
    if (r == 0)
        r = transferred; // return number of bytes sent on success

    free(write_buf);

    return r;
}

/**
 * Get the options to the binary. Options are stored in the global "opts".
 */
void get_options(int argc, char **argv) {
    int c;

    while (1) {
        int option_index = 0;
        static struct option long_options[] = {
            {"verbose", 0, 0, 'v'},
            {0, 0, 0, 0}
        };

        c = getopt_long(
            argc, argv, "v",
            long_options, &option_index
        );
        if (c == -1)
            break;

        switch (c) {
            case 'v':
                opts.verbose = 1;
                break;
            default:
                break;
        }
    }

    // extra argument: ROM file
    if (optind < argc)
        opts.file = argv[optind];
}

/**
 * Main
 */
int main(int argc, char **argv) {
    int r, i;

    get_options(argc, argv);

    // call the cleanup when we're done
    atexit(ems_deinit);

    if (opts.verbose)
        printf("trying to find EMS cart\n");

    r = ems_init();
    if (r < 0)
        return 1;

    if (opts.verbose)
        printf("claimed EMS cart\n");

    // file provided: write the file
    if (opts.file != NULL) {
        FILE *write_file = fopen(opts.file, "r");
        if (write_file == NULL) {
            warn("Can't open ROM file %s", opts.file);
            return 1;
        }

        unsigned char buf[32];
        uint32_t offset = 0;

        while (fread(buf, sizeof(buf), 1, write_file) == 1) {
            r = ems_write(offset, buf, sizeof(buf));
            if (r < 0) {
                warn("can't write %d", r);
                return 1;
            }

            offset += sizeof(buf);
        }
    }

    // print the first 0x200 bytes in hex and ASCII
    if (opts.verbose) {
        unsigned char data[0x200];
        r = ems_read(0x0, data, sizeof(data));

        printf("received %d\n", r);

        for (i = 0; i < r; ++i) {
            printf("%02x ", data[i]);
            if ((i & 0xf) == 0xf)
                printf("\n");
        }

        for (i = 0; i < r; ++i) {
            char pr = isprint(data[i]) ? data[i] : '.';
            printf("%c", pr);

            if ((i & 0xf) == 0xf)
                printf("\n");
        }
    }

    return 0;
}
