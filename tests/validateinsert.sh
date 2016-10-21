#!/bin/sh

# Validate the output of insert.awk
# Takes three parameters: the initial image, the listing of the new ROMs and
# the output of insert.awk.

set -e

trap 'rm -rf "$tmpd"' EXIT
trap 'exit 1' TERM QUIT INT

tmpd=$(mktemp -d)

imagef=$1 newf=$2 insertf=$3

cut -f1-3 "$insertf" | sort > "$tmpd"/t1
cat "$imagef" "$newf" | sort > "$tmpd"/t2
cmp "$tmpd"/t1 "$tmpd"/t2

awk 'BEGIN {FS=OFS="\t"} {print $1, $4, $3}' "$insertf" |
    awk -f validateimage.awk

awk '
    BEGIN {FS="\t"}

    function error(msg) {
        printf FILENAME": line %s: %s\n", NR, msg > "/dev/stderr"
        exit 1
    }

    { id=$1; origoffset=$2; size=$3; newoffset=$4 }

    NF != 4 {
        error("invalid number of fields" NF)
    }

    newoffset !~ /^[0-9]+$/ {
        error("newoffset: invalid number format")
    }

    origoffset != "" && origoffset < newoffset {
        error("moving a ROM from left to right: " id)
    }
' "$insertf"
