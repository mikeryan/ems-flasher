/* for tempnam() */
#define _XOPEN_SOURCE

/* for strsep(): */
#define _BSD_SOURCE

#include "ems.h"
#include "header.h"
#include "flash.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <err.h>

/* for open() and close(): */
#include <fcntl.h>
#include <unistd.h>

#include <sys/stat.h>
#include <sys/wait.h>

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

static void
printenhancements(int enh) {
    if (enh == 0) {
        printf("None");
    } else {
        if (enh & HEADER_ENH_GBC)
            printf("Color");
        if (enh & HEADER_ENH_SGB)
            printf(" + Super");
    }
}

void
cmd_title(int page) {
    struct image image;
    ems_size_t base, free;
    int menuenh, compat;

    base = page * PAGESIZE;

    printf("Bank  Title             Size     Enhancements\n");

    list(page, &image);

    if (image.count > 0 && image.romlist[0].offset == 0 &&
        strcmp(image.romlist[0].header.title, "GB16M           ") == 0) {
            menuenh = image.romlist[0].header.enhancements;
    } else {
            menuenh = -1;
    }

    compat = 1; // Assume ROMs have same enh. settings than the menu
    free = PAGESIZE;
    for (int i = 0; i < image.count; i++) {
        struct rom *rl;

        rl = &image.romlist[i];
        printf("%3d   %s  %4"PRIuEMSSIZE" KB  ",
            (int)((base + rl->offset) / 16384), rl->header.title,
            rl->header.romsize >> 10);

        printenhancements(rl->header.enhancements);
        if (rl->header.gbc_only)
            printf(" (marked as for Game Boy Color only)");

        putchar('\n');

        if (rl->header.enhancements != menuenh)
            compat = 0;

        if (free < rl->header.romsize)
            errx(1, "format error: sum of ROM sizes on flash exceeds the page size");
        free -= rl->header.romsize;  
    }

    putchar('\n');
    printf("Page: %d\n", page+1);
    if (menuenh >= 0) {
            printf("Page enhancements: ");
            printenhancements(menuenh);
            if (!compat)
                printf(" (some ROMs have incompatible settings)");
            putchar('\n');
    } else {
        printf("Menu: no menu ROM found at bank 0\n");
    }

    printf("Free space: %4"PRIuEMSSIZE" KB\n", free >> 10);
}

void
cmd_delete(int page, int verbose, int argc, char **argv) {
    for (int i = 0; i < argc; i++) {
        unsigned char rawheader[HEADER_SIZE];
        unsigned char zerobuf[32];
        char *arg;
        ems_size_t offset;
        int r, bank;

        memset(zerobuf, 0, 32);
        arg = argv[i];
        bank = atoi(arg); //TODO: proper bank number validation
        offset = bank * 16384;

        r = ems_read(FROM_ROM, offset, rawheader, HEADER_SIZE);
        if (r < 0) {
            errx(1, "flash read error (address=%"PRIuEMSSIZE")",
                offset);
        }

        if (header_validate(rawheader) != 0) {
            warnx("no ROM or invalid header at bank %d", bank);
            continue;
        }

        r = ems_write(TO_ROM, offset + 0x110, zerobuf, 32);
        if (r < 0) {
            errx(1, "flash write error (address=%"PRIuEMSSIZE")",
                    offset + 0x110);
        }
        r = ems_write(TO_ROM, offset + 0x130, zerobuf, 32);
        if (r < 0) {
            errx(1, "flash write error (address=%"PRIuEMSSIZE")",
                    offset + 0x130);
        }
    }
}

void
cmd_format(int page, int verbose) {
    unsigned char zerobuf[32];
    ems_size_t base, offset;
    int r;

    memset(zerobuf, 0, 32);
    base = page * PAGESIZE;
    for (offset = 0; offset <= PAGESIZE - 32768; offset += 32768) {
        r = ems_write(TO_ROM, base + offset + 0x130, zerobuf, 32);
        if (r < 0) {
            errx(1, "flash write error (address=%"PRIuEMSSIZE")",
                    base + offset + 0x130);
        }
    }
}

static ems_size_t progresstotal, progressbytes;

static void
progress(ems_size_t bytes) {
    progressbytes += bytes;
    printf("\r %"PRIuEMSSIZE"%%", progressbytes*100/progresstotal);
    fflush(stdout);
    if (progressbytes == progresstotal)
        putchar('\n');
}

