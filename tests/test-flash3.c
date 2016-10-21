/*
 * Test case 3 for flash.c
 * Check that the functions return immediatly with FLASH_EUSB when ems_read() or
 * ems_write() fails.
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

#define TC3_WRITE_ERROR(o) write_error_ofs = (o)
#define TC3_READ_ERROR(o) read_error_ofs = (o)

ems_size_t read_error_ofs;
ems_size_t write_error_ofs;
int error_returned;

int
ems_write(int to, uint32_t offset, unsigned char *buf, size_t count) {
    TEST_ASSERT(error_returned == 0);
    if (write_error_ofs == offset) {
        error_returned = 1;
        return -1;
    }
    return count;
}

int
ems_read(int from, uint32_t offset, unsigned char *buf, size_t count) {
    TEST_ASSERT(error_returned == 0);
    if (read_error_ofs == offset) {
        error_returned = 1;
        return -1;
    }
    return count;
}

static void
setup(void) {
    flash_init(NULL, NULL);
    write_error_ofs = read_error_ofs = -1;
}

static void
test_write1(void) {
    TC3_WRITE_ERROR(0x120);
    TEST_ASSERT(flash_write(0, 32*KB, 0) == FLASH_EUSB);
}

static void
test_write2(void) {
    TC3_WRITE_ERROR(32);
    TEST_ASSERT(flash_write(0, 32*KB ,0) == FLASH_EUSB);
}

static void
test_write3(void) {
    TC3_WRITE_ERROR(0x100);
    TEST_ASSERT(flash_write(0, 32*KB, 0) == FLASH_EUSB);
}

static void
test_write4(void) {
    TC3_WRITE_ERROR(16*KB);
    TEST_ASSERT(flash_write(0, 32*KB, 0) == FLASH_EUSB);
}

static void
test_move1(void) {
    TC3_READ_ERROR(32*KB);
    TEST_ASSERT(flash_move(0, 32*KB, 32*KB) == FLASH_EUSB);
}

static void
test_move2(void) {
    TC3_WRITE_ERROR(16*KB);
    TEST_ASSERT(flash_move(0, 32*KB, 32*KB) == FLASH_EUSB);
}

static void
test_move3(void) {
    TC3_WRITE_ERROR(0x120);
    TEST_ASSERT(flash_move(0, 32*KB, 32*KB) == FLASH_EUSB);
}

static void
test_read(void) {
    TC3_READ_ERROR(16*KB);
    TEST_ASSERT(flash_read(0, 32*KB,0) == FLASH_EUSB);
}

static void
test_writef1(void) {
    ems_size_t dest = 0, size = 32*KB;
    char *tempfn = ecreatetmpf(size);
    TC3_WRITE_ERROR(0x100);
    TEST_ASSERT(flash_writef(dest, size, tempfn) == FLASH_EUSB);
    eremove(tempfn);
}

static void
test_writef2(void) {
    ems_size_t dest = 0, size = 32*KB;
    char *tempfn = ecreatetmpf(size);
    TC3_WRITE_ERROR(16*KB);
    TEST_ASSERT(flash_writef(dest, size, tempfn) == FLASH_EUSB);
    eremove(tempfn);
}

static void
test_writef3(void) {
    char *tempfn = ecreatetmpf(0);
    eremove(tempfn);
    TEST_ASSERT(flash_writef(0, 32*KB, tempfn) == FLASH_EFILE);
}

static void
test_erase(void) {
    TC3_WRITE_ERROR(32);
    TEST_ASSERT(flash_erase(0) == FLASH_EUSB);
}

static void
test_delete(void) {
    TC3_WRITE_ERROR(0x130);
    TEST_ASSERT(flash_delete(0, 1) == FLASH_EUSB);
}

int
main(int argc, char **argv) {    
    test_init(argc, argv, setup, NULL);

    TEST(test_write1);
    TEST(test_write2);
    TEST(test_write3);
    TEST(test_write4);
    TEST(test_move1);
    TEST(test_move2);
    TEST(test_move3);
    TEST(test_writef1);
    TEST(test_writef2);
    TEST(test_writef3);
    TEST(test_read);
    TEST(test_erase);
    TEST(test_delete);

    test_done();
}
