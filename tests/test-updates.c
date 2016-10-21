#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <sys/stat.h>
#include <err.h>

#include "test.h"

#include "../ems.h"
#include "../header.h"
#include "../cmd.h"
#include "../flash.h"
#include "../update.h"
#include "../updates.h"

#define mock_calls_next(c) SIMPLEQ_NEXT(c, calls)

SIMPLEQ_HEAD(mock_calls, mock_call) mock_calls = \
    SIMPLEQ_HEAD_INITIALIZER(mock_calls);

struct updates *updates;

struct mock_call {
    struct update *update;
    int retval;
    SIMPLEQ_ENTRY(mock_call) calls;
} *mock_expected;

struct romfile *commonromf; /* empty file created at start for tests  */

#define KB 1024

static void
test_updates() {
    int r;
    mock_expected = (struct mock_call*)SIMPLEQ_FIRST(&mock_calls);
    r = apply_updates(0, 0, updates);
    TEST_ASSERT(mock_expected == NULL);
    if (r != 0) exit(r);
}

static struct romfile*
mkromfile() {
    char *path;
    struct romfile *rf = emalloc(sizeof(*rf));
    struct stat buf;

    path = ecreatetmpf(0);
    if (stat(path, &buf) == -1) {
        warn("can't stat temp file: %s", path);
        abort();
    }
    rf->path = path;
    rf->ctime = buf.st_ctime;
    return rf;
}

void
catchint(){}

static struct update*
update_writef(ems_size_t offset, ems_size_t size, struct romfile *romfile) { 
    struct update *u = emalloc(sizeof(*u));
    struct rom *r = emalloc(sizeof(*r));
    *r = (struct rom) {
        .offset = offset,
        .romsize = size,
        .source.type = ROM_SOURCE_FILE,
        .source.u.fileinfo = romfile
    };
    *u = (struct update) {
        .cmd = UPDATE_CMD_WRITEF,
        .rom = r,
    };
    updates_insert_tail(updates, u);
    return u;
}

static struct update*
update_move(ems_size_t offset, ems_size_t size, ems_size_t origoffset) {
    struct update *u = emalloc(sizeof(*u));
    struct rom *r = emalloc(sizeof(*r));
    *r = (struct rom) {
        .offset = offset,
        .romsize = size,
        .source.type = ROM_SOURCE_FLASH,
        .source.u.origoffset = origoffset};
    *u = (struct update) {
    .cmd = UPDATE_CMD_MOVE,
    .rom = r};
    updates_insert_tail(updates, u);
    return u;
}

static struct update*
update_read(int slot, ems_size_t size, ems_size_t offset) {
    struct update *u = emalloc(sizeof(*u));
    struct rom *r = emalloc(sizeof(*r));
    *r = (struct rom) {
        .source.type = ROM_SOURCE_FLASH,
        .source.u.origoffset = offset,
        .romsize = size,
    };
    *u = (struct update) {
        .cmd = UPDATE_CMD_READ,
        .rom = r,
        .u.slot = slot,
    };
    updates_insert_tail(updates, u);
    return u;
}

static struct update*
update_write(ems_size_t offset, ems_size_t size, int slot) {
    struct update *u = emalloc(sizeof(*u));
    struct rom *r = emalloc(sizeof(*r));
    *r = (struct rom) {
        .source.type = ROM_SOURCE_FLASH,
        .offset = offset,
        .romsize = size,
    };
    *u = (struct update) {
        .cmd = UPDATE_CMD_WRITE,
        .rom = r,
        .u.slot = slot,
    };
    updates_insert_tail(updates, u);
    return u;
}

static struct update*
update_erase(ems_size_t offset) {
    struct update *u = emalloc(sizeof(*u));
    *u = (struct update) {
        .cmd = UPDATE_CMD_ERASE,
        .u.offset = offset};
    updates_insert_tail(updates, u);
    return u;
}

