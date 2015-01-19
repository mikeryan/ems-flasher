#!/bin/sh

# Tests
#
# Currently only tests the defragmentation algorithm in insert.awk
#
# Environment variables:
#    AWK: used to invoke AWK. Will use "awk" if empty.

set -e

trap 'rm -rf "$tmpd"' EXIT
trap 'exit 1' TERM QUIT INT

tmpd=$(mktemp -d)

if [ -z "$AWK" ]; then
    AWK=awk
fi

# Generate a ROM listing from an image string
# $1: the image string
# Output the list on stdout
strtolist() {
    "$AWK" -vimage=$1 '
    BEGIN {
        OFS = "\t"
        id = offset = 0
        for (i = 1; i <= length(image); i++) {
            size = 32768 * 2^substr(image, i, 1)
            if (substr(image, i+1, 1) != "*")
                print id++, offset, size
            else
                i++
            offset += size
        }
    }
    '
}

# Test procedure for the defragmentation algorithm: for each ROM size big enough
# to trigger the defragmenter, insert a ROM of this size to the given image.
# The test is skipped if the image is not fragmented
#
# $1: the image string
# $2: the page size
#
# Temporary files (in $tmpd): image, insert, new, validateupdate
testdefrag() {
    local image PAGESIZE freerommaxsize freetotal
    image=$1
    PAGESIZE=$2

    strtolist $image > "$tmpd/image"

    # Determine the total free space and the size of the biggest free ROM
    # location
    eval $(
        (cat "$tmpd/image"; printf "\t%d\t0\n" $PAGESIZE) | 
            "$AWK" '
            BEGIN {
                FS="\t"
                lastoffset = freerommaxsize = 0
            }

            {
                id = $1; offset = $2; size = $3
                freetotal += offset - lastoffset
                while (lastoffset < offset) {
                    for (s = 4*1024*1024; s >= 32768; s /= 2) {
                        if (lastoffset % s == 0 && offset-lastoffset >= s) {
                            if (s > freerommaxsize)
                                freerommaxsize = s
                            lastoffset += s
                            break
                        }
                    }
                }
                lastoffset += size
            }

            END {
                print "freetotal=" freetotal
                print "freerommaxsize=" freerommaxsize
            }
            '
    )

    if [ $freetotal -eq 0 ]; then
        continue
    fi

    (
    trap '
        if [ $? -ne 0 ]; then
            (echo "TEST FAILED"; cat "$tmpd/image" "$tmpd/new") >&2
        fi
    ' EXIT

    size=$((freerommaxsize*2))
    while [ $size -le $freetotal ]; do
        echo "Testing defrag() with $image and a ROM of $((size>>10)) KB" >&2
        printf "NEW\t\t$size\n" > "$tmpd/new"
        cat "$tmpd/image" "$tmpd/new" |
            PAGESIZE=$PAGESIZE AWK=$AWK ./test.sh >/dev/null
        size=$((size*2))
    done
    )
}

### Test the defrag function of insert.awk ###

# Generate all possible images of 256 KB. The output is a list of strings. Each
# string represents an image as a sequence of digits and "*". The first digit
# is a ROM location at offset 0. "0" is a 32 KB ROM. "1" is a 64 KB ROM, ...
# When a digit is followed by an asterisk, it designates a free ROM.
awk -vPAGESIZE=$((256<<10)) '
    BEGIN {
        ROMMAXSIZE = PAGESIZE
        genimage("", 0)
    }

    function genimage(image, lastoffset,    size, sizecode) {
        if (lastoffset == PAGESIZE) {
                print image
                return
        }
        for (size = 32768; size <= ROMMAXSIZE; size *= 2) {
            if (lastoffset%size == 0) {
                sizecode = int(log(size)/log(2))-15
                genimage(image sizecode, lastoffset + size)
                if (size == 32768)
                    genimage(image sizecode "*", lastoffset + size)
            }
        }
    }
' > "$tmpd/images"

while read image; do
   testdefrag $image $((256<<10))
done < "$tmpd/images"

### Test an image of 4 MB filled with a ROM of 32KB every 64 KB ###
i=0 image=
while [ $i -lt 64 ]; do
    image=$image'00*'
    i=$((i+1))
done

testdefrag $image $((4<<20))

echo "All tests passed"
