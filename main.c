#include <err.h>
#include <ctype.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ems.h"
#include "header.h"
#include "cmd.h"

// don't forget to bump this :P
#define VERSION "0.04"

const int limits[3] = {0, PAGESIZE, SRAMSIZE};

// operation mode
#define MODE_READ   1
#define MODE_WRITE  2
#define MODE_TITLE  3
#define MODE_DELETE 4
#define MODE_FORMAT 5
#define MODE_RESTORE 6
#define MODE_DUMP 7

/* options */
typedef struct _options_t {
    int verbose;
    int blocksize;
    int mode;
    char *file;
    int bank;
    int space;
    int rem_argc;
    char **rem_argv;
    int force;
} options_t;

// defaults
options_t opts = {
    .verbose            = 0,
    .blocksize          = 0,
    .mode               = 0,
    .file               = NULL,
    .bank               = 0,
    .space              = 0,
    .force              = 0,
};

// default blocksizes
#define BLOCKSIZE_READ  4096
#define BLOCKSIZE_WRITE 32

/**
 * Usage
 */
void usage(char *name) {
    printf("Usage: %s < --read | --write > [ --verbose ] <totally_legit_rom.gb> [<dontsteal.gb>]\n", name);
    printf("       %s --delete BANK [BANK]...\n", name);
    printf("       %s --format\n", name);
    printf("       %s --title\n", name);
    printf("       %s --version\n", name);
    printf("       %s --help\n", name);
    printf("Writes a ROM or SAV file to the EMS 64 Mbit USB flash cart\n\n");
    printf("Options:\n");
    printf("    --read                  read entire cart into file\n");
    printf("    --write                 write ROM file(s) to cart\n");
    printf("    --dump                  dump an entire page of SRAM or flash"
                                        "to a file\n");
    printf("    --restore               restore a dump taken by --dump\n");
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
            {"restore", 0, 0, 'e'},
            {"dump", 0, 0, 'u'},
            {"title", 0, 0, 't'},
            {"delete", 0, 0, 'd'},
            {"format", 0, 0, 'f'},
            {"blocksize", 1, 0, 's'},
            {"bank", 1, 0, 'b'},
            {"save", 0, 0, 'S'},
            {"rom", 0, 0, 'R'},
            {"force", 0, 0, 'F'},
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
            case 'e':
                if (opts.mode != 0) goto mode_error;
                opts.mode = MODE_RESTORE;
                break;
            case 'u':
                if (opts.mode != 0) goto mode_error;
                opts.mode = MODE_DUMP;
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
            case 'F':
                opts.force = 1;
                break;
            default:
                usage(argv[0]);
                break;
        }
    }

    // user didn't supply mode
    if (opts.mode == 0)
        goto mode_error;

    opts.rem_argc = argc - optind;
    if (optind < argc)
        opts.rem_argv = &argv[optind];

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
    } else if (opts.mode == MODE_WRITE || opts.mode == MODE_READ ||
               opts.mode == MODE_RESTORE || opts.mode == MODE_DUMP) {
        // user didn't give a filename
        if (optind >= argc) {
            printf("Error: you must provide an %s filename\n", opts.mode == MODE_READ ? "output" : "input");
            usage(argv[0]);
        }

        // extra argument: ROM file
        opts.file = argv[optind];

        // set a default blocksize if the user hasn't given one
        if (opts.blocksize == 0)
            opts.blocksize = (opts.mode == MODE_READ
                || opts.mode == MODE_DUMP)? BLOCKSIZE_READ : BLOCKSIZE_WRITE;
    }

    return;

mode_error:
    printf("Error: must supply exactly one of --read, --write, --dump, "
           "--restore, --delete, --format or --title\n");
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
    uint32_t base = opts.bank * PAGESIZE;
    if (opts.verbose)
        printf("base address is 0x%X\n", base);
    
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
    if (opts.mode == MODE_DUMP) {
        cmd_dump(opts.bank, opts.verbose, opts.file, space);
    } else if (opts.mode == MODE_RESTORE) {
        cmd_restore(opts.bank, opts.verbose, opts.file, space);
    } else if (opts.mode == MODE_WRITE) {
        cmd_write(opts.bank, opts.verbose, opts.force, opts.rem_argc, opts.rem_argv);
    } else if (opts.mode == MODE_DELETE) {
        cmd_delete(opts.bank, opts.verbose, opts.rem_argc, opts.rem_argv);
    } else if (opts.mode == MODE_FORMAT) {
        cmd_format(opts.bank, opts.verbose);
    }
    // read the ROM header
    else if (opts.mode == MODE_TITLE) {
        cmd_title(opts.bank);
    }

    // should never reach here
    else
        errx(1, "Unknown mode %d, file a bug report", opts.mode);

    return 0;
}
