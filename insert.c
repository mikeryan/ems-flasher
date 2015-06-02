#include "ems.h"
#include "image.h"
#include "insert.h"

ems_size_t insert_pagesize = PAGESIZE;

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

int
image_insert_defrag(struct image *image, struct rom *newrom) {
    if (image_insert(image, newrom)) {
        image_defrag(image, newrom->romsize);
        return image_insert(image, newrom);
    }
    return 0;
}

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

int
image_defrag(struct image *image, ems_size_t size) {
    struct rom *firstrom, *secondrom, *insertrom, *nxt, *move, *prev;
    ems_size_t firstoffset, secondoffset, buddyoffset;
    struct rom dummyrom[2];

    if (size == MINROMSIZE)
        return 1;

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

    insertrom = image_prev(firstrom);
    firstoffset = firstrom->offset;
    image_remove(image, firstrom);

    prev = image_prev(secondrom);
    nxt = image_next(secondrom);
    secondoffset = secondrom->offset;
    image_remove(image, secondrom);

    if ((secondoffset & (size/2)) == 0) {
        move = nxt;
        buddyoffset = secondoffset + size/2;
        while (move != NULL && move->offset < buddyoffset+size/2) {
            struct rom *nextmove = image_next(move);
            moverom(image, insertrom, firstoffset, size/2, move);
            insertrom = move;
            move = nextmove;
        }
    } else {
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
