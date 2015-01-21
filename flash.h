#ifndef EMS_FLASH_H
#define EMS_FLASH_H

int flash_writef(ems_size_t, ems_size_t, char*);
int flash_move(ems_size_t, ems_size_t, ems_size_t);
int flash_read(int, ems_size_t, ems_size_t);
int flash_write(ems_size_t, ems_size_t, int);
int flash_erase(ems_size_t);

#endif /* EMS_FLASH_H */