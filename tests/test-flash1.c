/*
 * Test case 1 for flash.c: test the functions in normal operation, without
 * errors nor interruptions.
 * Mocks are used for ems_read() and ems_write()
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

#define BUF_FF (-1)
#define BUF_00 (-2)

/* struct mock_call: expected parameters and value to return for a call to
 * ems_read() or ems_write().
 *
 * Notice that the type of "buf" has been changed from uint32_t* to uint32_t
 * to save some space. The content of the buffer is represented by an integer
 * value:
 *   - BUF_FF represents a buffer filled with "count" 0xff bytes
 *   - BUF_00 represents a buffer filled with "count" zero bytes
 *   - another value represents a buffer filled with "count" divided by 32
 *     chunks of 32 bytes. The first chunk starts with the value (copied with
 *     memcpy()). The value of the other bytes doesn't matter. The second chunk
 *     starts with the value added to 32 and so on.
 *     The mock of ems_read() generates a buffer as described above.
 *     The mock of ems_write() checks that the buffer passed by the tested
 *     function matches the scheme described above. This technique is not
 *     perfect but I think it is sufficient to ensure that the program didn't
 *     mess with the buffer it passes to ems_write(). The passed buffer may
 *     come from ems_read() or a file or may be from another buffer filled by
 *     flash_read().
 */
struct mock_call {
    enum {CALL_EMS_WRITE, CALL_EMS_READ} func;
    int from;
    uint32_t offset;
    uint32_t buf;
    size_t count;
    int ret;
    SIMPLEQ_ENTRY(mock_call) calls;
} *mock_expected;

SIMPLEQ_HEAD(mock_calls, mock_call) mock_calls = \
    SIMPLEQ_HEAD_INITIALIZER(mock_calls);

static ems_size_t oldflashlastofs;

static struct mock_call*
mock_create(int func, int from, uint32_t offset, uint32_t buf, size_t count) {
    struct mock_call *call = emalloc(sizeof(*call));
    *call = (struct mock_call) {func, from, offset, buf, count};
    return call;
}

#define mock_ems_read(from, offset, buf, count) \
    mock_create(CALL_EMS_READ, (from), (offset), (buf), (count))

#define mock_ems_write(to, offset, buf, count) \
    mock_create(CALL_EMS_WRITE, (to), (offset), (buf), (count))

#define mock(call, retval) \
    do { \
        struct mock_call *c = mock_##call; \
        c->ret = retval; \
        SIMPLEQ_INSERT_TAIL(&mock_calls, c, calls); \
    } while (0)

static int
mock_call_check(int func, int from, uint32_t offset, unsigned char *buf,
    size_t count) {

    struct mock_call *expect = mock_expected;
    uint32_t data;
    int i;

    if (expect == NULL ||
        func != expect->func ||
        from != expect->from ||
        offset != expect->offset ||
        count != expect->count) {
            warnx("mock: unexpected function call");
            abort();
        }

    if (func == CALL_EMS_READ) {
        for (i = 0; i < count; i += WRITEBLOCKSIZE) {
            data = mock_expected->buf+i;
            memcpy(buf+i, &data, sizeof(data));
        }
    } else {
        if (expect->buf == BUF_00 || expect->buf == BUF_FF) {
            int i, cmp;
            cmp = expect->buf == BUF_00?0:0xff;
            for (i = 0; i < count; i++)
                if (buf[i] != cmp) {
                    warnx("mock: buf");
                    abort();
                }
        } else {
            for (i = 0; i < count; i += WRITEBLOCKSIZE) {
                memcpy(&data, &buf[i], sizeof(data));
                if (data != expect->buf + i) {
                    warnx("data %"PRIu32" %"PRIu32, data,
                        expect->buf + i);
                    abort();
                }
            }
        }
    }

    return mock_expected->ret;
}

/*
 * Mock of ems_write()
 */
int
ems_write(int to, uint32_t offset, unsigned char *buf, size_t count) {
    int r;

    r = mock_call_check(CALL_EMS_WRITE, to, offset, buf, count);

    mock_expected = SIMPLEQ_NEXT(mock_expected, calls);

    return r;
}

/*
 * Mock of ems_read()
 */
int
ems_read(int from, uint32_t offset, unsigned char *buf, size_t count) {
    int r;

    r = mock_call_check(CALL_EMS_READ, from, offset, buf, count);

    mock_expected = SIMPLEQ_NEXT(mock_expected, calls);

    return r;
}

static void
setup(void) {
    flash_init(NULL, NULL);

    oldflashlastofs = flash_lastofs;
}

static void
teardown(void) {
    TEST_ASSERT(mock_expected == NULL);
}

static void
test_erase(void) {
    ems_size_t dest = 128*KB;

    mock(ems_write(TO_ROM, dest, BUF_FF, WRITEBLOCKSIZE), WRITEBLOCKSIZE);
    mock(ems_write(TO_ROM, dest + WRITEBLOCKSIZE, BUF_FF, WRITEBLOCKSIZE),
        WRITEBLOCKSIZE);

    mock_expected = SIMPLEQ_FIRST(&mock_calls);

    TEST_ASSERT(!flash_erase(dest));
    TEST_ASSERT(flash_lastofs == dest+WRITEBLOCKSIZE);
}

static void
test_read(void) {
    ems_size_t src = 3*64*KB, size = 64*KB;
    int i;

    for (i = 0; i < size; i += READBLOCKSIZE)
        mock(ems_read(FROM_ROM, src+i, src+i, READBLOCKSIZE),
            READBLOCKSIZE);

    mock_expected = SIMPLEQ_FIRST(&mock_calls);

    TEST_ASSERT(!flash_read(0, size, src));
    TEST_ASSERT(flash_lastofs == oldflashlastofs);
}

