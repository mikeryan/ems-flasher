#!/bin/sh

# Validate the header of a ROM passed through stdin (update the logo and the
# header checksum) and enable the GBC and/or the SGB enhancements.
# Output the ROM with the updated header on stdout.
#
# Used in the Makefile when generating the menu files.

while getopts "cs" opt; do
    case $opt in
    c)  color=1;;
    s)  super=1;;
    esac
done

# need to do some magic to make this portable to OS X and Linux
AWK="awk"
which gawk > /dev/null
if [[ $? == 0 ]]; then
    AWK="gawk"
fi

OD="od"
which god > /dev/null
if [[ $? == 0 ]]; then
    OD="god"
fi

$OD -v -Ad -tu1 -w1 | LC_ALL=C $AWK -vcolor=$color -vsuper=$super '
BEGIN {
    split( \
        " 206 237 102 102 204  13   0  11   3 115   0 131   0  12   0  13" \
        "  0   8  17  31 136 137   0  14 220 204 110 230 221 221 217 153" \
        " 187 187 103  99 110  14 236 204 221 220 153 159 187 185  51  62",
        logo)
}

$2 == "" {next}

{ out = $2 }

# 0x104 to 0x133 - Update the logo
$1 >= 260 && $1 <= 307 {
    out = logo[$1-259]
}

# 0x144: Update the GBC flag
color && $1 == 323 {
    if (out < 128)
        out += 128
}

# 0x146: Update the SGB field
super && $1 == 326 {
    out = 3
}

# 0x14b: Update Old license code field for SGB
super && $1 == 331 {
    out = 51
}

# 0x14d - Update the checksum
$1 == 333 {
    out = chksum
}

# 0x134 to 0x14c - Compute the checksum of the new header (must be after all
# rules modifying "out")
$1 >= 308 && $1 <= 332 {
    chksum = chksum - out -1
    if (chksum < 0)
        chksum += 256
}

{ printf "%c", out }
'
