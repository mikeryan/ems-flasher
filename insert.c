#include "ems.h"
#include "image.h"
#include "insert.h"

ems_size_t insert_pagesize = PAGESIZE;

/*
 * Important: the struct image provided to these functions should be valid
 * (see update.h). The ROMs must have a size power of two.
 */

/**
 * Insert a ROM in an image using best-fit to limit the fragmentation (as ROM
 * size is always a power of two).
 *
 * Returns non-zero in case of error.
 */
int
image_insert(struct image *image, struct rom *newrom) {
    struct {
        ems_size_t size;
        struct rom *prev;
        ems_size_t offset;
    } bestfit;
    struct rom lastrom = {.offset = insert_pagesize}, *prev, *rom;
    ems_size_t offset;

    image_insert_tail(image, &lastrom);

    offset = 0;
    prev = NULL;
    bestfit.size = (ems_size_t)-1;

    /*
     * For each free contiguous space:
     *    Consider all free ROM (aligned) locations of this space. Always taking
     *    the biggest possible ROM size for each location.
     */
    image_foreach(image, rom) {
        ems_size_t cur, next;

        cur = rom->offset;
        next = cur + rom->romsize;
        while (cur-offset > 0) {
            ems_size_t bigest;

            for (bigest = insert_pagesize; bigest >= MINROMSIZE; bigest/=2) {
                    if (offset%bigest == 0 &&
                        cur-offset >= bigest) {
                            if (bigest >= newrom->romsize) {
                                if (bestfit.size > bigest) {
                                    bestfit.size = bigest;
                                    bestfit.offset = offset;
                                    bestfit.prev = prev;
                                }
                            }
                            break;
                    }
                }
            offset += bigest;
        }
        prev = rom;
        offset = next;
    }

    image_remove(image, &lastrom);

    if (bestfit.size != (ems_size_t)-1) {
        newrom->offset = bestfit.offset;
        if (bestfit.prev != NULL)
            image_insert_after(image, bestfit.prev, newrom);
        else
            image_insert_head(image, newrom);

        return 0;
    }

    return 1;
}

/**
 * Insert a ROM in an image, triggering incremental defragmentation if
 * necessary.
 *
 * Returns non-zero in case of error
 */
int
image_insert_defrag(struct image *image, struct rom *newrom) {
    if (image_insert(image, newrom)) {
        image_defrag(image, newrom->romsize);
        return image_insert(image, newrom);
    }
    return 0;
}

/**
 * Move a ROM from a used buddy of "buddysize" to a free buddy of the same size
 * while conserving the offset relative to the start of the buddy.
 *
 * Used iteratively by image_defrag to move the ROMs of a used buddy to a free
 * one.
 */
static void
moverom(struct image *image, struct rom *destrom, ems_size_t destofs,
    ems_size_t buddysize, struct rom *srcrom) {

    image_remove(image, srcrom);
    srcrom->offset = destofs + srcrom->offset%buddysize;
    if (destrom != NULL)
        image_insert_after(image, destrom, srcrom);
    else
        image_insert_head(image, srcrom);
}

/**
 * Defragment incrementally the image to make space for a ROM of "size" bytes,
 * aligned to its size.
 *
 * Note: it is guaranteed that ROMs are always moved from higher addresses to
 *       lower addresses. This is important for image_update().
 *
 * Returns non-zero in case of error
 */
int
image_defrag(struct image *image, ems_size_t size) {
    struct rom *firstrom, *secondrom, *insertrom, *nxt, *move, *prev;
    ems_size_t firstoffset, secondoffset, buddyoffset;
    struct rom dummyrom[2];

    if (size == MINROMSIZE)
        return 1;

    /* This algorithm takes advantage of the buddy system. */

    /*
     * Reserve two free spaces of size/2 by allocating two dummy ROMs (firstrom
     * and secondrom). This function will be called recursively by
     * image_insert_defrag() when necessary.
     *
     * firstrom will be the ROM with the lowest offset.
     */
    dummyrom[0].romsize = dummyrom[1].romsize = size/2;
    firstrom = &dummyrom[0];
    secondrom = &dummyrom[1];

    if (image_insert_defrag(image, firstrom))
        return 1;

    if (image_insert_defrag(image, secondrom)) {
        image_remove(image, firstrom);
        return 1;
    }

    if (secondrom->offset < firstrom->offset) {
        struct rom *temp = firstrom;
        firstrom = secondrom;
        secondrom = temp;
    }

    /*
     * Move the ROMs contained in the buddy of the second free ROM location into
     * the first free ROM location. This will free the buddy of the second free
     * ROM and then create a free location for a ROM of "size" bytes aligned
     * correctly.
     *
     * Remark: it could happen that the two free spaces are buddies. In this
     * case, no ROM would be moved.
     */

    insertrom = image_prev(firstrom);
    firstoffset = firstrom->offset;
    image_remove(image, firstrom);

    prev = image_prev(secondrom);
    nxt = image_next(secondrom);
    secondoffset = secondrom->offset;
    image_remove(image, secondrom);

    if ((secondoffset & (size/2)) == 0) {
        /* The buddy of the second free space is on its right (higher
           addresses) */
        move = nxt;
        buddyoffset = secondoffset + size/2;
        while (move != NULL && move->offset < buddyoffset+size/2) {
            struct rom *nextmove = image_next(move);
            moverom(image, insertrom, firstoffset, size/2, move);
            insertrom = move;
            move = nextmove;
        }
    } else {
        /* The buddy of the second free space is on its left (lower
           addresses) */
        move = prev;
        buddyoffset = secondoffset - size/2;
        while (move != NULL && move->offset >= buddyoffset) {
            struct rom *prevmove = image_prev(move);
            moverom(image, insertrom, firstoffset, size/2, move);
            move = prevmove;
        }
    }
    return 0;
}