static void
test_write(void) {
    ems_size_t dest = 4*64*KB, size = 64*KB;
    int i;

    for (i = 0; i < size; i += WRITEBLOCKSIZE)
        if (i != 0x100)
            mock(ems_write(TO_ROM, dest+i, BUF_00 /*src+i*/, WRITEBLOCKSIZE),
                WRITEBLOCKSIZE);
    mock(ems_write(TO_ROM, dest+0x100, BUF_00 /* src+0x100 */, WRITEBLOCKSIZE),
        WRITEBLOCKSIZE);

    mock_expected = SIMPLEQ_FIRST(&mock_calls);

    TEST_ASSERT(!flash_write(dest, size, 0));
    TEST_ASSERT(flash_lastofs == dest+size-WRITEBLOCKSIZE);
}

static void
test_move(void) {
    ems_size_t src = 2*MB, dest = 1*MB, size = 256*KB;
    int i, j;

    for (i = 0; i < size; i += READBLOCKSIZE) {
        mock(ems_read(FROM_ROM, src+i, src+i, READBLOCKSIZE), READBLOCKSIZE);
        for (j=0; j < READBLOCKSIZE ;j += WRITEBLOCKSIZE) {
            if (i+j != 0x100 && i+j != 0x120)
                mock(ems_write(TO_ROM, dest+i+j, src+i+j, WRITEBLOCKSIZE),
                    WRITEBLOCKSIZE);
        }
    }
    mock(ems_write(TO_ROM, dest+0x100, src+0x100, WRITEBLOCKSIZE),
        WRITEBLOCKSIZE);
    mock(ems_write(TO_ROM, dest+0x120, src+0x120, WRITEBLOCKSIZE),
        WRITEBLOCKSIZE);
    mock(ems_write(TO_ROM, src+0x110, BUF_00, WRITEBLOCKSIZE), WRITEBLOCKSIZE);
    mock(ems_write(TO_ROM, src+0x130, BUF_00, WRITEBLOCKSIZE), WRITEBLOCKSIZE);

    mock_expected = SIMPLEQ_FIRST(&mock_calls);

    TEST_ASSERT(!flash_move(dest, size, src)); 
    TEST_ASSERT(flash_lastofs == dest+size-WRITEBLOCKSIZE);
}

static void
test_writef(void) {
    ems_size_t dest = 256*KB;
    ems_size_t size = 256*KB;
    FILE *f;
    char *tmpf; 
    int i;

    tmpf = ecreatetmpf(0);
    if ((f = fopen(tmpf, "wb")) == NULL) {
        warn("error: can't create temp file: %s", tmpf);
        abort();
    }

    for (i = 0; i < size; i += WRITEBLOCKSIZE) {   
        char buf[WRITEBLOCKSIZE];
        ems_size_t data = i;

        memset(buf, 0, WRITEBLOCKSIZE);
        memcpy(buf, &data, sizeof(data)); 

        if (fwrite(buf, WRITEBLOCKSIZE, 1, f) != 1) {
            warn("error: can't write to temp file: %s", tmpf);
            abort();
        }
    }
    if (fclose(f) == EOF) {
        warn("error: can't close temp file");
        abort();
    }

    for (i = 0; i < size; i += WRITEBLOCKSIZE) {
        if (i != 0x100 && i!=0x120)
            mock(ems_write(TO_ROM, dest+i, i, WRITEBLOCKSIZE), WRITEBLOCKSIZE);
    }
    mock(ems_write(TO_ROM, dest+0x100, 0x100, WRITEBLOCKSIZE), WRITEBLOCKSIZE);
    mock(ems_write(TO_ROM, dest+0x120, 0x120, WRITEBLOCKSIZE), WRITEBLOCKSIZE);

    mock_expected = SIMPLEQ_FIRST(&mock_calls);

    TEST_ASSERT(!flash_writef(dest, size, tmpf));
    TEST_ASSERT(flash_lastofs == dest+size-WRITEBLOCKSIZE);
    eremove(tmpf);
}

static void
test_delete1(void) {
    ems_size_t dest = 128*KB;

    mock(ems_write(TO_ROM, dest+0x130, BUF_00, WRITEBLOCKSIZE), WRITEBLOCKSIZE);

    mock_expected = SIMPLEQ_FIRST(&mock_calls);

    TEST_ASSERT(!flash_delete(dest, 1));
    TEST_ASSERT(flash_lastofs == oldflashlastofs);
}

static void
test_delete2(void) {
    ems_size_t dest = 128*KB;

    mock(ems_write(TO_ROM, dest+0x110, BUF_00, WRITEBLOCKSIZE), WRITEBLOCKSIZE);
    mock(ems_write(TO_ROM, dest+0x130, BUF_00, WRITEBLOCKSIZE), WRITEBLOCKSIZE);

    mock_expected = SIMPLEQ_FIRST(&mock_calls);

    TEST_ASSERT(!flash_delete(dest, 2));
    TEST_ASSERT(flash_lastofs == oldflashlastofs);
}

int
main(int argc, char **argv) {    
    test_init(argc, argv, setup, teardown);

    TEST(test_erase);
    TEST(test_read);
    TEST(test_write);
    TEST(test_move);
    TEST(test_writef);
    TEST(test_delete1);
    TEST(test_delete2);

    test_done();
}
