#ifndef EMS_HEADER_H
#define EMS_HEADER_H

#include "ems.h"

#define HEADER_SIZE 336
#define HEADER_TITLE_SIZE 16

/*
 * struct header is a selection of decoded fields of a ROM header.
 *   title: title as it would be displayed by the menu software (only Nintendo
 *          ASCII characters). Invalid characters are replaced by a space.
 *          Trailing spaces are ignored.
 *   romsize: size of the ROM. May not match the real size. Equals 0 if the
 *            field contained an incorrect size code.
 *   enhancements: GBC, SGB, both or none
 *   gbc_only: if true, the header is marked Game Boy Color Only. This fact is
 *             not enforced by the hardware. 
 */

struct header {
    char title[HEADER_TITLE_SIZE+1];
    ems_size_t romsize;
    enum {
        HEADER_ENH_GBC = 1,
        HEADER_ENH_SGB = 2,
        HEADER_ENH_ALL = HEADER_ENH_GBC | HEADER_ENH_SGB
    } enhancements;
    int gbc_only;
};

int     header_validate(unsigned char*);
void    header_decode(struct header*, unsigned char*);

#endif /* EMS_HEADER_H */
