#include "ems.h"
#include "header.h"

#include <stdio.h>
#include <err.h>

void
cmd_title(int page) {
    unsigned char buf[512];
    struct header header;
    ems_size_t offset, base;
    int r;

    base = page * PAGESIZE;

    printf("Bank  Title             Size     Compatibility\n");
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

        printf("%3d   %s  %4"PRIuEMSSIZE" KB  ",
            (int)((base + offset) / 16384), header.title,
            header.romsize >> 10);

        if (header.gbc_only)
            printf("Color only");
        else {
            printf("Classic");
            if (header.enhancements & HEADER_ENH_GBC)
                printf(" + Color");
            if (header.enhancements & HEADER_ENH_SGB)
                printf(" + Super");
        }
        putchar('\n');

        offset += header.romsize;
    } while (offset < PAGESIZE);
}