static void
flasherr(int e) {
    switch(e) {
    case FLASH_EUSB:
        strcpy(flash_lasterrorstr, "FLASH_EUSB");
        break;
    case FLASH_EFILE:
        strcpy(flash_lasterrorstr, "FLASH_EFILE");
        break;
    case FLASH_EINTR:
        strcpy(flash_lasterrorstr, "FLASH_EINTR"); 
        break;
    }    
} 

void
flash_init(void (*progress_cb)(int, ems_size_t), int (*checkint_cb)(void)) {
}

void
flash_setprogresscb(void (*progress_cb)(int, ems_size_t)) {
}

int
flash_writef(ems_size_t offset, ems_size_t size, char *path) {
    int retval;

    TEST_ASSERT(mock_expected != NULL && mock_expected->update->rom != NULL &&
        mock_expected->update->update_writef_dstofs == offset &&
        mock_expected->update->update_writef_size == size &&
        ((struct romfile*)mock_expected->update->update_writef_fileinfo)->path == path
    );
    retval = mock_expected->retval;
    //printf("WRITEF %"PRIuEMSSIZE" %"PRIuEMSSIZE" %s\n", offset, size, path);
    mock_expected = mock_calls_next(mock_expected);

    if (retval) flasherr(retval);
    return retval;
}

int
flash_move(ems_size_t offset, ems_size_t size, ems_size_t origoffset) {
    int retval;

    TEST_ASSERT(mock_expected != NULL && mock_expected->update->rom != NULL && 
        mock_expected->update->update_move_dstofs == offset &&
        mock_expected->update->update_move_size == size &&
        mock_expected->update->update_move_srcofs == origoffset
    );

    retval = mock_expected->retval;
    //printf("MOVE %"PRIuEMSSIZE" %"PRIuEMSSIZE" %"PRIuEMSSIZE" %d\n",
    //    offset, size, origoffset, retval);
    mock_expected = mock_calls_next(mock_expected);
    if (retval) flasherr(retval);
    return retval;
}

int
flash_read(int slotn, ems_size_t size, ems_size_t offset) {
    int retval;

    TEST_ASSERT(mock_expected != NULL && mock_expected->update->rom != NULL && 
        mock_expected->update->update_read_dstslot == slotn &&
        mock_expected->update->update_read_size == size &&
        mock_expected->update->update_read_srcofs == offset
    );

    retval = mock_expected->retval;
    //printf("READ %d %"PRIuEMSSIZE" %"PRIuEMSSIZE"\n", slotn, size, offset);
    mock_expected = mock_calls_next(mock_expected);
    if (retval) flasherr(retval);
    return retval;
}

int
flash_write(ems_size_t offset, ems_size_t size, int slotn) {
    int retval;

    TEST_ASSERT(mock_expected != NULL && mock_expected->update->rom != NULL && 
        mock_expected->update->update_write_srcslot == slotn &&
        mock_expected->update->update_write_size == size &&
        mock_expected->update->update_write_dstofs == offset
    );

    retval = mock_expected->retval;
    //printf("WRITE %"PRIuEMSSIZE" %"PRIuEMSSIZE" %d\n", offset, size, slotn);
    mock_expected = mock_calls_next(mock_expected);
    if (retval) flasherr(retval);
    return retval;
}

int
flash_erase(ems_size_t offset) {
    int retval;

    TEST_ASSERT(mock_expected != NULL && mock_expected->update->cmd == UPDATE_CMD_ERASE &&
        mock_expected->update->update_erase_dstofs == offset);

    retval = mock_expected->retval;
    //printf("ERASE %"PRIuEMSSIZE" %d\n", offset, mock_expected->retval);
    mock_expected = mock_calls_next(mock_expected);
    if (retval) flasherr(retval);
    return retval;
}

int
flash_delete(ems_size_t offset, int blocks) {
    return 0;
}

void progress_newline(void){}
void progress_start(struct updates *updates){}
void progress(int type, ems_size_t bytes){}

