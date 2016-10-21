/*
 * Test case 2 for flash.c
 * Checks that the functions behave correctly when interrupted by a signal.
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

static int flipflop;
static int intval;
ems_size_t int_write;
ems_size_t int_read;
int int_count;

static int
testint(void) {
    int_count++;
    TEST_ASSERT(flipflop == 0);
    return intval;
}

int
ems_write(int to, uint32_t offset, unsigned char *buf, size_t count) {
    flipflop = 1-flipflop;
    if (int_write == offset)
        intval = 1;
    return count;
}

int
ems_read(int from, uint32_t offset, unsigned char *buf, size_t count) {
    TEST_ASSERT(flipflop == 0);
    if (int_read == offset)
        intval = 1;
    return count;
}

static void
setup(void) {
    flash_init(NULL, testint);
    int_count = intval = flipflop = 0;
    int_read = int_write = -1;
}

static void
test_move1(void) {
    ems_size_t src = 2*MB, dest = 1*MB, size = 256*KB;

    int_read = src;
    TEST_ASSERT(flash_move(dest, size, src) == FLASH_EINTR);
    TEST_ASSERT(flipflop == 0);
}

static void
test_move2(void) {
    ems_size_t src = 2*MB, dest = 1*MB, size = 256*KB;

    int_write = dest+4*KB-32;
    TEST_ASSERT(flash_move(dest, size, src) == FLASH_EINTR);
    TEST_ASSERT(flipflop == 0);
}

static void
test_move3(void) {
    ems_size_t src = 2*MB, dest = 1*MB, size = 256*KB;

    int_write = dest+32;
    TEST_ASSERT(flash_move(dest, size, src) == FLASH_EINTR);
    TEST_ASSERT(flipflop == 0);
}

static void
test_move4(void) {
    ems_size_t src = 2*MB, dest = 1*MB, size = 256*KB;

    int_write = dest+0x120;
    TEST_ASSERT(flash_move(dest, size, src) == FLASH_EINTR);
    TEST_ASSERT(flipflop == 0);
}

static void
test_writef1(void) {
    ems_size_t dest = 0, size = 32*KB;
    char *tempfn = ecreatetmpf(size);
    int_write = 32;
    TEST_ASSERT(flash_writef(dest, size, tempfn) == FLASH_EINTR);
    TEST_ASSERT(flipflop == 0);
}

static void
test_writef2(void) {
    ems_size_t dest = 0, size = 32*KB;
    char *tempfn = ecreatetmpf(size);
    int_write = 0x120;
    TEST_ASSERT(flash_writef(dest, size, tempfn) == 0);
    TEST_ASSERT(flipflop == 0);
    eremove(tempfn);
}

static void
test_write1(void) {
    ems_size_t dest = 1*MB, size = 256*KB;
    int slot = 0;

    int_write = 32;
    TEST_ASSERT(flash_write(dest, size, slot) == 0);
    TEST_ASSERT(flipflop == 0);
}

static void
test_write2(void) {
    ems_size_t dest = 0;
    ems_size_t size = 256*KB;
    int slot = 0;

    int_write = 0x120;
    TEST_ASSERT(flash_write(dest, size, slot) == 0);
    TEST_ASSERT(flipflop == 0);
}

static void
test_read(void) {
    ems_size_t src = 0, size = 32*KB;
    int slot = 0;

    int_read = 0;
    TEST_ASSERT(flash_read(slot, size, src) == FLASH_EINTR);
}

static void
test_erase1(void) {
    int_write = 32;
    TEST_ASSERT(flash_erase(0) == 0);
    TEST_ASSERT(flipflop == 0);
}

static void
test_erase2(void) {
    intval = 1;
    TEST_ASSERT(flash_erase(0) == FLASH_EINTR);
    TEST_ASSERT(flipflop == 0);
}

static void
test_delete1(void) {
    int_write = 0x130;
    TEST_ASSERT(flash_delete(0, 1) == 0);
    TEST_ASSERT(flipflop == 1);
}

static void
test_delete2(void) {
    int_write = 0x130;
    TEST_ASSERT(flash_delete(0, 2) == 0);
    TEST_ASSERT(flipflop == 0);
}

int
main(int argc, char **argv) {    
    test_init(argc, argv, setup, NULL);

    TEST(test_move1);
    TEST(test_move2);
    TEST(test_move3);
    TEST(test_move4);
    TEST(test_writef1);
    TEST(test_writef2);
    TEST(test_write1);
    TEST(test_write2);
    TEST(test_read);
    TEST(test_erase1);
    TEST(test_erase2);
    TEST(test_delete1);
    TEST(test_delete2);

    test_done();
}
