#ifndef __EMS_H__
#define __EMS_H__

#include <stdint.h>

int ems_init(void);

int ems_read(int from, uint32_t offset, unsigned char *buf, size_t count);
int ems_write(int to, uint32_t offset, unsigned char *buf, size_t count);

#define FROM_ROM    1
#define FROM_SRAM   2
#define TO_ROM      FROM_ROM
#define TO_SRAM     FROM_SRAM

#endif /* __EMS_H__ */
