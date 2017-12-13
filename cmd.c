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

#include "config.h"
#include "ems.h"
#include "header.h"
#include "flash.h"
#include "image.h"
#include "insert.h"
#include "update.h"
#include "updates.h"
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

volatile sig_atomic_t int_state = 0, int_sig = 0;

static void
int_handler(int s) {
    static const char msg[] = "Termination signal received.\n"; 
    int_state = 1;
    int_sig = s;
    write(2, msg, strlen(msg));
}

int
checkint() {
    return int_state;
}

struct sigaction oldsa_hup, oldsa_int, oldsa_term;

void
catchint() {
    struct sigaction sa;

    int_state = 0;

    memset(&sa, 0, sizeof(sa));
    sigemptyset(&sa.sa_mask);
    sa.sa_handler = &int_handler;
    sa.sa_flags = SA_RESTART;

    sigaction(SIGHUP, NULL, &oldsa_hup);
    if (oldsa_hup.sa_handler != SIG_IGN)
        sigaction(SIGHUP, &sa, NULL);

    sigaction(SIGINT, NULL, &oldsa_int);
    if (oldsa_int.sa_handler != SIG_IGN)
        sigaction(SIGINT, &sa, NULL);

    sigaction(SIGTERM, NULL, &oldsa_term);
    if (oldsa_term.sa_handler != SIG_IGN)
        sigaction(SIGTERM, &sa, NULL);
}

void
restoreint()
{
    sigaction(SIGHUP, &oldsa_hup, NULL);
    sigaction(SIGINT, &oldsa_int, NULL);
    sigaction(SIGTERM, &oldsa_term, NULL);

    if (checkint())
        kill(0, int_sig);
}

