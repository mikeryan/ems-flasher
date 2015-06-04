#ifndef EMS_UPDATE_H
#define EMS_UPDATE_H

#include "ems.h"
#include "image.h"
#include "queue.h"

#define UPDATE_NBSLOTS 3

SIMPLEQ_HEAD(updates, update);

struct update {
    SIMPLEQ_ENTRY(update) updates;

    enum {
        UPDATE_CMD_WRITEF, UPDATE_CMD_MOVE, UPDATE_CMD_WRITE,
        UPDATE_CMD_READ, UPDATE_CMD_ERASE
    } cmd;

    struct rom *rom;

    union {
        int slot;
        ems_size_t offset;
    } u;

#define update_writef_fileinfo rom->source.u.fileinfo
#define update_writef_dstofs rom->offset
#define update_writef_size rom->romsize

#define update_move_srcofs rom->source.u.origoffset
#define update_move_dstofs rom->offset
#define update_move_size rom->romsize

#define update_read_srcofs rom->source.u.origoffset
#define update_read_dstslot u.slot
#define update_read_size rom->romsize

#define update_write_dstofs rom->offset
#define update_write_srcslot u.slot
#define update_write_size rom->romsize

#define update_erase_dstofs u.offset
};

#define updates_foreach(us, u) SIMPLEQ_FOREACH(u, us, updates)
#define updates_insert_tail(us, u) SIMPLEQ_INSERT_TAIL(us, u, updates)
#define updates_next(u) SIMPLEQ_NEXT(u, updates)
#define updates_init(us) SIMPLEQ_INIT(us)

int image_update(struct image*, struct updates**);

#endif /* EMS_UPDATE_H */
