#define _XOPEN_SOURCE 500 /* for usleep() */
    
#include <stdio.h>
#include <unistd.h>
#include <err.h>
#include <limits.h>
#include <time.h>
#include <sys/time.h>
#include "ems.h"
#include "progress.h"
#include "flash.h"

#define PROGRESS_NBSTEPS 8

struct {
    ems_size_t remain, total;
    long steps[PROGRESS_NBSTEPS];
    int stepscount, stepscur;
} progress_type[PROGRESS_TYPESNB];

static struct timeval prectime;

static int crprinted; // indicate if \r was just printed

/**
 * Start a new line if progression status was just printed (terminated by
 * a carriage return (\r))
 */
void
progress_newline(void) {
    if (crprinted) {
        putchar('\n');
        crprinted = 0;
    }
}

/**
 * Initialize progression
 *   - get the current time
 *   - compute the total number of bytes transferred for each transfer type.
 *     (see flash.c, under "Progress status").
 */
void
progress_start(struct updates *updates) {
    struct update *u;

    crprinted = 0;

    updates_foreach(updates, u) {
        switch (u->cmd) {
        case UPDATE_CMD_WRITEF:
            if (u->update_writef_dstofs%ERASEBLOCKSIZE == 0)
                progress_type[PROGRESS_ERASE].total += (u->update_writef_size +
                    ERASEBLOCKSIZE-1)/ERASEBLOCKSIZE;
            progress_type[PROGRESS_WRITEF].total += u->update_writef_size;
            break;
        case UPDATE_CMD_MOVE:
            if (u->update_move_dstofs%ERASEBLOCKSIZE == 0)
                progress_type[PROGRESS_ERASE].total += (u->update_move_size +
                     ERASEBLOCKSIZE-1)/ERASEBLOCKSIZE;
            progress_type[PROGRESS_WRITE].total += u->update_move_size;
            progress_type[PROGRESS_READ].total += u->update_move_size;
            break;
        case UPDATE_CMD_WRITE:
            if (u->update_write_dstofs%ERASEBLOCKSIZE == 0)
                progress_type[PROGRESS_ERASE].total += (u->update_write_size +
                    ERASEBLOCKSIZE-1)/ERASEBLOCKSIZE;
            progress_type[PROGRESS_WRITE].total += u->update_write_size;
            break;
        case UPDATE_CMD_READ:
            progress_type[PROGRESS_READ].total += u->update_read_size;
            break;
        case UPDATE_CMD_ERASE:
            progress_type[PROGRESS_ERASE].total++;
            break;
        }
    }

    for (int i = 0; i < PROGRESS_TYPESNB; i++) {
        progress_type[i].remain = progress_type[i].total;
        progress_type[i].stepscount = 0;
        progress_type[i].stepscur = 0;
    }

    if (gettimeofday(&prectime, NULL) == -1) {
        progress_newline();
        warn("gettimeofday");
        prectime.tv_sec = 0;
    }
}

/**
 * Callback called regularly by the transfer functions in flash.c.
 * Parameters:
 *   type:
 *     A transfer type: ERASE, WRITEF, READ, WRITE. This information is useful
 *     to estimate at best the transfer rate as it can differ from one type to
 *     another. REFRESH simply display the last status.
 *   bytes:
 *      The size of the chunk transfered.
 *
 * The estimated remaining time is computed from the number of bytes left to be
 * transfered and the measured rate for each type of transfer. The estimation is
 * made from the PROGRESS_NBSTEPS last samples.
 *
 * Important: currently, this function expects to be called with a chunk size of
 * 4 KB (READBLOCKSIZE), expected for ERASE and REFRESH.
 */
void
progress(int type, ems_size_t bytes) {
    ems_size_t progressbytes, progresstotal;
    struct timeval curtime;
    double remtime, dt;
    long diff;

    if (type == PROGRESS_REFRESH || prectime.tv_sec == 0)
        goto refresh;

    if (type == PROGRESS_ERASE) {
        progress_type[type].remain--;
    } else {
        if (bytes != READBLOCKSIZE) {
            progress_newline();
            warnx("internal error: progress: bytes != READBLOCKSIZE");
        }
        progress_type[type].remain -= bytes;
    }

#if 0
    // simulate operation time.
    {
    long delay;

    switch (type) {
    case PROGRESS_ERASE:
        delay = 1000;
        break;
    case PROGRESS_WRITEF:
        delay = bytes*1000/17500;
        break;
    case PROGRESS_WRITE:
        delay = bytes*1000/17500;
        break;
    case PROGRESS_READ:
        delay = bytes*1000/43500;
        break;
    }
    usleep(delay*1000);
    }
#endif

    if (gettimeofday(&curtime, NULL) == -1) {
        progress_newline();
        warn("gettimeofday");
        prectime.tv_sec = 0;
        goto refresh;
    }

    dt = difftime(curtime.tv_sec, prectime.tv_sec);
    if (dt >= 0  &&
        dt <= (LONG_MAX-999)/1000) {
            diff = dt*1000 +
                   ((long)curtime.tv_usec - (long)prectime.tv_usec)/1000;
    } else {
        diff = 0;
    }

    if (diff > 0) {
        progress_type[type].steps[progress_type[type].stepscur] = diff;
        if (progress_type[type].stepscur == progress_type[type].stepscount)
            progress_type[type].stepscount++;
        progress_type[type].stepscur = (progress_type[type].stepscur+1)%PROGRESS_NBSTEPS;
    }
    prectime = curtime;

refresh:
    remtime = progresstotal = progressbytes = 0;
    for (int i = 0; i < PROGRESS_TYPESNB; i++) {
        double rate;
        long time;

        if (i != PROGRESS_ERASE) {
            progresstotal += progress_type[i].total;
            progressbytes += progress_type[i].total - progress_type[i].remain;
        }

        time = 0;
        for (int j = 0; j < progress_type[i].stepscount; j++)
            time += progress_type[i].steps[j];

        if (progress_type[i].stepscount == 0 || time == 0) {
            rate = 0;
        } else {
            if (i != PROGRESS_ERASE)
                rate = (double)READBLOCKSIZE*progress_type[i].stepscount*1000/time;
            else
                rate = (double)time/progress_type[i].stepscount/1000;
        }

        // Use default when the rate is not yet known
        if (rate == 0) {
            switch (i) {
            case PROGRESS_ERASE:
                rate = 1;
                break;
            case PROGRESS_WRITEF:
                rate = 17500;
                break;
            case PROGRESS_WRITE:
                rate = 17500;
                break;
            case PROGRESS_READ:
                rate = 43500;
                break;
            }
        }

        remtime += progress_type[i].remain/rate;
    }

    printf(" %3"PRIuEMSSIZE"%%", progressbytes*100/progresstotal);
    if (prectime.tv_sec != 0)
        printf(" %02d:%02d", (int)(remtime+0.99)/60, (int)(remtime+0.99)%60);
    putchar('\r');
    crprinted = 1;
    fflush(stdout);
}
