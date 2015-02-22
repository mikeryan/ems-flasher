#include "ems.h"

#include <stdio.h>
#include <string.h>
#include <err.h>

#define WRITEBLOCKSIZE 32
#define READBLOCKSIZE 4096

#define NBSLOTS 3
static unsigned char slot[NBSLOTS][ERASEBLOCKSIZE/2];

int
flash_writef(ems_size_t offset, ems_size_t size, char *path,
  void (*progress)(ems_size_t)) {
    unsigned char blockbuf[WRITEBLOCKSIZE];
    FILE *f;
    ems_size_t remain;
    int r;

    if ((f = fopen(path, "rb")) == NULL) {
        warn("can't open %s", path);
        return 1;
    }

    for (remain = size; remain > 0; remain -= WRITEBLOCKSIZE) {
        if (fread(blockbuf, 1, WRITEBLOCKSIZE, f) < WRITEBLOCKSIZE) {
            if (ferror(f)) {
                warn("error reading %s", path);
                fclose(f);
                return 1;
            } else break;
        }

        if ((r = ems_write(TO_ROM, offset, blockbuf,
            WRITEBLOCKSIZE)) < 0) {
                warnx("write error flashing %s", path);
                fclose(f);
                return 1;
        }
        offset += WRITEBLOCKSIZE;
        if ((remain-WRITEBLOCKSIZE)%READBLOCKSIZE == 0)
            progress(READBLOCKSIZE);
    }

    if (fclose(f) == EOF) {
        warn("can't close %s", path);
        return 1;
    }
    return 0;
}

int
flash_move(ems_size_t offset, ems_size_t size, ems_size_t origoffset,
  void (*progress)(ems_size_t)) {
    unsigned char blockbuf[(READBLOCKSIZE < WRITEBLOCKSIZE)?WRITEBLOCKSIZE:READBLOCKSIZE];
    ems_size_t remain, remain_w, src, dest;
    int r;

    src = origoffset;
    dest = offset;

    for (remain = size; remain > 0; remain -= READBLOCKSIZE) {
        if ((r = ems_read(FROM_ROM, src, blockbuf, READBLOCKSIZE)) < 0) {
            warnx("read error updating flash memory");
            return 1;
        }

        progress(READBLOCKSIZE);

        for (remain_w = READBLOCKSIZE; remain_w > 0; remain_w -= WRITEBLOCKSIZE) {
            if ((r = ems_write(TO_ROM, dest, blockbuf,
                WRITEBLOCKSIZE)) < 0) {
                    warnx("write error updating flash memory");
                    return 1;
            }
        }

        progress(READBLOCKSIZE);

        src += READBLOCKSIZE;
        dest += READBLOCKSIZE;
    }

    {
        unsigned char zerobuf[32];
        memset(zerobuf, 0, 32);
        if (ems_write(TO_ROM, origoffset + 0x110, zerobuf, 32) < 0) {
            warnx("write error updating flash memory");
            return 1;
        }
        if (ems_write(TO_ROM, origoffset + 0x130, zerobuf, 32) < 0) {
            warnx("write error updating flash memory");
            return 1;
        }
    }
    return 0;
}

int
flash_read(int slotn, ems_size_t size, ems_size_t offset,
  void (*progress)(ems_size_t)) {
    ems_size_t remain;
    unsigned char *block;
    int r;

    block = slot[slotn];

    for (remain = size; remain > 0; remain -= READBLOCKSIZE) {
        if ((r = ems_read(FROM_ROM, offset, block, READBLOCKSIZE)) < 0) {
                warnx("read error updating flash memory");
                return 1;
        }

        block += READBLOCKSIZE;
        offset += READBLOCKSIZE;

        progress(READBLOCKSIZE);
    }

    return 0;
}

int
flash_write(ems_size_t offset, ems_size_t size, int slotn,
  void (*progress)(ems_size_t)) {
    ems_size_t dest, remain;
    unsigned char *buf;
    int r;

    buf = slot[slotn];

    dest = offset;
    for (remain = size; remain > 0 ; remain -= WRITEBLOCKSIZE) {
        if ((r = ems_write(TO_ROM, dest, buf, WRITEBLOCKSIZE)) < 0) {
            warnx("write error updating flash memory");
            return 1;
        }

        buf += WRITEBLOCKSIZE;
        dest += WRITEBLOCKSIZE;
        if ((remain - WRITEBLOCKSIZE) % READBLOCKSIZE == 0)
            progress(READBLOCKSIZE);
    }

    return 0;
}

int
flash_erase(ems_size_t offset, void (*progress)(ems_size_t)) {
    unsigned char blankbuf[32];
    int i, r;

    for (i = 0; i < 2; i++) {
        memset(blankbuf, 0xff, 32);
        if ((r = ems_write(TO_ROM, offset + i*32, blankbuf, 32)) < 0) {
                warnx("write error updating flash memory");
                return 1;
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
        r = ems_write(TO_ROM, offset + 0x130 - blocks*32, zerobuf, 32);
        if (r < 0) {
            warnx("flash write error (address=%"PRIuEMSSIZE")",
                    offset + 0x130 - blocks*32);
            return 1;
        }
    }

    return 0;
}
