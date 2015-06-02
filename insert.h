#ifndef EMS_INSERT_H
#define EMS_INSERT_H

#include "ems.h"
#include "image.h"

ems_size_t insert_pagesize;

int image_insert(struct image*, struct rom*);
int image_insert_defrag(struct image*, struct rom*);
int image_defrag(struct image*, ems_size_t);

#endif /* EMS_INSERT_H */
