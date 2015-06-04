#ifndef EMS_IMAGE_H
#define EMS_IMAGE_H

#include "ems.h"
#include "queue.h"
#include "header.h"

#define MINROMSIZE 32768

TAILQ_HEAD(image, rom);

struct rom {
    ems_size_t offset;
    ems_size_t romsize;
    struct {
        enum {ROM_SOURCE_FLASH, ROM_SOURCE_FILE} type;
        union {
            ems_size_t origoffset;
            void *fileinfo;
        } u;
    } source;
    struct header header;

    TAILQ_ENTRY(rom) roms;
};

#define image_init(image) TAILQ_INIT(image)
#define image_insert_head(image, rom) TAILQ_INSERT_HEAD(image, rom, roms)
#define image_insert_after(image, after, rom) \
    TAILQ_INSERT_AFTER(image, after, rom, roms)
#define image_insert_tail(image, rom) TAILQ_INSERT_TAIL(image, rom, roms)
#define image_remove(image, rom) TAILQ_REMOVE(image, rom, roms)
#define image_foreach(image, rom) TAILQ_FOREACH(rom, image, roms)
#define image_foreach_safe(image, rom, temprom) \
    TAILQ_FOREACH_SAFE(rom, image, roms, temprom)
#define image_prev(rom) TAILQ_PREV(rom, image, roms)
#define image_next(rom) TAILQ_NEXT(rom, roms)

#endif /* EMS_IMAGE_H */
