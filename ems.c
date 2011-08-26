#include <assert.h>
#include <err.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h> // FIXME this will (probably) go away with error coes
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h> /* for htonl */

#include <libusb.h>

#include "ems.h"

/* magic numbers! */
#define EMS_VID 0x4670
#define EMS_PID 0x9394

#define EMS_EP_SEND (2 | LIBUSB_ENDPOINT_OUT)
#define EMS_EP_RECV (1 | LIBUSB_ENDPOINT_IN)

enum {
    CMD_READ    = 0xff,
    CMD_WRITE   = 0x57,
    CMD_READ_SRAM   = 0x6d,
    CMD_WRITE_SRAM  = 0x4d,
};

static struct libusb_device_handle *devh = NULL;
static int claimed = 0;

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
 * TODO replace printed error with return code
 *
 * Returns:
 *  0       Success
 *  < 0     Failure
 */
int ems_init(void) {
    int r;
    void ems_deinit(void);

    // call the cleanup when we're done
    atexit(ems_deinit);

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
 *  from    FROM_ROM or FROM_SRAM
 *  offset  absolute read address from the cart
 *  buf     buffer to read into (buffer must be at least count bytes)
 *  count   number of bytes to read
 *
 * Returns:
 *  >= 0    number of bytes read (will always == count)
 *  < 0     error sending command or reading data
 */
int ems_read(int from, uint32_t offset, unsigned char *buf, size_t count) {
    int r, transferred;
    unsigned char cmd;
    unsigned char cmd_buf[9];

    assert(from == FROM_ROM || from == FROM_SRAM);

    cmd = from == FROM_ROM ? CMD_READ : CMD_READ_SRAM;
    ems_command_init(cmd_buf, cmd, offset, count);

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
 *  to      TO_ROM or TO_SRAM
 *  offset  address to write to
 *  buf     data to write
 *  count   number of bytes out of buf to write
 *
 * Returns:
 *  >= 0    number of bytes written (will always == count)
 *  < 0     error writing data
 */
int ems_write(int to, uint32_t offset, unsigned char *buf, size_t count) {
    int r, transferred;
    unsigned char cmd;
    unsigned char *write_buf;

    assert(to == TO_ROM || to == TO_SRAM);
    cmd = to == TO_ROM ? CMD_WRITE : CMD_WRITE_SRAM;

    // thx libusb for having no scatter/gather io
    write_buf = malloc(count + 9);
    if (write_buf == NULL)
        err(1, "malloc");
    
    // set up the command buffer
    ems_command_init(write_buf, cmd, offset, count);
    memcpy(write_buf + 9, buf, count);

    r = libusb_bulk_transfer(devh, EMS_EP_SEND, write_buf, count + 9, &transferred, 0);
    if (r == 0)
        r = transferred; // return number of bytes sent on success

    free(write_buf);

    return r;
}
