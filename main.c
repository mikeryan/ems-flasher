#include <err.h>
#include <ctype.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>

#include "ems.h"

// don't forget to bump this :P
#define VERSION "0.02"

// one bank is 32 megabits
#define BANK_SIZE 0x400000

// operation mode
#define MODE_READ   1
#define MODE_WRITE  2
#define MODE_TITLE  3

/* options */
typedef struct _options_t {
    int verbose;
    int blocksize;
    int mode;
    char *file;
} options_t;

// defaults
options_t opts = {
    .verbose            = 0,
    .blocksize          = 0,
    .mode               = 0,
    .file               = NULL,
};

// default blocksizes
#define BLOCKSIZE_READ  4096
#define BLOCKSIZE_WRITE 32

/**
 * Usage
 */
void usage(char *name) {
    printf("Usage: %s < --read | --write > [ --verbose ] <totally_legit_rom.gb>\n", name);
    printf("       %s --title\n", name);
    printf("       %s --version\n", name);
    printf("       %s --help\n", name);
    printf("Writes a ROM file to the first bank of an EMS 64 Mbit USB flash cart\n\n");
    printf("Options:\n");
    printf("    --read                  read entire first bank into file\n");
    printf("    --write                 write ROM file to first bank\n");
    printf("    --title                 title of the ROM in bank 1\n");
    printf("    --verbose               displays more information\n");
    printf("\n");
    printf("You MUST supply exactly one of --read, --write, or --title\n");
    printf("\n");
    printf("Advanced options:\n");
    printf("    --blocksize <size>      bytes per block (default: 4096 read, 32 write)\n");
    printf("\n");
    printf("Written by Mike Ryan <mikeryan@lacklustre.net>\n");
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
            {"blocksize", 1, 0, 's'},
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
            case 's':
                optval = atoi(optarg);
                if (optval <= 0) {
                    printf("Error: block size must be > 0\n");
                    usage(argv[0]);
                }
                // TODO make sure it divides evenly into bank size
                opts.blocksize = optval;
                break;
            default:
                usage(argv[0]);
                break;
        }
    }

    // user didn't supply mode
    if (opts.mode == 0)
        goto mode_error;

    if (opts.mode == MODE_WRITE || opts.mode == MODE_READ) {
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
    printf("Error: must supply exactly one of --read, --write, or --title\n");
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
    unsigned char *buf = malloc(blocksize);
    if (buf == NULL)
        err(1, "malloc");

    // read the ROM from bank 1 and save it into the file
    if (opts.mode == MODE_READ) {
        FILE *save_file = fopen(opts.file, "w");
        if (save_file == NULL)
            err(1, "Can't open %s for writing", opts.file);

        if (opts.verbose)
            printf("saving ROM into %s\n", opts.file);

        while (offset < BANK_SIZE) {
            r = ems_read(offset, buf, blocksize);
            if (r < 0) {
                warnx("can't read %d bytes at offset %u\n", blocksize, offset);
                return 1;
            }

            r = fwrite(buf, blocksize, 1, save_file);
            if (r != 1)
                err(1, "can't write %d bytes into ROM file at offset %u", blocksize, offset);

            offset += blocksize;
        }

        fclose(save_file);

        if (opts.verbose)
            printf("Successfully wrote %u bytes into %s\n", offset, opts.file);
    }

    // write ROM in the file to bank 1
    else if (opts.mode == MODE_WRITE) {
        FILE *write_file = fopen(opts.file, "r");
        if (write_file == NULL)
            err(1, "Can't open ROM file %s", opts.file);

        printf("Writing ROM file %s\n", opts.file);

        while (fread(buf, blocksize, 1, write_file) == 1) {
            r = ems_write(offset, buf, blocksize);
            if (r < 0) {
                warnx("can't write %d bytes at offset %u", blocksize, offset);
                return 1;
            }

            offset += blocksize;
        }

        fclose(write_file);

        if (opts.verbose)
            printf("Successfully wrote %u from %s\n", offset, opts.file);
    }

    // read the title from the ROM
    else if (opts.mode == MODE_TITLE) {
        char buf[17];

        r = ems_read(0x134, (unsigned char *)buf, 16);
        if (r < 0)
            errx(1, "can't read 16 bytes at offset 0x134\n");

        // dirty little trick, doesn't work right on games with codes in the title
        buf[16] = 0;
        puts(buf);
    }

    // should never reach here
    else
        errx(1, "Unknown mode %d, file a bug report", opts.mode);

    // belt and suspenders
    free(buf);

    return 0;
}
