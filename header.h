#ifndef EMS_HEADER_H
#define EMS_HEADER_H

#include "ems.h"

#define HEADER_SIZE 336
#define HEADER_TITLE_SIZE 16

struct header {
    char title[HEADER_TITLE_SIZE+1];
    ems_size_t romsize;
    enum {
        HEADER_ENH_GBC = 1,
        HEADER_ENH_SGB = 2
    } enhancements;
    int gbc_only;
};

int     header_validate(unsigned char*);
void    header_decode(struct header*, unsigned char*);

#endif /* EMS_HEADER_H */
