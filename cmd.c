/*
 * Notes:
 * - cmd_xxx() functions doesn't have to free resources they use as the
 *   program exits directly on return. To quit the program with an exit code
 *   other than 0, simply use exit() or err()...
 * - error messages are written by the functions where the errors occured, so
 *   when a function return an error, you can exit directly.
 */

/* for sigaction() and SA_RESTART */
#define _XOPEN_SOURCE 500

#include "ems.h"
#include "header.h"
#include "flash.h"
#include "image.h"
#include "insert.h"
#include "update.h"
#include "cmd.h"
#include "progress.h"

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <err.h>

#include <unistd.h>
#include <sys/stat.h>

#define MENUTITLE "MENU#"

struct listing_rom {
    ems_size_t offset;
    struct header header;
};

struct listing {
    int count;
    struct listing_rom romlist[PAGESIZE/32768];
};

/**
 * Create a listing of the ROM.
 * The listing is guaranteed to represent a valid image:
 *   - no ROM overlapping
 *   - size is a power of two
 *   - ROMs are aligned to their size
 * ROMs that doesn't meet these conditions are discarded.
 */
static int
list(int page, struct listing *listing) {
    struct header header;
    unsigned char buf[HEADER_SIZE];
    ems_size_t base, offset;
    int r;

    base = page * PAGESIZE;

    listing->count = 0;
    offset = 0;
    do {
        r = ems_read(FROM_ROM, base + offset, buf, HEADER_SIZE);
        if (r < 0) {
            warnx("flash read error (address=%"PRIuEMSSIZE")",
                base + offset);
            return 1;
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

        listing->romlist[listing->count].offset = offset;
        listing->romlist[listing->count].header = header;
        listing->count++;

        offset += header.romsize;
    } while (offset < PAGESIZE);

    return 0;
}

static void
printenhancements(int enh) {
    if (enh == 0) {
        printf("None");
    } else {
        if (enh & HEADER_ENH_GBC)
            printf("Color");
        if (enh & HEADER_ENH_SGB)
            printf("%sSuper", enh != HEADER_ENH_SGB?" + ":"");
    }
}

void
cmd_title(int page) {
    struct listing listing;
    ems_size_t free;
    int menuenh, compat;

    printf("Bank  Title             Size     Enhancements\n");

    if (list(page, &listing))
        exit(1);

    if (listing.count > 0 && listing.romlist[0].offset == 0 &&
        strcmp(listing.romlist[0].header.title, MENUTITLE) == 0) {
            menuenh = listing.romlist[0].header.enhancements;
    } else {
            menuenh = -1;
    }

    compat = 1; // Assume ROMs have same enh. settings than the menu
    free = PAGESIZE;
    for (int i = 0; i < listing.count; i++) {
        struct listing_rom *rl;

        rl = &listing.romlist[i];
        printf("%3d   %-*s  %4"PRIuEMSSIZE" KB  ",
            (int)((rl->offset) / 16384), HEADER_TITLE_SIZE, rl->header.title,
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
        struct header header;
        char *arg;
        ems_size_t offset, base;
        int r, bank;

        base = page * PAGESIZE;

        arg = argv[i];
        bank = atoi(arg); //TODO: proper bank number validation
        offset = bank * 16384;

        r = ems_read(FROM_ROM, base + offset, rawheader, HEADER_SIZE);
        if (r < 0) {
            errx(1, "flash read error (address=%"PRIuEMSSIZE")",
                offset);
        }

        if (header_validate(rawheader) != 0) {
            warnx("no ROM or invalid header at bank %d", bank);
            continue;
        }

        header_decode(&header, rawheader);

        if (verbose)
            printf("Deleting ROM at bank %d: %s...\n", bank, header.title);

        if (flash_delete(base + offset, 2) != 0) {
            warnx("%s", flash_lasterrorstr);
            exit(1);
        }
    }
}

void
cmd_format(int page, int verbose) {
    ems_size_t base, offset;

    if (verbose)
        printf("Formating...\n");

    base = page * PAGESIZE;
    for (offset = 0; offset <= PAGESIZE - 32768; offset += 32768)
        if (flash_delete(base + offset, 1) != 0) {
            warnx("%s", flash_lasterrorstr);
            exit(1);
        }
}

/*
 * --write command handling
 */

volatile sig_atomic_t int_state = 0;

static void
int_handler(int s) {
    static const char msg[] = "Termination signal received.\n"; 
    int_state = 1;
    write(2, msg, strlen(msg));
}

static int
checkint() {
    return int_state;
}

struct romfile {
    struct header header;
    char *path;
    time_t ctime;
};

int
romfiles_compar_size_desc(const void *pa, const void *pb) {
    ems_size_t a = ((struct romfile *)pa)->header.romsize;
    ems_size_t b = ((struct romfile *)pb)->header.romsize;
    if (b < a)
        return -1;
    if (b > a)
        return 1;
    return 0;
}

/**
 * Validate a ROM file:
 *   - the header must be valid
 *   - the size declared in the header must be a power of two and match the
 *     file size
 *
 *  Set a struct romfile with the header, the path and the ctime of the file
 */
static int
validate_romfile(char *path, struct romfile *romfile) {
    struct header header;
    unsigned char buf[HEADER_SIZE];
    time_t ctime;
    FILE *f;

    if ((f = fopen(path, "rb")) == NULL) {
        warn("can't open %s", path);
        return 1;
    }
    if (fread(buf, HEADER_SIZE, 1, f) < 1) {
        if (ferror(f)) {
            warn("error reading %s", path);
            return 1;
        } else {
            warnx("invalid header for %s", path);
            fclose(f);
            return 1;
        }
    }
    if (fclose(f) == EOF) {
        warn("error closing %s", path);
        return 1;
    }
    if (header_validate(buf) != 0) {
        warnx("invalid header for %s", path);
        return 1;
    }
    header_decode(&header, buf);

    // Check ROM size validity
    if (header.romsize == 0){
        warnx("invalid romsize code in header of %s", path);
        return 1;
    }
    {
    struct stat buf;
    if (stat(path, &buf) == -1) {
        warn("can't stat %s", path);
        return 1;
    }

    ctime = buf.st_ctime;

    if (buf.st_size != header.romsize) {
        warnx("ROM size declared in header of %s doesn't match file size",
            path);
        return 1;
    }
    }
    if ((header.romsize & (header.romsize - 1)) != 0) {
        warnx("size of %s is not a power of two", path);
        return 1;
    }

    romfile->header = header;
    romfile->path = path;
    romfile->ctime = ctime;

    return 0;
}

void
cmd_write(int page, int verbose, int force, int argc, char **argv) {
    struct listing listing;
    struct image image;
    ems_size_t base, freesize;
    struct romfile *romfiles;
    struct romfile *menuromfile;
    int r;

    if (argc == 0)
        return;

    image_init(&image);

    base = page * PAGESIZE;
    menuromfile = romfiles = NULL;
    freesize = PAGESIZE;

    if ((romfiles = malloc(argc*sizeof(*romfiles))) == NULL)
        err(1, "malloc");

    for (int i = 0; i < argc; i++)
        if (validate_romfile(argv[i], &romfiles[i]))
            exit(1);

    if (list(page, &listing))
        exit(1);

    /*
     * If present, remove the menu if:
     *   - there is no other ROM in the page
     *   - the user wants to insert a 4MB flash (taking the entire page)
     *   - or the hardware enh. doesn't match those of the first ROM file.
     *
     * Note: the menu is not deleted from the flash right now but it will be
     *       overwritten later.
     */
    if (listing.count == 1 &&
        listing.romlist[0].offset == 0 &&
        strcmp(listing.romlist[0].header.title, MENUTITLE) == 0 &&
        listing.romlist[0].header.romsize == 32768) {
            if (romfiles[0].header.romsize == PAGESIZE ||
                romfiles[0].header.enhancements != listing.romlist[0].header.enhancements) {
                    listing.count--;
            }
    }

    /*
     * Add a menu if the page is empty and the the user doesn't want to insert
     * a 4 MB ROM.
     * The hardware enh. will be set according to the first ROM file.
     */
    if (listing.count == 0 && romfiles[0].header.romsize < PAGESIZE) {
        struct romfile *romf;
        char *menupath;

        if (verbose)
            printf("Loading the menu ROM...");

        romf = &romfiles[0];

        switch (romf->header.enhancements & (HEADER_ENH_GBC | HEADER_ENH_SGB)) {
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

        if ((menuromfile = malloc(sizeof(*menuromfile))) == NULL)
            err(1, "malloc");

        if (validate_romfile(menupath, menuromfile))
            exit(1);

        if (strcmp(menuromfile->header.title, MENUTITLE) != 0 ||
            menuromfile->header.romsize != 32768) {
                errx(1, "%s [%s] doesn't seem to be a menu ROM", menupath,
                    menuromfile->header.title);
        }
    }

    /*
     * Create an image of the page with existing ROMs in flash.
     * (with the exception of the menu if it was removed in a previous step)
     */
    for (int i = 0; i < listing.count; i++) {
        struct listing_rom *lsrom;
        struct rom *rom;

        lsrom = &listing.romlist[i];

        if ((rom = malloc(sizeof(*rom))) == NULL)
            err(1, "malloc");
        rom->source.type = ROM_SOURCE_FLASH;
        rom->romsize = lsrom->header.romsize;
        rom->offset = rom->source.u.origoffset = lsrom->offset;
        rom->header = lsrom->header;

        image_insert_tail(&image, rom);

        if (freesize < rom->romsize)
            errx(1, "format error: sum of ROM sizes on flash exceeds the page size");
        freesize -= rom->romsize;
    }

    // Insert the menu in the image (it means that the image is empty)
    if (menuromfile) {
        struct rom *rom;

        if ((rom = malloc(sizeof(*rom))) == NULL)
            err(1, "malloc");
        rom->source.type = ROM_SOURCE_FILE;
        rom->romsize = 32768;
        rom->offset = 0;
        rom->source.u.fileinfo = menuromfile;
        rom->header = menuromfile->header;

        image_insert_tail(&image, rom);
        freesize -= 32768;
    }

    /*
     * Insert ROM files ordered by size in descending order in the image to
     * limit the fragmentation.
     *
     * Ensure that there is no duplicate title.
     */

    qsort(romfiles, argc, sizeof(*romfiles), romfiles_compar_size_desc);

    for (int i = 0; i < argc; i++) {
        struct romfile *romf;
        struct rom *rom;

        romf = &romfiles[i];

        // Check for duplicate titles
        image_foreach(&image, rom) {
            if (strcmp(romf->header.title, rom->header.title) == 0) {
                if (rom->source.type == ROM_SOURCE_FLASH)
                    errx(1, "%s: duplicate title with a ROM on cartridge: %s",
                        romf->path, romf->header.title);
                else errx(1, "%s: duplicate title with %s: %s", romf->path,
                    ((struct romfile*)rom->source.u.fileinfo)->path,
                    romf->header.title);
            }
        }

        if ((rom = malloc(sizeof(*rom))) == NULL)
            err(1, "malloc");
        rom->source.type = ROM_SOURCE_FILE;
        rom->romsize = romf->header.romsize;
        rom->source.u.fileinfo = romf;
        rom->header = romf->header;

        image_insert_defrag(&image, rom);
        if (freesize < romf->header.romsize)
            errx(1,"no space left on page");
        freesize -= romf->header.romsize;
    }

    /*
     * Update the flash memory
     *
     * Compute updates with image_update() and execute each commands
     * sequentialy.
     * In case of a non-USB error, recover the small ROMs saved by the read
     * command.
     */
    {
    struct updates *updates;
    struct update *u;
    struct sigaction sa, oldsa;
    int indefrag;

    image_update(&image, &updates);

    int_state = 0;

    memset(&sa, 0, sizeof(sa));
    sigemptyset(&sa.sa_mask);
    sa.sa_handler = &int_handler;
    sa.sa_flags = SA_RESTART;

    sigaction(SIGHUP, NULL, &oldsa);
    if (oldsa.sa_handler != SIG_IGN)
        sigaction(SIGHUP, &sa, NULL);

    sigaction(SIGINT, NULL, &oldsa);
    if (oldsa.sa_handler != SIG_IGN)
        sigaction(SIGINT, &sa, NULL);

    sigaction(SIGTERM, NULL, &oldsa);
    if (oldsa.sa_handler != SIG_IGN)
        sigaction(SIGTERM, &sa, NULL);

    progress_start(updates);

    flash_init(verbose?progress:NULL, checkint);

    indefrag = 0;
    updates_foreach(updates, u) {
        if (verbose) {
            if (u->cmd == UPDATE_CMD_WRITEF) {
                progress_newline();
                printf("Writing %s [%s]...\n",
                    ((struct romfile*)u->update_writef_fileinfo)->path,
                    u->rom->header.title);
                progress(PROGRESS_REFRESH, 0);
                indefrag = 0;
            } else if (!indefrag) {
                progress_newline();
                printf("Defragmenting...\n");
                progress(PROGRESS_REFRESH, 0);
                indefrag = 1;
            }
        }

        r = 0;
        switch (u->cmd) {
        case UPDATE_CMD_WRITEF: {
            struct romfile *romfile;
            struct stat buf;

            // Check if the file has changed this the last time.
            // Note: use ctime and it is not reliable with all OS or filesystems.
            romfile = u->update_writef_fileinfo;
            if (stat(romfile->path, &buf) == -1) {
                progress_newline();
                warn("can't stat %s", romfile->path);
                r = FLASH_EFILE;
                break;
            }
            if (difftime(buf.st_ctime, romfile->ctime) != 0) {
                progress_newline();
                warnx("%s has changed", romfile->path);
                r = FLASH_EFILE;
                break;
            }

            r = flash_writef(base + u->update_writef_dstofs,
                u->update_writef_size, romfile->path);
            break;
        }
        case UPDATE_CMD_MOVE:
            r = flash_move(base + u->update_move_dstofs,
                u->update_move_size, base + u->update_move_srcofs);
            break;
        case UPDATE_CMD_READ:
            r = flash_read(u->update_read_dstslot, u->update_read_size,
                    base + u->update_read_srcofs);
            break;
        case UPDATE_CMD_WRITE:
            r = flash_write(base + u->update_write_dstofs,
                u->update_write_size, u->update_write_srcslot);
            break;
        case UPDATE_CMD_ERASE:
            r = flash_erase(base + u->update_erase_dstofs);
            break;
        default:
            progress_newline();
            errx(1, "internal error: bad update command (%d)", u->cmd);
        }

        if (r) {
            progress_newline();
            warnx("%s", flash_lasterrorstr);
            break;
        }
    }

    progress_newline();
    flash_setprogresscb(NULL);

    // Error recovery
    if (r) {
        struct update *err_update = u; // Command that caused the error

        // For each write command in the erase-block in which the error occured
        // and only if this erase-block was formated:
        for (; u != NULL; u = updates_next(u)) {
            if (u->cmd != UPDATE_CMD_WRITE)
                continue;

            if (flash_lastofs/ERASEBLOCKSIZE != u->update_write_dstofs/ERASEBLOCKSIZE)
                break;

            // Recover only on non-USB error and don't re-execute write if it is
            // this command that caused the error.
            if (r != FLASH_EUSB && u != err_update) {
                int err;

                if (verbose)
                    printf("Recovering %s\n", u->rom->header.title);

                err = flash_write(base + u->update_write_dstofs,
                    u->update_write_size, u->update_write_srcslot);
                if (err) {
                    warnx("%s", flash_lasterrorstr);
                    r = err;
                    err_update = u;
                }           
            }

            // Otherwise, display the title of lost ROMs.
            // We can't be sure that the erase-block was formated if the error
            // occured while writing to offset 0 of the erase-block.
            if (r == FLASH_EUSB || u == err_update) {
                warnx("%slost %s\n",
                    flash_lastofs%ERASEBLOCKSIZE == 0?"possibly ":"",
                    u->rom->header.title);
            }
        }
        exit(1);
    }
    }
}
