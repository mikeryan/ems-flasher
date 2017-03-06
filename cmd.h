#ifndef EMS_CMD_H
#define EMS_CMD_H

#include <time.h>
#include "ems.h"

struct romfile {
    struct header header;
    char *path;
    time_t ctime;
};

int checkint();
void catchint();
void restoreint();
void cmd_title(int);
void cmd_delete(int, int, int, char**);
void cmd_format(int, int);
void cmd_write(int, int, int, int, char**);

#endif /* EMS_CMD_H */