int
checkint() {
    return 0;
}

#define update(call) update_##call

#define mock(call, rv) \
    do { \
        struct mock_call *c = emalloc(sizeof(*c)); \
        c->update = call; \
        c->retval = (rv); \
        SIMPLEQ_INSERT_TAIL(&mock_calls, c, calls); \
    } while (0)

void
tc1_teardown(void) {
    test_updates();
}

/*
 * Invoke all commands
 */
void
tc1_test_basic(void) {
    mock(update(writef(0, 32*KB, commonromf)), 0);
    mock(update(read(0, 32*KB, 32*KB)), 0);
    mock(update(write(0, 32*KB, 0)), 0);
    mock(update(erase(128*KB)), 0);
    mock(update(move(0, 32*KB, 64*KB)), 0);
}

/*
 * Place a ROM to an erase-block containing several ROMs without errors.
 */
void
tc1_test_recovery1(void) {
    mock(update(read(0, 32*KB, 32*KB)), 0);
    mock(update(read(1, 32*KB, 64*KB)), 0);
    mock(update(read(2, 32*KB, 96*KB)), 0);
    mock(update(writef(0, 32*KB, commonromf)), 0);
    mock(update(write(32*KB, 32*KB, 0)), 0);
    mock(update(write(64*KB, 32*KB, 1)), 0);
    mock(update(write(96*KB, 32*KB, 2)), 0);
    mock(update(writef(128*KB, 32*KB, commonromf)), 0);
}

/*
 * Same as recovery1 but a user interruption occurs while flashing a file
 */
void
tc1_test_recovery2(void) /* exitcode=1 */ {
    mock(update(read(0, 32*KB, 32*KB)), 0);
    mock(update(read(1, 32*KB, 64*KB)), 0);
    mock(update(read(2, 32*KB, 96*KB)), 0);
    mock(update(writef(0, 32*KB, commonromf)), FLASH_EINTR);

    /* Recover this erase-block only and stop  */
    mock(update(write(32*KB, 32*KB, 0)), 0);
    mock(update(write(64*KB, 32*KB, 1)), 0);
    mock(update(write(96*KB, 32*KB, 2)), 0);
    update(writef(128*KB, 32*KB, commonromf));
}

/*
 * Check if only write commands on the same block are executed in a recovery
 */
void
tc1_test_recovery3(void) /* exitcode=1 */ {
    mock(update(read(0, 32*KB, 32*KB)), 0);
    mock(update(read(2, 32*KB, 96*KB)), 0);
    mock(update(writef(0, 32*KB, commonromf)), FLASH_EINTR);
    mock(update(write(32*KB, 32*KB, 0)), 0);
    update(move(64*KB, 32*KB, 256*KB));
    mock(update(write(96*KB, 32*KB, 2)), 0);
    update(write(128*KB, 32*KB, 0));
    update(move(160*KB, 32*KB, 384*KB));
}

/*
 * A USB error should not trigger the recovery procedure
 */
void
tc1_test_recovery4(void) /* exitcode=1 */ {
    mock(update(read(0, 32*KB, 32*KB)), 0);
    mock(update(read(1, 32*KB, 64*KB)), 0);
    mock(update(read(2, 32*KB, 96*KB)), FLASH_EUSB);
    update(writef(0, 32*KB, commonromf));
    update(write(32*KB, 32*KB, 0));
    update(write(64*KB, 32*KB, 1));
    update(write(96*KB, 32*KB, 2));
    update(writef(128*KB, 32*KB, commonromf));
}

/*
 * An error USB write error while recovery
 */
void
tc1_test_recovery5(void) /* exitcode=1 */ {
    mock(update(read(0, 32*KB, 32*KB)), 0);
    mock(update(read(1, 32*KB, 64*KB)), 0);
    mock(update(read(2, 32*KB, 96*KB)), 0);
    mock(update(writef(0, 32*KB, commonromf)), FLASH_EFILE);
    mock(update(write(32*KB, 32*KB, 0)), 0);
    mock(update(write(64*KB, 32*KB, 1)), FLASH_EUSB);
    update(write(96*KB, 32*KB, 2));
    update(writef(128*KB, 32*KB, commonromf));
}

