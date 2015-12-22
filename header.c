/* decode GB ROM header */

#include <stdint.h>
#include <string.h>

#include "header.h"

//offsets to parts of the cart header
enum headeroffsets {
    HEADER_LOGO = 0x104,
    HEADER_TITLE = 0x134,
    HEADER_CGBFLAG = 0x143,
    HEADER_SGBFLAG = 0x146,
    HEADER_ROMSIZE = 0x148,
    HEADER_RAMSIZE = 0x149,
    HEADER_REGION = 0x14A,
    HEADER_OLDLICENSEE = 0x14B,
    HEADER_ROMVER = 0x14C,
    HEADER_CHKSUM = 0x14D,
};

const unsigned char nintylogo[0x30] = {
    0xCE, 0xED, 0x66, 0x66, 0xCC, 0x0D, 0x00, 0x0B,
    0x03, 0x73, 0x00, 0x83, 0x00, 0x0C, 0x00, 0x0D,
    0x00, 0x08, 0x11, 0x1F, 0x88, 0x89, 0x00, 0x0E,
    0xDC, 0xCC, 0x6E, 0xE6, 0xDD, 0xDD, 0xD9, 0x99,
    0xBB, 0xBB, 0x67, 0x63, 0x6E, 0x0E, 0xEC, 0xCC,
    0xDD, 0xDC, 0x99, 0x9F, 0xBB, 0xB9, 0x33, 0x3E
};

/**
 * Check if a ROM header is valid.
 *
 * Returns 0 if the header is valid.
 * 
 */
int
header_validate(unsigned char *header) {
    uint8_t calculated_chk;

    if (memcmp(&header[HEADER_LOGO], &nintylogo, sizeof(nintylogo)) != 0)
        return 1;

    calculated_chk = 0;
    for (int i = HEADER_TITLE; i < HEADER_CHKSUM; i++) {
        calculated_chk -= header[i] + 1;
    }
    if (calculated_chk != header[HEADER_CHKSUM])
        return 1;

    return 0;
}

/**
 * Decode fields of a ROM header
 *
 * See header.h for the data format.
 */
void
header_decode(struct header *header, unsigned char *raw) {
    int i;

    for (i = 0; i < HEADER_TITLE_SIZE; i++) {
        int c = raw[HEADER_TITLE + i];
        header->title[i] = (c >= 32 && c <= 126)?c:32;
    }

    for (i = HEADER_TITLE_SIZE-1; i >= 0; i--)
        if (header->title[i] != 32)
            break;
    header->title[i+1] = '\0';

    switch (raw[HEADER_ROMSIZE]) {
        case 0:
        case 1:
        case 2:
        case 3:
        case 4:
        case 5:
        case 6:
        case 7:
        case 8:
            header->romsize = (ems_size_t)32768 << raw[HEADER_ROMSIZE];
            break;
        case 0x52:
            header->romsize = (ems_size_t)1152 << 10;
            break;
        case 0x53:
            header->romsize = (ems_size_t)1280 << 10;
            break;
        case 0x54:
            header->romsize = (ems_size_t)1536 << 10;
            break;
        default:
            header->romsize = 0;
            break;
    }

    header->enhancements = 0;
    if (raw[HEADER_CGBFLAG] & 0x80)
        header->enhancements |= HEADER_ENH_GBC;
    if (raw[HEADER_SGBFLAG] == 0x03 && raw[HEADER_OLDLICENSEE] == 0x33)
        header->enhancements |= HEADER_ENH_SGB;
    header->gbc_only = (raw[HEADER_CGBFLAG] & 0xc0) == 0xc0;
}
