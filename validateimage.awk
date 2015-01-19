#!/usr/bin/awk -f

# Validate the format of an image as accepted by insert.awk

BEGIN {
    FS="\t"
    PAGESIZE=4*1024*1024

    for (i = 0; i <= 8; i++)
        validsizes[32768 * 2^i] = 1
}

function error(msg) {
    printf "%s (id=%s, offset=%s, size=%s, lastoffset=%d)\n", msg, id, offset,
        size, lastoffset > "/dev/stderr"
    exit 1
}

{
    id=$1; offset=$2; size=$3

    if (NF != 3)
        error("invalid number of fields")

    if (offset !~ /^[0-9]+$/ || size !~ /^[0-9]+$/)
        error("invalid number format")

    if (!(size in validsizes))
        error("invalid size")

    if (offset%size != 0)
        error("offset not aligned to the size")

    if (offset < lastoffset)
        error("offset not in order or two ROMs are intertwined.")

    if (offset+size > PAGESIZE)
        error("out of space")

    lastoffset = offset + size
}