/*
 * a USB error on the first write of the recovery procedure
 */
void
tc1_test_recovery6(void) /* exitcode=1 */ {
    mock(update(read(0, 32*KB, 32*KB)), 0);
    mock(update(read(1, 32*KB, 64*KB)), 0);
    mock(update(read(2, 32*KB, 96*KB)), 0);
    mock(update(writef(0, 32*KB, commonromf)), FLASH_EFILE);
    mock(update(write(32*KB, 32*KB, 0)), FLASH_EUSB);
    update(write(64*KB, 32*KB, 1));
    update(write(96*KB, 32*KB, 2));
    update(writef(128*KB, 32*KB, commonromf));
}

/*
 * USB error should not trigger the recovery procedure 2
 */
void
tc1_test_recovery7(void) /* exitcode=1 */ {
    mock(update(read(0, 32*KB, 32*KB)), 0);
    mock(update(read(1, 32*KB, 64*KB)), 0);
    mock(update(read(2, 32*KB, 96*KB)), 0);
    mock(update(writef(0, 32*KB, commonromf)), 0);
    mock(update(write(32*KB, 32*KB, 0)), FLASH_EUSB);
    update(write(64*KB, 32*KB, 1));
    update(write(96*KB, 32*KB, 2));
    update(writef(128*KB, 32*KB, commonromf));
}

/*
 * Same as revovery2 but user interruption on the first write
 */
void
tc1_test_recovery8(void) /* exitcode=1 */ {
    mock(update(read(0, 32*KB, 32*KB)), 0);
    mock(update(read(1, 32*KB, 64*KB)), 0);
    mock(update(read(2, 32*KB, 96*KB)), 0);
    mock(update(writef(0, 32*KB, commonromf)), 0);
    mock(update(write(32*KB, 32*KB, 0)), FLASH_EINTR);
    mock(update(write(64*KB, 32*KB, 1)), 0);
    mock(update(write(96*KB, 32*KB, 2)), 0);
    update(writef(128*KB, 32*KB, commonromf));
}

/*
 * The file specified in writef doesn't exists
 */
void
tc1_test_file1(void) /* exitcode=1 */ {
    struct romfile *romf;
    romf = mkromfile();
    remove(romf->path);

    update(writef(0, 32*KB, romf));
    mock(update(write(32*KB, 32*KB, 0)), 0);
    update(move(64*KB, 32*KB, 256*KB));
}

/*
 * The ctime of the file specified in writef has changed
 */
void
tc1_test_file2(void) /* exitcode=1 */ {
    struct romfile *romf;
    romf = mkromfile();
    romf->ctime++;
    update(writef(0, 32*KB, romf));
    mock(update(write(32*KB, 32*KB, 0)), 0);
    update(move(64*KB, 32*KB, 256*KB));
}

int
main(int argc, char **argv) {
    updates = malloc(sizeof(*updates));
    updates_init(updates);
    commonromf = mkromfile();

    test_init(argc, argv, NULL, tc1_teardown);
    TEST(tc1_test_basic);
    TEST(tc1_test_recovery1);
    TEST_exit(tc1_test_recovery2, 1);
    TEST_exit(tc1_test_recovery3, 1);
    TEST_exit(tc1_test_recovery4, 1);
    TEST_exit(tc1_test_recovery5, 1);
    TEST_exit(tc1_test_recovery6, 1);
    TEST_exit(tc1_test_recovery7, 1);
    TEST_exit(tc1_test_recovery8, 1);
    TEST_exit(tc1_test_file1, 1);
    TEST_exit(tc1_test_file2, 1);

    test_done();
}
