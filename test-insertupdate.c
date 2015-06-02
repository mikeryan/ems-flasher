#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <err.h>

#include "ems.h"
#include "image.h"
#include "insert.h"
#include "update.h"

void dumpimage(struct image *);

int
main(int argc, char **argv) {
    struct image image;
    struct updates *updates;
    char line[128];
    int linen;
    char *s;

    if ((s = getenv("PAGESIZE")) != NULL)
        (void)sscanf(s, "%"SCNuEMSSIZE, &insert_pagesize);

    image_init(&image);

    for (linen = 1; fgets(line, sizeof(line), stdin) != NULL; linen++) {
        struct rom *rom;
        char *token, *path;
        ems_size_t offset, size;

        if ((token = strtok(line, "\t")) == NULL) token = "";
        if (line[0] != '\t')
            path = token;
        else {
            offset = atol(token);
            path = NULL;
        }

        size = atol((token = strtok(NULL, "\t")) != NULL ? token : "");

        if ((rom = malloc(sizeof(struct rom))) == NULL)
            err(1, "malloc");

        if (path == NULL) {
            rom->offset = offset;
            rom->romsize = size;
            rom->source.type = ROM_SOURCE_FLASH;
            rom->source.u.origoffset = offset;
            image_insert_tail(&image, rom); 
        } else {
            rom->romsize = size;
            rom->source.type = ROM_SOURCE_FILE;
            if ((rom->source.u.fileinfo = malloc(strlen(path)+1)) == NULL)
                err(1, "malloc");
            strcpy(rom->source.u.fileinfo, path);
            if (image_insert_defrag(&image, rom))
                errx(1, "insert_defrag() failed for "
                    "linen = %d path=%s size=%"PRIuEMSSIZE, linen, path, size);
        }
    }
    if (ferror(stdin))
        err(1, "fgets");

    dumpimage(&image);
    putchar('\n');
    if (image_update(&image, &updates))
        errx(1, "update");

    {
    struct update *u;
    updates_foreach(updates, u) {
        switch (u->cmd) {
        case UPDATE_CMD_WRITEF:
            printf("writef\t%"PRIuEMSSIZE"\t%"PRIuEMSSIZE"\t%s\n",
                u->update_writef_dstofs,
                u->update_writef_size,
                u->update_writef_fileinfo);
            break;
        case UPDATE_CMD_MOVE:
            printf("move\t%"PRIuEMSSIZE"\t%"PRIuEMSSIZE"\t%"PRIuEMSSIZE"\n",
                u->update_move_dstofs,
                u->update_move_size,
                u->update_move_srcofs);
            break; 
        case UPDATE_CMD_WRITE:
            printf("write\t%"PRIuEMSSIZE"\t%"PRIuEMSSIZE"\t%d\n",
                u->update_write_dstofs,
                u->update_write_size,
                u->update_write_srcslot);
            break;
        case UPDATE_CMD_READ:
            printf("read\t%d\t%"PRIuEMSSIZE"\t%"PRIuEMSSIZE"\n",
                u->update_read_dstslot,
                u->update_read_size,
                u->update_read_srcofs);
            break;
        case UPDATE_CMD_ERASE:
            printf("erase\t%"PRIuEMSSIZE"\n",
                u->update_erase_dstofs);
            break;
        }
    }
    }
}

void
dumpimage(struct image *image) {
    struct rom *rom;

    image_foreach(image, rom) {
        if (rom->source.type == ROM_SOURCE_FLASH)
            printf("\t%"PRIuEMSSIZE"\t", rom->source.u.origoffset);
        else
            printf("%s\t\t", rom->source.u.fileinfo);
        printf("%"PRIuEMSSIZE"\t%"PRIuEMSSIZE"\n", rom->romsize, rom->offset);
    }
}
