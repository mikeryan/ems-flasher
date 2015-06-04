#include <stdlib.h>

#include "update.h"

#define ERASEBLOCKNB(ofs) ((ofs)/ERASEBLOCKSIZE)

static int
insert_cmd(struct updates *updates, struct update update) {
    struct update *u;

    if ((u = malloc(sizeof(*u))) == NULL)
        return 1;
    *u = update;
    updates_insert_tail(updates, u);
    return 0;
}

#define insert_writef(u, r) \
    insert_cmd(u, (struct update){.cmd = UPDATE_CMD_WRITEF, .rom = (r)})

#define insert_move(u, r) \
    insert_cmd(u, (struct update){.cmd = UPDATE_CMD_MOVE, .rom = (r)})

#define insert_write(u, r, s) \
    insert_cmd(u, (struct update){.cmd = UPDATE_CMD_WRITE, .rom = (r), \
        .update_write_srcslot = (s)})

#define insert_read(u, r, s) \
    insert_cmd(u, (struct update){.cmd = UPDATE_CMD_READ, .rom = (r), \
        .update_read_dstslot = (s)})

#define insert_erase(u, o) \
    insert_cmd(u, (struct update){.cmd = UPDATE_CMD_ERASE, .rom = NULL, \
        .update_erase_dstofs = (o)})

static int
update_bigrom(struct updates *updates, struct rom *rom) {
    int r;

    if (rom->source.type == ROM_SOURCE_FILE) {
        if ((r = insert_writef(updates, rom)) != 0)
            return r;
    } else {
        if ((r = insert_move(updates, rom)) != 0)
            return r;
    }
    return 0;
}

static int
update_smallroms(struct updates *updates, struct rom *from) {
    struct rom *cur;
    int r, slot;

#define FOREACH_SMALLROM(from, cur)                                            \
    for ((cur) = (from);                                                       \
        (cur) != NULL &&                                                       \
            ERASEBLOCKNB((cur)->offset) == ERASEBLOCKNB((from)->offset);       \
        (cur) = image_next(cur))

    slot = 0;
    FOREACH_SMALLROM(from, cur) {
        if (cur->source.type == ROM_SOURCE_FLASH &&
            ERASEBLOCKNB(cur->source.u.origoffset) == ERASEBLOCKNB(from->offset)) {
                if ((r = insert_read(updates, cur, slot++)) != 0)
                    return r;
        }
    }

    if (from->offset%ERASEBLOCKSIZE != 0) {
        r = insert_erase(updates, from->offset - from->offset%ERASEBLOCKSIZE);
        if (r != 0)
            return r;
    }

    slot = 0;
    FOREACH_SMALLROM(from, cur) {
        if (cur->source.u.origoffset != -1 &&
            ERASEBLOCKNB(cur->source.u.origoffset) == ERASEBLOCKNB(from->offset)) {
                if ((r = insert_write(updates, cur, slot++)) != 0)
                    return r;
        } else {
            if (cur->source.type == ROM_SOURCE_FILE) {
                if ((r = insert_writef(updates, cur)) != 0)
                    return r;
            } else {
                if ((r = insert_move(updates, cur)) != 0)
                    return r;
            }
        }
    }
    return 0;
}

int
image_update(struct image *image, struct updates **updates) {
    struct rom *rom;
    int r;

    if ((*updates = malloc(sizeof(**updates))) == NULL)
        return 1;

    updates_init(*updates);

    image_foreach(image, rom) {
        if (rom->source.type == ROM_SOURCE_FLASH &&
            rom->offset == rom->source.u.origoffset)
                continue;

        if (rom->romsize >= ERASEBLOCKSIZE) {
            if ((r = update_bigrom(*updates, rom)) != 0)
                return r;
        } else {
            struct rom *prev, *next, *from;

            from = rom;
            while ((prev = image_prev(from)) != NULL &&
                ERASEBLOCKNB(prev->offset) == ERASEBLOCKNB(from->offset)) {
                    from = prev;
            }

            if ((r = update_smallroms(*updates, from)))
                return r;

            while ((next = image_next(rom)) != NULL &&
                ERASEBLOCKNB(next->offset) == ERASEBLOCKNB(from->offset)) {
                    rom = next;
            }
        }
    }

    return 0;
}
