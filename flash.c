/*
 * High-level flash I/O commands, notably those described in update.h
 *
 * This module can be initialized by flash_init() with progress_cb and
 * checkint_cb callbacks. Both callbacks are optional.
 *
 * When writing a ROM to flash memory, one piece of the header is written last
 * (containing a part of the logo), making the header invalid and thus the ROM
 * hidden until the very last block is transferred and all operations succeeded.
 *
 * The move operation delete the ROM from its source location only when it has
 * been copied completely.
 *
 * Global Variables
 *
 * flash_lastofs: higher address written on flash. This can be used to determine
 *   the last formated erase-block.
 *
 * Progression status
 *
 *   progress_cb is called for every 4 KB of transfered bytes as required by
 *   the current implementation of progress.c.
 *   The total number of bytes transferred is computed as follow:
 *         writef, read, write: "size" bytes
 *         move: 2*"size" bytes
 *         erase: 0 bytes
 *
 * Signals handling
 *
 *   checkint_cb is called regularly during a transfer and must return non-zero
 *   if an interrupt signal has been caught. A function returns EFLASH_EINTR
 *   when interrupted.
 *   The caller is responsible for blocking or redirecting signals.
 *   write() is atomic, it doesn't check for interrupts. This keeps us from
 *   recovering a partialy written ROM.
 */

#include "ems.h"
#include "flash.h"
#include "progress.h"

#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <err.h>

#define NBSLOTS 3
static unsigned char slot[NBSLOTS][ERASEBLOCKSIZE/2];

static void (*flash_progress_cb)(int, ems_size_t);
static int (*flash_checkint_cb)(void);

#define CHECKINT (flash_checkint_cb?flash_checkint_cb():0)
#define PROGRESS(type, size)                                                   \
    do {                                                                       \
        if (flash_progress_cb) flash_progress_cb(type, size);                  \
    } while (0)

void
flash_init(void (*progress_cb)(int, ems_size_t), int (*checkint_cb)(void)) {
    flash_lastofs = -1;
    flash_progress_cb = progress_cb;
    flash_checkint_cb = checkint_cb;
}

int
flash_writef(ems_size_t offset, ems_size_t size, char *path) {
    unsigned char blockbuf[WRITEBLOCKSIZE*2], blockbuf100[WRITEBLOCKSIZE*2];
    ems_size_t blockofs, progress;
    FILE *f;
    int i, r;

    if ((f = fopen(path, "rb")) == NULL) {
        warn("can't open %s", path);
        return FLASH_EFILE;
    }

    progress = 0;
    for (blockofs = 0; blockofs < size; blockofs += WRITEBLOCKSIZE*2) {    
        if (fread(blockbuf, 1, WRITEBLOCKSIZE*2, f) < WRITEBLOCKSIZE*2) {
            if (ferror(f)) {
                warn("error reading %s", path);
                fclose(f);
                return FLASH_EFILE;
            } else break;
        }

        if (blockofs == 0x100) {
            memcpy(blockbuf100, blockbuf, WRITEBLOCKSIZE*2);
            continue;
        }

        if (CHECKINT) {
            warnx("operation interrupted");
            return FLASH_EINTR;
        }

        for (i = 0; i < 2; i++) {
            if ((r = ems_write(TO_ROM,
                flash_lastofs = offset + blockofs + i*WRITEBLOCKSIZE,
                blockbuf + i*WRITEBLOCKSIZE, WRITEBLOCKSIZE)) < 0) {
                    warnx("write error flashing %s", path);
                    fclose(f);
                    return FLASH_EUSB;
            }
        }

        if ((offset + blockofs)%ERASEBLOCKSIZE == 0)
            PROGRESS(PROGRESS_ERASE, 0);

        if ((progress += WRITEBLOCKSIZE*2)%READBLOCKSIZE == 0)
            PROGRESS(PROGRESS_WRITEF, READBLOCKSIZE);
    }

    for (i = 0; i < 2; i++) {
        if ((r = ems_write(TO_ROM,
            offset + 0x100 + i*WRITEBLOCKSIZE,
            blockbuf100 + i*WRITEBLOCKSIZE, WRITEBLOCKSIZE)) < 0) {
                warnx("write error flashing %s", path);
                fclose(f);
                return FLASH_EUSB;
        }
    }

    PROGRESS(PROGRESS_WRITEF, READBLOCKSIZE);

    if (fclose(f) == EOF) {
        warn("can't close %s", path);
        return FLASH_EFILE;
    }
    return 0;
}

