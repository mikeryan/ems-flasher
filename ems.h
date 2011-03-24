#ifndef __EMS_H__
#define __EMS_H__

#include <stdint.h>

int ems_init(void);

int ems_read(uint32_t offset, unsigned char *buf, size_t count);
int ems_write(uint32_t offset, unsigned char *buf, size_t count);

#endif /* __EMS_H__ */
