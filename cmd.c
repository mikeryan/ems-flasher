#include "ems.h"
#include "header.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>

struct rom {
    ems_size_t offset;
    struct header header;
};

struct image {
    int count;
    struct rom romlist[PAGESIZE/32768];
};

static void
list(int page, struct image *image) {
    struct header header;
    unsigned char buf[HEADER_SIZE];
    ems_size_t base, offset;
    int r;

    base = page * PAGESIZE;

    image->count = 0;
    offset = 0;
    do {
        r = ems_read(FROM_ROM, base + offset, buf, HEADER_SIZE);
        if (r < 0) {
            errx(1, "flash read error (address=%"PRIuEMSSIZE")",
                base + offset);
        }

        /* Skip if it is not a valid header or the romsize code is incorrect
           or the size is not a power of 2 or the ROM is not aligned or
           not in page boundaries */
        if (header_validate(buf) != 0) {
            offset += 32768;
            continue;
        }
        header_decode(&header, buf);
        if (header.romsize == 0 ||
            (header.romsize & (header.romsize - 1)) != 0 ||
            offset % header.romsize != 0 ||
            offset + header.romsize > PAGESIZE) {
                offset += 32768;
                continue;
        }

        image->romlist[image->count].offset = offset;
        image->romlist[image->count].header = header;
        image->count++;

        offset += header.romsize;
    } while (offset < PAGESIZE);
}

void
cmd_title(int page) {
        struct image image;
        ems_size_t base;

        base = page * PAGESIZE;

        printf("Bank  Title             Size     Compatibility\n");

        list(page, &image);

        for (int i = 0; i < image.count; i++) {
            struct rom *rl;

            rl = &image.romlist[i];
            printf("%3d   %s  %4"PRIuEMSSIZE" KB  ",
                (int)((base + rl->offset) / 16384), rl->header.title,
                rl->header.romsize >> 10);

            if (rl->header.gbc_only)
                printf("Color only");
            else {
                printf("Classic");
                if (rl->header.enhancements & HEADER_ENH_GBC)
                    printf(" + Color");
                if (rl->header.enhancements & HEADER_ENH_SGB)
                    printf(" + Super");
            }
            putchar('\n');
        }
}
