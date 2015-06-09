#ifndef EMS_FLASH_H
#define EMS_FLASH_H

#include "ems.h"

#define WRITEBLOCKSIZE 32
#define READBLOCKSIZE 4096

enum {FLASH_EUSB = 1, FLASH_EFILE, FLASH_EINTR};

ems_size_t flash_lastofs;

void flash_init(void (*)(int, ems_size_t), int (*)(void));
int flash_writef(ems_size_t, ems_size_t, char*);
int flash_move(ems_size_t, ems_size_t, ems_size_t);
int flash_read(int, ems_size_t, ems_size_t);
int flash_write(ems_size_t, ems_size_t, int);
int flash_erase(ems_size_t);
int flash_delete(ems_size_t, int);

#endif /* EMS_FLASH_H */