void
blocksignals() {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sigemptyset(&sa.sa_mask);
    sa.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, &sa, NULL);
    sigaction(SIGTSTP, &sa, NULL);
    sigaction(SIGTTIN, &sa, NULL);
    sigaction(SIGTTOU, &sa, NULL);
}

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

    catchint();

    base = page * PAGESIZE;

    listing->count = 0;
    offset = 0;
    do {
        r = ems_read(FROM_ROM, base + offset, buf, HEADER_SIZE);
        if (r != HEADER_SIZE) {
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

    restoreint();

    return 0;
}

static char*
strenh(int enh) {
    enh &= HEADER_ENH_ALL;
    switch (enh) {
    case HEADER_ENH_GBC | HEADER_ENH_SGB:
        return "Color+Super";
    case HEADER_ENH_GBC:
        return "Color";
    case HEADER_ENH_SGB:
        return "SGB";
    default:
        return "None";
    }
}

void
cmd_title(int page) {
    struct listing listing;
    ems_size_t free;
    int menuenh, incompat_enh;

    blocksignals();

    printf("Bank  Title             Size     Enhancements\n");

    if (list(page, &listing))
        exit(1);

    if (listing.count > 0 && listing.romlist[0].offset == 0 &&
        strcmp(listing.romlist[0].header.title, MENUTITLE) == 0) {
            menuenh = listing.romlist[0].header.enhancements;
    } else {
            menuenh = -1;
    }

    incompat_enh = 0;
    free = PAGESIZE;
    for (int i = 0; i < listing.count; i++) {
        struct listing_rom *rl;

        rl = &listing.romlist[i];
        printf("%3d   %-*s  %4"PRIuEMSSIZE" KB  ",
            (int)((rl->offset) / BANKSIZE), HEADER_TITLE_SIZE, rl->header.title,
            rl->header.romsize >> 10);

        printf("%s", strenh(rl->header.enhancements));
        if (rl->header.gbc_only)
            printf(" (marked as for Game Boy Color only)");

        putchar('\n');

        if (menuenh != -1 && rl->header.enhancements != menuenh)
            incompat_enh |= rl->header.enhancements ^ menuenh;

        if (free < rl->header.romsize)
            errx(1, "format error: sum of ROM sizes on flash exceeds the page size");
        free -= rl->header.romsize;  
    }

    putchar('\n');
    printf("Page: %d\n", page+1);
    if (menuenh >= 0) {
        printf("Page enhancements: %s\n", strenh(menuenh));
        printf("Compatible consoles: ");
        if (!incompat_enh) {
            printf("All");
        } else {
            printf("Classic");
            if (incompat_enh != HEADER_ENH_ALL)
                printf("+%s", strenh(HEADER_ENH_NOT(incompat_enh)));
            printf(" (some ROMs have incompatible enh. settings)");
        }
        putchar('\n');
    } else {
        printf("Menu: no menu ROM found at bank 0\n");
    }

    printf("Free space: %4"PRIuEMSSIZE" KB\n", free >> 10);
}

void
cmd_delete(int page, int verbose, int argc, char **argv) {
    blocksignals();
    catchint();
    flash_init(NULL, checkint);

    for (int i = 0; i < argc; i++) {
        unsigned char rawheader[HEADER_SIZE];
        struct header header;
        char *arg;
        ems_size_t offset, base;
        int r, bank;
        long l;
        char *p;

        base = page * PAGESIZE;

        arg = argv[i];
        l = strtol(arg, &p, 10);
        if (p == arg || *p != '\0'  || l < 0 || l >= PAGESIZE/BANKSIZE) {
            warnx("--delete argument %d (%s): invalid bank number", i+1, arg);
            continue;
        }
        bank = l;
        offset = bank * BANKSIZE;

        r = ems_read(FROM_ROM, base + offset, rawheader, HEADER_SIZE);
        if (r != HEADER_SIZE) {
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
    restoreint();
}

void
cmd_format(int page, int verbose) {
    ems_size_t base, offset;

    blocksignals();
    catchint();
    flash_init(NULL, checkint);

    if (verbose)
        printf("Formating...\n");

    base = page * PAGESIZE;
    for (offset = 0; offset <= PAGESIZE - 32768; offset += 32768)
        if (flash_delete(base + offset, 1) != 0) {
            warnx("%s", flash_lasterrorstr);
            exit(1);
        }

    restoreint();
}

/*
 * --restore and --dump commands handling
 */

void
cmd_restore(int page, int verbose, char *path, int to) {
    struct progress_totals totals = {0};
    struct stat buf;
    ems_size_t base, size;

    if (to == TO_ROM) {
        base = page * PAGESIZE;
        size = PAGESIZE;
        totals.erase = PAGESIZE / ERASEBLOCKSIZE;
        totals.writef= size;
    } else {
        base = 0;
        size = SRAMSIZE;
        totals.writef= size;
    }

    if (stat(path, &buf) == -1)
        err(1, "can't stat %s", path);
    if (buf.st_size != size)
        errx(1, "file has an invalid size");

    progress_start(totals);

    blocksignals();
    catchint();
    flash_init(verbose?progress:NULL, checkint);
    if (flash_writef_to(to, base, size, path))
        errx(1, "%s", flash_lasterrorstr);
    restoreint();
}

void
cmd_dump(int page, int verbose, char *path, int from) {
    struct progress_totals totals = {0};
    ems_size_t base, size;

    if (from == FROM_ROM) {
        base = page * PAGESIZE;
        size = PAGESIZE;
        totals.read = PAGESIZE;
    } else {
        base = 0;
        size = SRAMSIZE;
        totals.read = SRAMSIZE;
    }
    progress_start(totals);
    blocksignals();
    catchint();
    flash_init(verbose?progress:NULL, checkint);
    if (flash_readf_from(from, path, size, base))
        errx(1, "%s", flash_lasterrorstr);
    restoreint();
}

/*
 * --write command handling
 */

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
    ems_size_t freesize;
    struct romfile *romfiles;
    struct romfile *menuromfile;

    blocksignals();

    if (argc == 0)
        return;

    image_init(&image);

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

    /* Abort if there is no valid menu on a non empty page */
    if (listing.count > 0 && (listing.romlist[0].offset != 0 ||
        strcmp(listing.romlist[0].header.title, MENUTITLE) != 0)) {
            errx(1, "error: no valid menu ROM found at bank 0");
    }

    /* 
     * Check compatibility of the enhancements required by the new ROMs
     * with those of the page unless --force has been specified
     */
    if (!force)
    {
        int enh_ign_mask, enh_page, enh_incompat;

        /* Determine the enhancements enabled by the page */
        if (listing.count > 0)
            /* non empty page: those of the first ROM in flash (the menu) */
            enh_page = listing.romlist[0].header.enhancements;
        else
            /*empty page:  those of the first ROM provided in arguments */
            enh_page = romfiles[0].header.enhancements;

        /*
         * Determine enhancements flags for which there is already a conflict
         * on the page.
         */
        enh_ign_mask = 0;
        for (int i = 0; i < listing.count; i++) {
            int enh_rom = listing.romlist[i].header.enhancements;
            if (enh_page != enh_rom)
                enh_ign_mask |= enh_page ^ enh_rom;
        }

        /*
         * Check compatibility of new ROMs ignoring enhancements in
         * the ignore mask
         */
        enh_incompat = 0;
        for (int i = 0; i < argc; i++) {
            int enh_rom = romfiles[i].header.enhancements;
            if ((enh_rom & HEADER_ENH_NOT(enh_ign_mask)) != (enh_page & HEADER_ENH_NOT(enh_ign_mask))) {
                enh_incompat |= (enh_rom & HEADER_ENH_NOT(enh_ign_mask)) ^ (enh_page & HEADER_ENH_NOT(enh_ign_mask));
                warnx(
                    "%s: incompatible enhancements:"
                    " this ROM requires a page with %s enh.",
                    romfiles[i].path,
                    strenh(enh_rom)
                );
            }
        }

        if (enh_incompat) {
            errx(1,
                "error: some ROMs have enhancements incompatible with this page."
                " Insert them on a compatible page"
                " or use --force if you don't use these consoles: %s. Page has"
                " the following enh.: %s",
                strenh(enh_incompat), strenh(enh_page)
            );
        }
    }

        /*
     * Add a menu if the page is empty and the the user doesn't want to insert
     * a 4 MB ROM.
     * The hardware enh. will be set according to the first ROM file.
     */
    if (listing.count == 0 && romfiles[0].header.romsize < PAGESIZE) {
        struct romfile *romf;
        char menupath[1024];
        char *menudir;

        if (verbose)
            printf("Loading the menu ROM...");

        romf = &romfiles[0];

        if ((menudir = getenv("MENUDIR")) == NULL)
            menudir = MENUDIR;

        strncpy(menupath, menudir, sizeof(menupath));
        switch (romf->header.enhancements & (HEADER_ENH_GBC | HEADER_ENH_SGB)) {
        case HEADER_ENH_GBC | HEADER_ENH_SGB:
            strncat(menupath, "/menucs.gb", sizeof(menupath) - strlen(menupath) - 1);
            break;
        case HEADER_ENH_GBC:
            strncat(menupath, "/menuc.gb", sizeof(menupath) - strlen(menupath) - 1);
            break;
        case HEADER_ENH_SGB:
            strncat(menupath, "/menus.gb", sizeof(menupath) - strlen(menupath) - 1);
            break;
        default:
            strncat(menupath, "/menu.gb", sizeof(menupath) - strlen(menupath) - 1);
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

    {
    struct updates *updates;

    image_update(&image, &updates);
    exit(apply_updates(page, verbose, updates));
    }
}

void
cmd_read(int page, int verbose, int argc, char **argv) {
    struct listing  listing;
    struct {struct listing_rom *rom; char *path;} *romfiles;
    ems_size_t totalread;
    char read_error;

    if (argc == 0)
	return;

    blocksignals();

    if (list(page, &listing))
	exit(1);

    if ((romfiles = malloc(argc * sizeof(*romfiles))) == NULL)
	err(1, "malloc");

    totalread = 0;
    read_error = 0;

    for (int i = 0; i < argc; i++) {
	ems_size_t      offset;
	char           *path, *p;
	int             bank;

	long l = strtol(argv[i], &p, 10);
	if (p == argv[i] || *p != ':')
	    errx(1, "invalid argument format (should be BANK:FILENAME)");

	if (l < 0 || l > PAGESIZE / BANKSIZE) {
	    errx(1, "bank must range between 0 and %d",
		  (int) (PAGESIZE / BANKSIZE - 1));
	}
	bank = l;

	offset = bank * BANKSIZE;
	path = p + 1;
	if (*path == '\0')
	    errx(1, "invalid filename");

	struct listing_rom *rom = NULL;
	for (int i = 0; i < listing.count; i++) {
	    if (listing.romlist[i].offset == offset) {
		rom = &listing.romlist[i];
		break;
	    }
	}
	if (rom == NULL)
	    errx(1, "No ROM found at bank %d", bank);

	if (rom->header.romsize == 0 ||
	    offset + rom->header.romsize > PAGESIZE) {
	    errx(1, "invalid ROM");
	}

        if (!read_error && totalread <= EMS_SIZE_MAX - rom->header.romsize)
            totalread += rom->header.romsize;
        else
            read_error = 1;

	romfiles[i].path = path;
        romfiles[i].rom = rom;
    }

    catchint();

    if (verbose && totalread > 0 && !read_error)
        progress_start((struct progress_totals){.read = totalread});

    for (int i = 0; i < argc; i++) {
        struct listing_rom *rom = romfiles[i].rom;
        char *path = romfiles[i].path;

	if (verbose) {
	    printf("Copying bank %d (%s) to %s\n", rom->offset/BANKSIZE,
                   rom->header.title, path);
	}

	flash_init(verbose ? progress : NULL, checkint);

	if (flash_readf_from(FROM_ROM, path, rom->header.romsize,
			     page * PAGESIZE + rom->offset))
	    errx(1, "%s", flash_lasterrorstr);
	if (verbose)
            putchar('\n');
    }

    restoreint();
}