void
cmd_write(int page, int verbose, int argc, char **argv) {
    struct image image;
    ems_size_t base, freesize;
    char *tempfn, *menupath;
    int pipe_in[2], pipe_out[2];
    FILE *p;
    pid_t pid;
    int tempfd;
    int r;

    base = page * PAGESIZE;

    /* 
     * Determine the update operations to add the new files to the image and put
     * them in a temporary file.
     *
     * Plumbing:
     * image + new files entries -> insert.awk -> update.awk -> update commands
     * See insert.awk and update.awk for information about the in/out formats
     */

    if (pipe(pipe_in) == -1)
        err(1, "pipe");

    if (pipe(pipe_out) == -1)
        err(1, "pipe");

    if ((pid = fork()) == -1)
        err(1, "fork");

    if (pid == 0) {
        if (close(pipe_in[1]) == -1)
            err(1, "close");
        if (close(pipe_out[0]) == -1)
            err(1, "close");
        if (dup2(pipe_in[0], 0) == -1)
            err(1, "dup2");
        if (dup2(pipe_out[1], 1) == -1)
            err(1, "dup2");
        execl(AWK, "insert.awk", "-f", LIBEXECDIR"/insert.awk", (char*)NULL);
        warn("insert.awk");
        _exit(1);
    }

    if (close(pipe_in[0]) == -1)
        err(1, "close");
    if (close(pipe_out[1]) == -1)
        err(1, "close");

    if ((tempfn = tempnam(NULL, NULL)) == NULL)
        err(1, "tempnam");
    if ((tempfd = open(tempfn, O_CREAT | O_EXCL | O_RDWR, 0600)) == -1)
        err(1, "error creating temporary file");
    if (unlink(tempfn) == -1)
        err(1, "unlink");

    if ((pid = fork()) == -1)
        err(1, "fork");

    if (pid == 0) {
        if (close(pipe_in[1]) == -1)
            err(1, "close");
        if (dup2(pipe_out[0], 0) == -1)
            err(1, "dup2");
        if (dup2(tempfd, 1) == -1)
            err(1, "dup2");
        execl(AWK, "update.awk", "-f", LIBEXECDIR"/update.awk", (char*)NULL);
        warn("update.awk");
        _exit(1);
    }

    if (close(pipe_out[0]) == -1)
        err(1, "close");

    if ((p = fdopen(pipe_in[1], "w")) == NULL)
        err(1, "fdopen");

    freesize = PAGESIZE;
    list(page, &image);

    // Delete the menu if it is the only ROM in the page
    if (image.count == 1 &&
        image.romlist[0].offset == 0 &&
        strcmp(image.romlist[0].header.title, "GB16M           ") == 0) {
            unsigned char zerobuf[32];

            memset(zerobuf, 0, 32);
            r = ems_write(TO_ROM, base + 0x110, zerobuf, 32);
            if (r < 0) {
                errx(1, "flash write error (address=%"PRIuEMSSIZE")",
                        base + 0x110);
            }
            r = ems_write(TO_ROM, base + 0x130, zerobuf, 32);
            if (r < 0) {
                errx(1, "flash write error (address=%"PRIuEMSSIZE")",
                        base + 0x130);
            }
            image.count--;
    }

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

        // For the first new ROM: add a menu if the page is empty excepted if
        // the ROM is 4MB.
        if (i == 0 && image.count == 0 && header.romsize < PAGESIZE) {
            fprintf(p, "%d\t\t32768\n", argc);

            switch (header.enhancements & (HEADER_ENH_GBC | HEADER_ENH_SGB)) {
            case HEADER_ENH_GBC | HEADER_ENH_SGB:
                menupath = MENUDIR"/menucs.gb";
                break;
            case HEADER_ENH_GBC:
                menupath = MENUDIR"/menuc.gb";
                break;
            case HEADER_ENH_SGB:
                menupath = MENUDIR"/menus.gb";
                break;
            default:
                menupath = MENUDIR"/menu.gb";
            }

            freesize -= 32768;
            if (access(menupath, R_OK) != 0)
                err(1, "can't access the menu image (%s)", menupath);
        }

        if (freesize < header.romsize)
            errx(1,"no space left on page");
        freesize -= header.romsize;

        fprintf(p, "%d\t\t%"PRIuEMSSIZE"\n", i, header.romsize);
    }

    if (ferror(p))
        errx(1, "error executing subprocess");

    if (fclose(p) == -1)
        err(1, "fclose");

    {
    int status;
    while (wait(&status) != -1);
    if (errno != ECHILD)
        err(1, "wait");   
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
        errx(1, "error executing subprocess");
    }

    /*
     * Update the flash card
     *
     * The update operations are read twice from the temp file created by the
     * previous step.
     *
     * First pass:
     *   Compute the total bytes read and written by read/write/move commands.
     *
     * Second pass:
     *   Flash the page. The flashing functions regularly call a callback
     *   function with progression information.
     */
    {
    char line[1024], *linep;
    FILE *f;

    f = fdopen(tempfd, "r");
    if (f == NULL)
        err(1, "can't open temporary file");

    for (int pass = 1; pass <= 2; pass++) {

    if (pass == 1)
        progresstotal = 0;
    if (pass == 2) {
        progressbytes = 0;
        progress(0);
    }

    rewind(f);
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

        // printf("%s\t%ld\t%ld\t%ld\n", cmd, dest, size, src);

        r = 0;
        if (strcmp(cmd, "writef") == 0) {
            char *path;

            if (pass == 1) {
                progresstotal += size;
            } else {
                if (src < argc)
                    path = argv[src];
                else
                    path = menupath;
                r = flash_writef(base + dest, size, path, &progress);
            }
        } else if (strcmp(cmd, "move") == 0) {
            if (pass == 1)
                progresstotal += size*2;
            else
                r = flash_move(base + dest, size, base + src, &progress);
        } else if (strcmp(cmd, "read") == 0) {
            if (pass == 1)
                progresstotal += size;
            else
                r = flash_read(dest, size, base + src, &progress);
        } else if (strcmp(cmd, "write") == 0) {
            if (pass == 1)
                progresstotal += size;
            else
                r = flash_write(base + dest, size, src, &progress);
        } else if (strcmp(cmd, "erase") == 0) {
            if (pass == 2)
                r = flash_erase(base + dest, &progress);
        } else {
            errx(1, "internal error: bad update command (%s)", cmd);
        }
        if (r != 0)
            exit(1);
    }
    if (ferror(f))
        err(1, "error reading temporary file");
    }

    if (fclose(f) == EOF)
        err(1, "can't close temporary file");
    }
}
