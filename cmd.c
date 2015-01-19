/* for tempnam(), popen() and pclose(): */
#define _XOPEN_SOURCE

/* for strsep(): */
#define _BSD_SOURCE

#include "ems.h"
#include "header.h"
#include "flash.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>

/* for open() and close(): */
#include <fcntl.h>
#include <unistd.h>

/* for stat() */
#include <sys/stat.h>

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

void
cmd_write(int page, int verbose, int argc, char **argv) {
    struct image image;
    ems_size_t base, freesize;
    FILE *p;
    char *tempfn;
    char command[1024];
    int r;

    // TODO: ignoring SIGPIPE (for popen)

    base = page * PAGESIZE;

    // Create a temporary file and ensure it was created by this process.
    // Then close it as it will be used by a subprocess.
    {
    int tempfd;

    tempfn = tempnam(NULL, NULL);
    if (tempfn == NULL)
        err(1, "tempnam");
    if ((tempfd = open(tempfn, O_CREAT | O_EXCL, 0600)) == -1)
        err(1, "error creating temporary file");
    close(tempfd);
    }

    // TODO: escape single quotes in tempfn
    r = snprintf(command, sizeof(command),
        AWK " -f "LIBEXECDIR"/insert.awk |" \
        AWK " -f "LIBEXECDIR"/update.awk > '%s'", tempfn);
    if (r < 0 || r >= sizeof(command))
        errx(1, "internal error: command too long");

    p = popen(command, "w");
    if (p == NULL)
        err(1, "error starting subprocess");

    freesize = PAGESIZE;
    list(page, &image);
    for (int i = 0; i < image.count; i++) {
        fprintf(p, "\t%"PRIuEMSSIZE"\t%"PRIuEMSSIZE"\n",
            image.romlist[i].offset,
            image.romlist[i].header.romsize);
        if (freesize < image.romlist[i].header.romsize)
            errx(1, "format error: sum of ROM sizes on flash exceeds the page size");
        freesize -= image.romlist[i].header.romsize;
    }

    // TODO: files should be sorted by size (descending order)
    for (int i = 0; i < argc; i++) {
        struct header header;
        unsigned char buf[HEADER_SIZE];
        FILE *f;

        f = fopen(argv[i], "rb");
        if (f == NULL)
            err(1, "can't open %s", argv[i]);
        if (fread(buf, HEADER_SIZE, 1, f) < 1) {
            if (ferror(f))
                err(1, "error reading %s", argv[i]);
            else
                errx(1, "invalid header for %s", argv[i]);
        }
        if (fclose(f) == EOF)
            err(1, "error closing %s", argv[i]);
        if (header_validate(buf) != 0)
            errx(1, "invalid header for %s", argv[i]);
        header_decode(&header, buf);

        // Check ROM size validity
        if (header.romsize == 0)
            errx(1, "invalid romsize code in header of %s", argv[i]);
        {
        struct stat buf;
        if (stat(argv[i], &buf) == -1)
            err(1, "can't stat %s", argv[i]);
        if (buf.st_size != header.romsize)
            errx(1, "ROM size declared in header of %s doesn't match file size",
                argv[i]);
        }
        if ((header.romsize & (header.romsize - 1)) != 0)
            errx(1, "size of %s is not a power of two", argv[i]);

        if (freesize < header.romsize)
            errx(1,"no space left on page");
        freesize -= header.romsize;
        fprintf(p, "%d\t\t%"PRIuEMSSIZE"\n", i, header.romsize);
    }

    if (ferror(p))
        errx(1, "error executing subprocess");

    r = pclose(p);
    if (r == -1)
        err(1, "error executing subprocess");
    else if (r != 0)
        errx(1, "error executing subprocess (exit code = %d)", r);

    {
    char line[1024], *linep;
    FILE *f;
    f = fopen(tempfn, "r");
    if (f == NULL)
        err(1, "can't open temporary file");

    while (fgets(line, sizeof(line), f) != NULL) {
        char *token;
        char *cmd;
        long dest, size, src;

        linep = line;

        cmd = strsep(&linep, "\t");
        if (cmd == NULL)
            errx(1, "internal error: bad update command (empty)");
        strsep(&linep, "\t"); // skip id
        dest = atol((token = strsep(&linep, "\t"))!=NULL?token:"");
        size = atol((token = strsep(&linep, "\t"))!=NULL?token:"");
        src = atol((token = strsep(&linep, "\t"))!=NULL?token:"");

        //printf("%s\t%ld\t%ld\t%ld\n", cmd, dest, size, src);

        if (strcmp(cmd, "writef") == 0) {
            r = flash_writef(base + dest, size, argv[src]);
        } else if (strcmp(cmd, "move") == 0) {
            r = flash_move(base + dest, size, base + src);
        } else if (strcmp(cmd, "read") == 0) {
            r = flash_read(dest, size, base + src);
        } else if (strcmp(cmd, "write") == 0) {
            r = flash_write(base + dest, size, src);
        } else if (strcmp(cmd, "erase") == 0) {
            r = flash_erase(base + dest);
        } else {
            errx(1, "internal error: bad update command (%s)", cmd);
        }
        if (r != 0)
            exit(1);
    }
    if (ferror(f))
        err(1, "error reading temporary file");

    if (fclose(f) == EOF)
        err(1, "can't close temporary file");
    }
}
