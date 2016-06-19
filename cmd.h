#ifndef EMS_CMD_H
#define EMS_CMD_H

#include "ems.h"

void cmd_title(int);
void cmd_delete(int, int, int, char**);
void cmd_format(int, int);
void cmd_write(int, int, int, int, char**);

#endif /* EMS_CMD_H */
