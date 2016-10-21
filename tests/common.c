#define _XOPEN_SOURCE 500
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <err.h>

#include "test.h"

/*
 * Common functions
 */

void *
emalloc(size_t size) {
    void *m;
    if ((m = malloc(size)) == NULL) {
        warn("malloc");
        abort();
    }
    return m;
}

char *
ecreatetmpf(int size) {
    char fname[] = ".tmp_XXXXXX";
    char *retfn;

    if (mkstemp(fname) == -1) {
        warn("mkstemp");
        abort();
    }

    if ((retfn = strdup(fname)) == NULL) {
        warn("strdup");
        abort();
    }

    if (size != 0)
        if (truncate(fname, size)) {
            warnx("truncate: %s", fname);
            abort();
        }

    return retfn;
}

void
eremove(char *fn) {
    if (remove(fn)) {
        warnx("remove: %s", fn); 
        abort();
    }
}

