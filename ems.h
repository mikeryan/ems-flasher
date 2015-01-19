#ifndef __EMS_H__
#define __EMS_H__

#include <stddef.h>
#include <stdint.h>
#include <inttypes.h>

typedef uint_least32_t ems_size_t;
#define PRIuEMSSIZE PRIuLEAST32

int ems_init(void);

int ems_read(int from, uint32_t offset, unsigned char *buf, size_t count);
int ems_write(int to, uint32_t offset, unsigned char *buf, size_t count);

#define FROM_ROM    1
#define FROM_SRAM   2
#define TO_ROM      FROM_ROM
#define TO_SRAM     FROM_SRAM

#define PAGESIZE    ((ems_size_t)4<<20)
#define ERASEBLOCKSIZE ((ems_size_t)128<<10)

#endif /* __EMS_H__ */
