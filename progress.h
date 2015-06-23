#ifndef EMS_PROGRESS_H
#define EMS_PROGRESS_H

#include "update.h"

enum {
    PROGRESS_REFRESH = -1,
    PROGRESS_ERASE, PROGRESS_WRITEF, PROGRESS_WRITE, PROGRESS_READ,
    PROGRESS_TYPESNB
};

void progress_newline(void);
void progress_start(struct updates*);
void progress(int, ems_size_t);

#endif /* EMS_PROGRESS_H */
