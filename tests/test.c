#define _XOPEN_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/types.h>

#include "test.h"

static int test_failed;
static int testran;

static void (*test_setup)(void);
static void (*test_teardown)(void);

void
test_init(int argc, char **argv, void (*setup)(void), void (*teardown)(void)) {
    if (argc > 1)
        test_onecasename = argv[1];
    test_setup = setup;
    test_teardown = teardown;
}

void
test_done(void) {
    printf("1..%d\n", testran);
    printf("Tests checked: %d, tests failed: %d\n", testran, test_failed);
    if (test_failed)
        printf("Some tests have failed\n");
}

void
test(char *title, void (*testfunc)(void), int expected_exit, int expected_sig) {
    fflush(0);
    switch (fork()) {
    case -1:
        perror("fork");
        abort();
    case 0:
        /* child */
        if (test_setup != NULL)
            test_setup();
        testfunc();
        if (test_teardown != NULL)
            test_teardown();
        fflush(0);
        _Exit(0);
    default: {
        /* parent */
        int status;

        testran++;
        if (wait(&status) == -1) {
            perror("internal error: wait");
            abort();
        }
        if (WIFSIGNALED(status)) {
            int sig = WTERMSIG(status);
            if (sig != expected_sig) {
                printf("Terminated with signal %d\n", sig);
                goto failed;
            }
        }
        if (WIFEXITED(status)) {
            int e = WEXITSTATUS(status);
            if (e != expected_exit) {
                printf("Exited with code %d but %d was expected\n", e, 
                    expected_exit);
                goto failed;
            }
        } else {
            printf("internal error: wait\n");
            goto failed;
        }
        break;
    }
    }

    printf("ok %d - %s\n", testran, title);
    return;

failed:
    test_failed++;
    printf("not ok %d - %s\n", testran, title);
    return;
}
