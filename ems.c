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

// don't forget to bump this :P
#define VERSION "0.01"

// one bank is 32 megabits
#define BANK_SIZE 0x400000

/* magic numbers! */
#define EMS_VID 0x4670
#define EMS_PID 0x9394

// operation mode
#define MODE_READ   1
#define MODE_WRITE  2

/* options */
typedef struct _options_t {
    int verbose;
    int blocksize;
    int mode;
    char *file;
} options_t;

// defaults
options_t opts = {
    .verbose            = 0,
    .blocksize          = 32,
    .mode               = 0,
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
 * Usage
 */
void usage(char *name) {
    printf("Usage: %s < --read | --write > [ --verbose ] <totally_legit_rom.gb>\n", name);
    printf("       %s --version\n", name);
    printf("       %s --help\n", name);
    printf("Writes a ROM file to the first bank of an EMS 64 Mbit USB flash cart\n\n");
    printf("Options:\n");
    printf("    --read                  read entire first bank into file\n");
    printf("    --write                 write ROM file to first bank\n");
    printf("    --verbose               displays more information\n");
    printf("\n");
    printf("You MUST supply exactly one of --read or --write\n");
    printf("\n");
    printf("Advanced options:\n");
    printf("    --blocksize <size>      block size to use while writing (Windows software uses 32)\n");
    printf("\n");
    printf("Written by Mike Ryan <mikeryan@lacklustre.net>\n");
    printf("See web site for more info:\n");
    printf("    http://lacklustre.net/gb/ems/\n");
    exit(1);
}

/**
 * Get the options to the binary. Options are stored in the global "opts".
 */
void get_options(int argc, char **argv) {
    int c, optval;

    while (1) {
        int option_index = 0;
        static struct option long_options[] = {
            {"help", 0, 0, 'h'},
            {"version", 0, 0, 'V'},
            {"verbose", 0, 0, 'v'},
            {"read", 0, 0, 'r'},
            {"write", 0, 0, 'w'},
            {"blocksize", 1, 0, 's'},
            {0, 0, 0, 0}
        };

        c = getopt_long(
            argc, argv, "hVvs:rw",
            long_options, &option_index
        );
        if (c == -1)
            break;

        switch (c) {
            case 'h':
                usage(argv[0]);
                break;
            case 'V':
                printf("EMS-flasher %s\n", VERSION);
                exit(0);
                break;
            case 'v':
                opts.verbose = 1;
                break;
            case 'r':
                if (opts.mode != 0) goto mode_error;
                opts.mode = MODE_READ;
                break;
            case 'w':
                if (opts.mode != 0) goto mode_error;
                opts.mode = MODE_WRITE;
                break;
            case 's':
                optval = atoi(optarg);
                if (optval <= 0) {
                    printf("Error: block size must be > 0\n");
                    usage(argv[0]);
                }
                // TODO make sure it divides evenly into bank size
                opts.blocksize = optval;
                break;
            default:
                usage(argv[0]);
                break;
        }
    }

    // user didn't supply mode
    if (opts.mode == 0)
        goto mode_error;

    // user didn't give a filename
    if (optind >= argc) {
        printf("Error: you must provide an %s filename\n", opts.mode == MODE_READ ? "input" : "output");
        usage(argv[0]);
    }

    // extra argument: ROM file
    opts.file = argv[optind];

    return;

mode_error:
    printf("Error: must supply exactly one of --read or --write\n");
    usage(argv[0]);
}

/**
 * Main
 */
int main(int argc, char **argv) {
    int r;

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

    // we'll need a buffer one way or another
    int blocksize = opts.blocksize;
    uint32_t offset = 0;
    unsigned char *buf = malloc(blocksize);
    if (buf == NULL)
        err(1, "malloc");

    // read the ROM from bank 1 and save it into the file
    if (opts.mode == MODE_READ) {
        FILE *save_file = fopen(opts.file, "w");
        if (save_file == NULL)
            err(1, "Can't open %s for writing", opts.file);

        if (opts.verbose)
            printf("saving ROM into %s\n", opts.file);

        while (offset < BANK_SIZE) {
            r = ems_read(offset, buf, blocksize);
            if (r < 0) {
                warnx("can't read %d bytes at offset %u\n", blocksize, offset);
                return 1;
            }

            r = fwrite(buf, blocksize, 1, save_file);
            if (r != 1)
                err(1, "can't write %d bytes into ROM file at offset %u", blocksize, offset);

            offset += blocksize;
        }

        fclose(save_file);

        if (opts.verbose)
            printf("Successfully wrote %u bytes into %s\n", offset, opts.file);
    }

    // write ROM in the file to bank 1
    else if (opts.mode == MODE_WRITE) {
        FILE *write_file = fopen(opts.file, "r");
        if (write_file == NULL)
            err(1, "Can't open ROM file %s", opts.file);

        printf("Writing ROM file %s\n", opts.file);

        while (fread(buf, blocksize, 1, write_file) == 1) {
            r = ems_write(offset, buf, blocksize);
            if (r < 0) {
                warnx("can't write %d bytes at offset %u", blocksize, offset);
                return 1;
            }

            offset += blocksize;
        }

        fclose(write_file);

        if (opts.verbose)
            printf("Successfully wrote %u from %s\n", offset, opts.file);
    }

    // should never reach here
    else
        errx(1, "Unknown mode %d, file a bug report", opts.mode);

    return 0;
}