int
flash_move(ems_size_t offset, ems_size_t size, ems_size_t origoffset) {
    unsigned char blockbuf[(READBLOCKSIZE < WRITEBLOCKSIZE)?WRITEBLOCKSIZE:READBLOCKSIZE];
    unsigned char blockbuf100[WRITEBLOCKSIZE*2];
    ems_size_t remain, src, dest, blockofs, progress;
    int r, flipflop;

    src = origoffset;
    dest = offset;
    progress = 0;
    flipflop = 0;

    for (remain = size; remain > 0; remain -= READBLOCKSIZE) {
        if (CHECKINT) {
            warnx("operation interrupted");
            return FLASH_EINTR;
        }

        if ((r = ems_read(FROM_ROM, src, blockbuf, READBLOCKSIZE)) < 0) {
            warnx("read error updating flash memory");
            return FLASH_EUSB;
        }

        PROGRESS(PROGRESS_READ, READBLOCKSIZE);

        for (blockofs = 0; blockofs < READBLOCKSIZE; blockofs += WRITEBLOCKSIZE) {
            if (src == origoffset && blockofs == 0x100)  {
                memcpy(blockbuf100, blockbuf+0x100, WRITEBLOCKSIZE*2);
                blockofs += WRITEBLOCKSIZE;
                continue;
            }

            if ((flipflop = 1-flipflop) && CHECKINT) {
                warnx("operation interrupted");
                return FLASH_EINTR;
            }

            if ((r = ems_write(TO_ROM, flash_lastofs = dest+blockofs,
                blockbuf+blockofs, WRITEBLOCKSIZE)) < 0) {
                    warnx("write error updating flash memory");
                    return FLASH_EUSB;
            }

            if ((dest + blockofs)%ERASEBLOCKSIZE == 0)
                PROGRESS(PROGRESS_ERASE, 0);

            if ((progress += WRITEBLOCKSIZE)%READBLOCKSIZE == 0)
                PROGRESS(PROGRESS_WRITE, READBLOCKSIZE);
        }

        src += READBLOCKSIZE;
        dest += READBLOCKSIZE;
    }

    for (int i = 0; i < 2; i++) {
        if (ems_write(TO_ROM, offset+0x100+i*WRITEBLOCKSIZE,
            blockbuf100+i*WRITEBLOCKSIZE, WRITEBLOCKSIZE) < 0) {
                warnx("write error updating flash memory");
                return FLASH_EUSB;
        }
    }

    PROGRESS(PROGRESS_WRITE, READBLOCKSIZE);

    return flash_delete(origoffset, 2);
}

int
flash_read(int slotn, ems_size_t size, ems_size_t offset) {
    ems_size_t remain;
    unsigned char *block;
    int r;

    block = slot[slotn];

    for (remain = size; remain > 0; remain -= READBLOCKSIZE) {
        if (CHECKINT) {
            warnx("operation interrupted");
            return FLASH_EINTR;
        }

        if ((r = ems_read(FROM_ROM, offset, block, READBLOCKSIZE)) < 0) {
                warnx("read error updating flash memory");
                return FLASH_EUSB;
        }

        block += READBLOCKSIZE;
        offset += READBLOCKSIZE;

        PROGRESS(PROGRESS_READ, READBLOCKSIZE);
    }

    return 0;
}

/* doesn't test for signals */
int
flash_write(ems_size_t offset, ems_size_t size, int slotn) {
    ems_size_t blockofs, progress;
    unsigned char *buf;
    int r;

    buf = slot[slotn];
    progress = 0;
    for (blockofs = 0; blockofs < size; blockofs += WRITEBLOCKSIZE) {
        if (blockofs == 0x100)
            continue;

        if ((r = ems_write(TO_ROM, flash_lastofs = offset + blockofs,
            buf + blockofs, WRITEBLOCKSIZE)) < 0) {
                warnx("write error updating flash memory");
                return FLASH_EUSB;
        }

        if ((offset + blockofs) % ERASEBLOCKSIZE == 0)
            PROGRESS(PROGRESS_ERASE, 0);

        if ((progress += WRITEBLOCKSIZE) % READBLOCKSIZE == 0)
            PROGRESS(PROGRESS_WRITE, READBLOCKSIZE);
    }

    if ((r = ems_write(TO_ROM, offset + 0x100, buf + 0x100,
        WRITEBLOCKSIZE)) < 0) {
            warnx("write error updating flash memory");
            return FLASH_EUSB;
    }

    PROGRESS(PROGRESS_WRITE, READBLOCKSIZE);

    return 0;
}

int
flash_erase(ems_size_t offset) {
    unsigned char blankbuf[32];
    int i, r;

    if (CHECKINT)  {
        warnx("operation interrupted");
        return FLASH_EINTR;
    }

    for (i = 0; i < 2; i++) {
        memset(blankbuf, 0xff, 32);
        if ((r = ems_write(TO_ROM, flash_lastofs = offset + i*32, blankbuf,
            32)) < 0) {
                warnx("write error updating flash memory");
                return FLASH_EUSB;
        }
    }

    PROGRESS(PROGRESS_ERASE, 0);

    return 0;
}

int
flash_delete(ems_size_t offset, int blocks) {
    unsigned char zerobuf[32];
    int r;

    memset(zerobuf, 0, 32);

    while (blocks--) {
        if ((blocks+1)%2 == 0 && CHECKINT) {
            warnx("operation interrupted");
            return FLASH_EINTR;
        }

        r = ems_write(TO_ROM, offset + 0x130 - blocks*32,
            zerobuf, 32);
        if (r < 0) {
            warnx("flash write error (address=%"PRIuEMSSIZE")",
                    offset + 0x130 - blocks*32);
            return FLASH_EUSB;
        }
    }

    return 0;
}
