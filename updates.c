#define _XOPEN_SOURCE 500

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <time.h>

#include <err.h>
#include <unistd.h>
#include <sys/stat.h>

#include "header.h"
#include "cmd.h"
#include "update.h"
#include "flash.h"
#include "progress.h"

/**
 * Update the flash memory
 *
 * Compute updates with image_update() and execute each commands
 * sequentialy.
 * In case of a non-USB error, recover the small ROMs saved by the read
 * command.
 *
 * Exit on error
 */
int
apply_updates(int page, int verbose, struct updates *updates) {
    struct update *u;
    ems_size_t base;
    int indefrag, r;

    base = page * PAGESIZE;

    progress_start(updates);

    catchint();
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
        return 1;
    }
    return 0;
}
