#include "ems.h"
#include "flash.h"

#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <err.h>

#define WRITEBLOCKSIZE 32
#define READBLOCKSIZE 4096

#define NBSLOTS 3
static unsigned char slot[NBSLOTS][ERASEBLOCKSIZE/2];

static void (*flash_progress_cb)(ems_size_t);
static int (*flash_checkint_cb)(void);

#define CHECKINT (flash_checkint_cb?flash_checkint_cb():0)
#define PROGRESS(size)                                                         \
    do {                                                                       \
        if (flash_progress_cb) flash_progress_cb(size);                        \
    } while (0)

void
flash_init(void (*progress_cb)(ems_size_t), int (*checkint_cb)(void)) {
    flash_lastofs = -1;
    flash_progress_cb = progress_cb;
    flash_checkint_cb = checkint_cb;
}

int
flash_writef(ems_size_t offset, ems_size_t size, char *path) {
    unsigned char blockbuf[WRITEBLOCKSIZE];
    FILE *f;
    ems_size_t remain;
    int r;

    if ((f = fopen(path, "rb")) == NULL) {
        warn("can't open %s", path);
        return FLASH_EFILE;
    }

    for (remain = size; remain > 0; remain -= WRITEBLOCKSIZE) {
        if (fread(blockbuf, 1, WRITEBLOCKSIZE, f) < WRITEBLOCKSIZE) {
            if (ferror(f)) {
                warn("error reading %s", path);
                fclose(f);
                return FLASH_EFILE;
            } else break;
        }

        if (CHECKINT && (remain/WRITEBLOCKSIZE)%2 == 0)
            return FLASH_EINTR;

        if ((r = ems_write(TO_ROM, flash_lastofs = offset, blockbuf,
            WRITEBLOCKSIZE)) < 0) {
                warnx("write error flashing %s", path);
                fclose(f);
                return 1;
        }
        offset += WRITEBLOCKSIZE;
        if ((remain-WRITEBLOCKSIZE)%READBLOCKSIZE == 0)
            PROGRESS(READBLOCKSIZE);
    }

    if (fclose(f) == EOF) {
        warn("can't close %s", path);
        return FLASH_EFILE;
    }
    return 0;
}

int
flash_move(ems_size_t offset, ems_size_t size, ems_size_t origoffset) {
    unsigned char blockbuf[(READBLOCKSIZE < WRITEBLOCKSIZE)?WRITEBLOCKSIZE:READBLOCKSIZE];
    ems_size_t remain, remain_w, src, dest;
    int r;

    src = origoffset;
    dest = offset;

    for (remain = size; remain > 0; remain -= READBLOCKSIZE) {
        if (CHECKINT)
            return FLASH_EINTR;

        if ((r = ems_read(FROM_ROM, src, blockbuf, READBLOCKSIZE)) < 0) {
            warnx("read error updating flash memory");
            return FLASH_EUSB;
        }

        PROGRESS(READBLOCKSIZE);

        for (remain_w = READBLOCKSIZE; remain_w > 0; remain_w -= WRITEBLOCKSIZE) {
            if (CHECKINT && (remain_w/WRITEBLOCKSIZE)%2 == 0)
                return FLASH_EINTR;

            if ((r = ems_write(TO_ROM, flash_lastofs = dest, blockbuf,
                WRITEBLOCKSIZE)) < 0) {
                    warnx("write error updating flash memory");
                    return FLASH_EUSB;
            }
        }

        PROGRESS(READBLOCKSIZE);

        src += READBLOCKSIZE;
        dest += READBLOCKSIZE;
    }

    {
        unsigned char zerobuf[32];
        memset(zerobuf, 0, 32);
        if (ems_write(TO_ROM, origoffset + 0x110, zerobuf,
            32) < 0) {
                warnx("write error updating flash memory");
                return FLASH_EUSB;
        }
        if (ems_write(TO_ROM, origoffset + 0x130, zerobuf,
            32) < 0) {
                warnx("write error updating flash memory");
                return FLASH_EUSB;
        }
    }
    return 0;
}

int
flash_read(int slotn, ems_size_t size, ems_size_t offset) {
    ems_size_t remain;
    unsigned char *block;
    int r;

    block = slot[slotn];

    for (remain = size; remain > 0; remain -= READBLOCKSIZE) {
        if (CHECKINT)
            return FLASH_EINTR;

        if ((r = ems_read(FROM_ROM, offset, block, READBLOCKSIZE)) < 0) {
                warnx("read error updating flash memory");
                return FLASH_EUSB;
        }

        block += READBLOCKSIZE;
        offset += READBLOCKSIZE;

        PROGRESS(READBLOCKSIZE);
    }

    return 0;
}

/* doesn't test for signals */
int
flash_write(ems_size_t offset, ems_size_t size, int slotn) {
    ems_size_t dest, remain;
    unsigned char *buf;
    int r;

    buf = slot[slotn];

    dest = offset;
    for (remain = size; remain > 0 ; remain -= WRITEBLOCKSIZE) {
        if ((r = ems_write(TO_ROM, flash_lastofs = dest, buf,
            WRITEBLOCKSIZE)) < 0) {
                warnx("write error updating flash memory");
                return FLASH_EUSB;
        }

        buf += WRITEBLOCKSIZE;
        dest += WRITEBLOCKSIZE;
        if ((remain - WRITEBLOCKSIZE) % READBLOCKSIZE == 0)
            PROGRESS(READBLOCKSIZE);
    }

    return 0;
}

int
flash_erase(ems_size_t offset) {
    unsigned char blankbuf[32];
    int i, r;

    if (CHECKINT)
        return FLASH_EINTR;

    for (i = 0; i < 2; i++) {
        memset(blankbuf, 0xff, 32);
        if ((r = ems_write(TO_ROM, flash_lastofs = offset + i*32, blankbuf,
            32)) < 0) {
                warnx("write error updating flash memory");
                return FLASH_EUSB;
        }
    }

    return 0;
}

int
flash_delete(ems_size_t offset, int blocks) {
    unsigned char zerobuf[32];
    int r;

    memset(zerobuf, 0, 32);

    while (blocks--) {
        if (CHECKINT && (blocks+1)%2 == 0)
            return FLASH_EINTR;
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
