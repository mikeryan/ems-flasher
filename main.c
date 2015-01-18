#include <err.h>
#include <ctype.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ems.h"
#include "header.h"

// don't forget to bump this :P
#define VERSION "0.04"

// one bank is 32 megabits
#define BANK_SIZE 0x400000
#define SRAM_SIZE 0x020000

const int limits[3] = {0, BANK_SIZE, SRAM_SIZE};

// operation mode
#define MODE_READ   1
#define MODE_WRITE  2
#define MODE_TITLE  3
#define MODE_DELETE 4
#define MODE_FORMAT 5

/* options */
typedef struct _options_t {
    int verbose;
    int blocksize;
    int mode;
    char *file;
    int bank;
    int space;
} options_t;

// defaults
options_t opts = {
    .verbose            = 0,
    .blocksize          = 0,
    .mode               = 0,
    .file               = NULL,
    .bank               = 0,
    .space              = 0,
};

// default blocksizes
#define BLOCKSIZE_READ  4096
#define BLOCKSIZE_WRITE 32

/**
 * Usage
 */
void usage(char *name) {
    printf("Usage: %s < --read | --write > [ --verbose ] <totally_legit_rom.gb>\n", name);
    printf("       %s --delete BANK [BANK]...\n", name);
    printf("       %s --format\n", name);
    printf("       %s --title\n", name);
    printf("       %s --version\n", name);
    printf("       %s --help\n", name);
    printf("Writes a ROM or SAV file to the EMS 64 Mbit USB flash cart\n\n");
    printf("Options:\n");
    printf("    --read                  read entire cart into file\n");
    printf("    --write                 write ROM file to cart\n");
    printf("    --delete                delete ROMs with the specified bank numbers\n");
    printf("    --format                delete all ROMs of the specified page\n");
    printf("    --title                 title of the ROM in both banks\n");
    printf("    --verbose               displays more information\n");
    printf("    --bank <num>            select cart bank (1 or 2)\n");
    printf("    --save                  force write to SRAM\n");
    printf("    --rom                   force write to Flash ROM\n");
    printf("\n");
    printf("You MUST supply exactly one of --read, --write, or --title\n");
    printf("Reading or writing with a file ending in .sav will write to SRAM.\n");
    printf("To select between ROM and SRAM, use ONE of the --save / --rom options.\n");
    printf("\n");
    printf("Advanced options:\n");
    printf("    --blocksize <size>      bytes per block (default: 4096 read, 32 write)\n");
    printf("\n");
    printf("Written by Mike Ryan <mikeryan@lacklustre.net> and others\n");
    printf("See web site for more info:\n");
    printf("    http://lacklustre.net/gb/ems/\n");
    exit(1);
}

/**
 * Get the options to the binary. Options are stored in the global "opts".
 */
void get_options(int argc, char **argv) {
    int c, optval;

    while (1) {
        int option_index = 0;
        static struct option long_options[] = {
            {"help", 0, 0, 'h'},
            {"version", 0, 0, 'V'},
            {"verbose", 0, 0, 'v'},
            {"read", 0, 0, 'r'},
            {"write", 0, 0, 'w'},
            {"title", 0, 0, 't'},
            {"delete", 0, 0, 'd'},
            {"format", 0, 0, 'f'},
            {"blocksize", 1, 0, 's'},
            {"bank", 1, 0, 'b'},
            {"save", 0, 0, 'S'},
            {"rom", 0, 0, 'R'},
            {0, 0, 0, 0}
        };

        c = getopt_long(
            argc, argv, "hVvs:rwt",
            long_options, &option_index
        );
        if (c == -1)
            break;

        switch (c) {
            case 'h':
                usage(argv[0]);
                break;
            case 'V':
                printf("EMS-flasher %s\n", VERSION);
                exit(0);
                break;
            case 'v':
                opts.verbose = 1;
                break;
            case 'r':
                if (opts.mode != 0) goto mode_error;
                opts.mode = MODE_READ;
                break;
            case 'w':
                if (opts.mode != 0) goto mode_error;
                opts.mode = MODE_WRITE;
                break;
            case 't':
                if (opts.mode != 0) goto mode_error;
                opts.mode = MODE_TITLE;
                break;
            case 'd':
                if (opts.mode != 0) goto mode_error;
                opts.mode = MODE_DELETE;
                break;
            case 'f':
                if (opts.mode != 0) goto mode_error;
                opts.mode = MODE_FORMAT;
                break;
            case 's':
                optval = atoi(optarg);
                if (optval <= 0) {
                    printf("Error: block size must be > 0\n");
                    usage(argv[0]);
                }
                // TODO make sure it divides evenly into bank size
                opts.blocksize = optval;
                break;
            case 'b':
                optval = atoi(optarg);
                if (optval < 1 || optval > 2) {
                    printf("Error: cart only has two banks 1 and 2\n");
                    usage(argv[0]);
                }
                opts.bank = optval - 1;
                break;
            case 'S':
                if (opts.space != 0) goto mode_error2;
                opts.space = FROM_SRAM;
                break;
            case 'R':
                if (opts.space != 0) goto mode_error2;
                opts.space = FROM_ROM;
                break;
            default:
                usage(argv[0]);
                break;
        }
    }

    // user didn't supply mode
    if (opts.mode == 0)
        goto mode_error;

    if (opts.mode == MODE_FORMAT || opts.mode == MODE_TITLE) {
        if (optind < argc) {
            printf("Error: no argument expected\n");
            usage(argv[0]);
        }
    } else if (opts.mode == MODE_DELETE) {
        if (optind >= argc) {
            printf("Error: you must provide bank numbers\n");
            usage(argv[0]);
        }
    } else if (opts.mode == MODE_WRITE || opts.mode == MODE_READ) {
        // user didn't give a filename
        if (optind >= argc) {
            printf("Error: you must provide an %s filename\n", opts.mode == MODE_READ ? "output" : "input");
            usage(argv[0]);
        }

        // extra argument: ROM file
        opts.file = argv[optind];

        // set a default blocksize if the user hasn't given one
        if (opts.blocksize == 0)
            opts.blocksize = opts.mode == MODE_READ ? BLOCKSIZE_READ : BLOCKSIZE_WRITE;
    }

    return;

mode_error:
    printf("Error: must supply exactly one of --read, --write, --delete, "
        "--format or --title\n");
    usage(argv[0]);

mode_error2:
    printf("Error: must supply zero or one of --sram, or --rom\n");
    usage(argv[0]);
}

