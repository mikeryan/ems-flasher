/* 
 * Replace ems.c. Performs cartridge I/O operations on an image file.
 * The file is an image of a full cartridge (2 pages). Use split -n2 to split
 * the image into two pages.
 *
 * Environment variables:
 *   IMAGEFILE: path to the image file (image.gb by default)
 *
 * Attention:
 *   - SRAM operations are not implemented.
 *   - Doesn't simulate the qwirks of the cartridge (read doesn't block when
 *     preceded by an odd number of writes, a write always succeeds on a
 *     non-blank erase-block, ...). See the Tech file.
 */
#include <assert.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ems.h"

#define DEFAULTIMAGEFILE "image.gb"

static FILE *imagef;
static char *imagepath;

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
  //  char *mode;
    void ems_deinit(void);

    // call the cleanup when we're done
    atexit(ems_deinit);

    if ((imagepath = getenv("IMAGEFILE")) == NULL)
        imagepath = DEFAULTIMAGEFILE;

    if ((imagef = fopen(imagepath, "r+b")) == NULL) {
        if ((imagef = fopen(imagepath, "w+b")) == NULL)
            err(1, "can't open (or create) %s", imagepath);
    }

    return 0;
}

/**
 * Cleanup / release the device. Registered with atexit.
 */
void ems_deinit(void) {
    if (imagef != NULL)
        fclose(imagef);
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
    size_t bytes;

    assert(from == FROM_ROM || from == FROM_SRAM);

    if (from == FROM_SRAM)
        errx(1, "ems_read from SRAM is not supported");

    if (fseek(imagef, offset, SEEK_SET) == -1)
        err(1, "seek (offset=%ld)", (long)offset);
    if ((bytes = fread(buf, 1, count, imagef)) < count) {
        if (ferror(imagef))
            err(1, "read error (offset=%ld)", (long)offset);
        memset(&buf[bytes], 0xff, count - bytes);
    }

    return count;
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
    assert(to == TO_ROM || to == TO_SRAM);

    if (to == TO_SRAM)
        errx(1, "ems_write to SRAM not supported");

    if (offset % ERASEBLOCKSIZE == 0) {
        ems_size_t remaining;
        unsigned char buf[4096];

        memset(buf, 0xff, 4096);

        if (fseek(imagef, offset, SEEK_SET) == -1)
            err(1, "seek (offset=%ld)", (long)offset);
        for (remaining = ERASEBLOCKSIZE; remaining > 0; remaining -= 4096)
            if (fwrite(buf, 1, 4096, imagef) < 4096)
                err(1, "write error (offset=%ld)", (long)offset);
    }

    if (fseek(imagef, offset, SEEK_SET) == -1)
        err(1, "seek (offset=%ld)", (long)offset);
    if (fwrite(buf, 1, count, imagef) < count)
        err(1, "write error (offset=%ld)", (long)offset);

    return count;
}
