/*
 * Test case 4: test the calls to the progression callback
 *
 * The progress callback records the number of bytes accumulated by each kind
 * of operation (the number of calls for erase).
 *
 * For each test, the tester must specify the expected value for each kind
 * of operation with TC4_EXPECT(type, val) (0 by default).
 */

#define _XOPEN_SOURCE 500
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <err.h>

#include "test.h"
#include "test-flash.h"
#include "../ems.h"
#include "../flash.h"
#include "../progress.h"

#define KB 1024
#define MB (KB*KB)

#define TC4_EXPECT(type, val) expect[PROGRESS_##type] = (val)

ems_size_t stats[PROGRESS_TYPESNB];
ems_size_t expect[PROGRESS_TYPESNB];

int
ems_write(int from, uint32_t offset, unsigned char *buf, size_t count) {
    return count;
}

int
ems_read(int from, uint32_t offset, unsigned char *buf, size_t count) {
    return count;
}

static void
t_progress(int type, ems_size_t bytes) {
    TEST_ASSERT(type >= 0 && type < PROGRESS_TYPESNB);

    if (type == PROGRESS_ERASE) {
        TEST_ASSERT(bytes == 0);
        bytes = 1;
    }

    TEST_ASSERT(bytes >= 0 && bytes <= PAGESIZE);
    TEST_ASSERT(stats[type] <= PAGESIZE-bytes);
    stats[type] += bytes;
}

static void
setup(void) {
    int i;

    flash_init(t_progress, NULL);

    for (i = 0; i < PROGRESS_TYPESNB; i++)
        stats[i] = expect[i] = 0;
}

static void
teardown(void) {
    TEST_ASSERT(stats[PROGRESS_ERASE] == expect[PROGRESS_ERASE]);
    TEST_ASSERT(stats[PROGRESS_WRITEF] == expect[PROGRESS_WRITEF]);
    TEST_ASSERT(stats[PROGRESS_WRITE] == expect[PROGRESS_WRITE]);
    TEST_ASSERT(stats[PROGRESS_READ] == expect[PROGRESS_READ]);
}

static void
test_move(void) {
    ems_size_t dest = 0, src = 1*MB, size = 512*KB;

    TEST_ASSERT(flash_move(dest, size, src) == 0);

    TC4_EXPECT(ERASE, size/ERASEBLOCKSIZE);
    TC4_EXPECT(READ, size);
    TC4_EXPECT(WRITE, size);
}

static void
test_writef(void) {
    ems_size_t dest = 256*KB, size = 256*KB;
    char *tempfn = ecreatetmpf(size);

    TEST_ASSERT(flash_writef(dest, size, tempfn) == 0);

    TC4_EXPECT(ERASE, size/ERASEBLOCKSIZE);
    TC4_EXPECT(WRITEF, size);
    eremove(tempfn);
}

static void
test_write1(void) {
    ems_size_t dest = 256*KB, size = 256*KB;

    TEST_ASSERT(flash_write(dest, size, 0) == 0);

    TC4_EXPECT(ERASE, size/ERASEBLOCKSIZE);
    TC4_EXPECT(WRITE, size);
}

static void
test_write2(void) {
    ems_size_t dest = 64*KB, size = 64*KB;

    TEST_ASSERT(flash_write(dest, size, 0) == 0);

    TC4_EXPECT(ERASE, 0);
    TC4_EXPECT(WRITE, size);
}

static void
test_erase(void) {
    ems_size_t dest = 256*KB;

    TEST_ASSERT(flash_erase(dest) == 0);

    TC4_EXPECT(ERASE, 1);
}

int
main(int argc, char **argv) {    
    test_init(argc, argv, setup, teardown);

    TEST(test_write1);
    TEST(test_write2);
    TEST(test_writef);
    TEST(test_erase);
    TEST(test_move);

    test_done();
}