/**
 * Main
 */
int main(int argc, char **argv) {
    int r;

    get_options(argc, argv);

    if (opts.verbose)
        printf("trying to find EMS cart\n");

    r = ems_init();
    if (r < 0)
        return 1;

    if (opts.verbose)
        printf("claimed EMS cart\n");

    // we'll need a buffer one way or another
    int blocksize = opts.blocksize;
    uint32_t offset = 0;
    uint32_t base = opts.bank * BANK_SIZE;
    if (opts.verbose)
        printf("base address is 0x%X\n", base);
    
    unsigned char *buf = malloc(blocksize);
    if (buf == NULL)
        err(1, "malloc");

    // determine what we're reading/writing from/to
    int space = opts.space;
    if (space == 0 && opts.file != NULL) {
        //attempt to autodetect the file
        //are the last four characters .sav ?
        size_t namelen = strlen(opts.file);

        if (opts.file[namelen - 4] == '.' &&
            tolower(opts.file[namelen - 3]) == 's' &&
            tolower(opts.file[namelen - 2]) == 'a' &&
            tolower(opts.file[namelen - 1]) == 'v') {
            space = FROM_SRAM;
        } else {
            space = FROM_ROM;
        }
    }

    // read the ROM and save it into the file
    if (opts.mode == MODE_READ) {
        FILE *save_file = fopen(opts.file, "w");
        if (save_file == NULL)
            err(1, "Can't open %s for writing", opts.file);

        if (opts.verbose && space == FROM_ROM)
            printf("Saving ROM into %s\n", opts.file);
        else if (opts.verbose)
            printf("Saving SAVE into %s\n", opts.file);

        while ((offset + blocksize) <= limits[space]) {
            r = ems_read(space, offset + base, buf, blocksize);
            if (r < 0) {
                warnx("can't read %d bytes at offset %u\n", blocksize, offset);
                return 1;
            }

            r = fwrite(buf, blocksize, 1, save_file);
            if (r != 1)
                err(1, "can't write %d bytes into file at offset %u", blocksize, offset);

            offset += blocksize;
        }

        fclose(save_file);

        if (opts.verbose)
            printf("Successfully wrote %u bytes into %s\n", offset, opts.file);
    }

    // write ROM in the file to bank 1
    else if (opts.mode == MODE_WRITE) {
        FILE *write_file = fopen(opts.file, "r");
        if (write_file == NULL) {
            if (space == TO_ROM)
                err(1, "Can't open ROM file %s", opts.file);
            else
                err(1, "Can't open SAVE file %s", opts.file);
        }

        if (opts.verbose && space == TO_ROM)
            printf("Writing ROM file %s\n", opts.file);
        else if (opts.verbose)
            printf("Writing SAVE file %s\n", opts.file);

        while ((offset + blocksize) <= limits[space] && fread(buf, blocksize, 1, write_file) == 1) {
            r = ems_write(space, offset + base, buf, blocksize);
            if (r < 0) {
                warnx("can't write %d bytes at offset %u", blocksize, offset);
                return 1;
            }

            offset += blocksize;
        }

        fclose(write_file);

        if (opts.verbose)
            printf("Successfully wrote %u from %s\n", offset, opts.file);
    } else if (opts.mode == MODE_DELETE) {
        for (int i = optind; i < argc; i++) {
            unsigned char rawheader[HEADER_SIZE];
            unsigned char zerobuf[32];
            char *arg;
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
    } else if (opts.mode == MODE_FORMAT) {
        unsigned char zerobuf[32];

        memset(zerobuf, 0, 32);
        base = opts.bank * PAGESIZE;
        for (offset = 0; offset <= PAGESIZE - 32768; offset += 32768) {
            r = ems_write(TO_ROM, base + offset + 0x130, zerobuf, 32);
            if (r < 0) {
                errx(1, "flash write error (address=%"PRIuEMSSIZE")",
                        base + offset + 0x130);
            }
        }
    }
    // read the ROM header
    else if (opts.mode == MODE_TITLE) {
        unsigned char buf[512];
        struct header header;

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

    // should never reach here
    else
        errx(1, "Unknown mode %d, file a bug report", opts.mode);

    // belt and suspenders
    free(buf);

    return 0;
}
