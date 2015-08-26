#include <stdlib.h>

#include "update.h"

#define ERASEBLOCKNB(ofs) ((ofs)/ERASEBLOCKSIZE)

/**
 * Add an update (command) to a struct updates (see update.h for the data
 * format)
 *
 * Returns non-zero in case of error
 */
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

    /*
     * Save in memory the ROMs present in the erase-block: the moved ROMs
     * originating from the same erase block and the untouched ROMs.
     */
    slot = 0;
    FOREACH_SMALLROM(from, cur) {
        if (cur->source.type == ROM_SOURCE_FLASH &&
            ERASEBLOCKNB(cur->source.u.origoffset) == ERASEBLOCKNB(from->offset)) {
                if ((r = insert_read(updates, cur, slot++)) != 0)
                    return r;
        }
    }

    /* Erase the erase-block explicitly if necessary */
    if (from->offset%ERASEBLOCKSIZE != 0) {
        r = insert_erase(updates, from->offset - from->offset%ERASEBLOCKSIZE);
        if (r != 0)
            return r;
    }

    /*
     * Flash the ROMs saved previously, the new ROMs and the moved ROMs
     * originating from another erase-block.
     */
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

/**
 * Generate I/O commands to be applied to the original image to obtain the
 * new image.
 *
 * image_defrag() in insert.c guarantees that ROMs are always moved from higher
 * addresses to lower addresses. As we generate update commands from lower
 * addresses to higher addresses, we can be sure that the source ROM of a move
 * command cannot be overwritten by a previous command. This keeps us from doing
 * a topological sorting.
 *
 * image must be valid (see image.h) and the ROMs must have a size power of two.
 *
 * Returns non-zero in case of error.
 */
int
image_update(struct image *image, struct updates **updates) {
    struct rom *rom;
    int r;

    if ((*updates = malloc(sizeof(**updates))) == NULL)
        return 1;

    updates_init(*updates);

    /* For each ROM to be flashed (new ROM or moved ROM): */
    image_foreach(image, rom) {
        if (rom->source.type == ROM_SOURCE_FLASH &&
            rom->offset == rom->source.u.origoffset)
                continue;

        if (rom->romsize >= ERASEBLOCKSIZE) {
            /*
             * ROM >= 128 KB (erase-block size): there is no precaution to take,
             * the destination erase-blocks can be overwritten.
             */
            if ((r = update_bigrom(*updates, rom)) != 0)
                return r;
        } else {
            /* 
             * ROM < 128 KB (small ROMs): the destination erase-block may
             * contain other ROMs. We must preserve the existing ROMs in memory
             * before the erasure of the erase-block.
             */
            struct rom *prev, *next, *from;

            /* Compute from = first ROM of the destination erase-block */
            from = rom;
            while ((prev = image_prev(from)) != NULL &&
                ERASEBLOCKNB(prev->offset) == ERASEBLOCKNB(from->offset)) {
                    from = prev;
            }

            if ((r = update_smallroms(*updates, from)))
                return r;

            /* 
             * Compute next = last ROM of the erase-block, so the next iteration
             * will start at the next erase-block
             */
            while ((next = image_next(rom)) != NULL &&
                ERASEBLOCKNB(next->offset) == ERASEBLOCKNB(from->offset)) {
                    rom = next;
            }
        }
    }

    return 0;
}
