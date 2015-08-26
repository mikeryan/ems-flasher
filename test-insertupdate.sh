#!/bin/sh

# Tests insert(), defrag() and update() using test-insertupdate.c.
#
# Stdin and Stdout: like test-insertupdate.c
#
# Environment variables:
#    PAGESIZE: test image size (in bytes). 4 MB if empty.

set -e

tmpd=$(mktemp -d)
trap 'rm -rf "$tmpd"' EXIT
trap 'exit 1' INT TERM QUIT

PAGESIZE=${PAGESIZE:-$((4<<20))}

awk -vtmpd="$tmpd" '
    BEGIN {FS = "\t"}
    $2 != "" {print >> (tmpd "/image")}
    $2 == "" {print >> (tmpd "/new")}
'

awk -f validateimage.awk "$tmpd/image"

cat "$tmpd/image" "$tmpd/new" |
    PAGESIZE=$PAGESIZE ./test-insertupdate > "$tmpd/insert_update"
awk -vtmpd="$tmpd" '
    BEGIN {FS = OFS = "\t"; path = tmpd"/insert"}
    $0 == "" {path = tmpd"/update"; next}
    { print >> path }
' "$tmpd/insert_update"

cut -f1-3 "$tmpd/insert" | sort > "$tmpd"/t1
cat "$tmpd/image" "$tmpd/new" | sort > "$tmpd"/t2
cmp "$tmpd"/t1 "$tmpd"/t2

awk 'BEGIN {FS=OFS="\t"} {print $1, $4, $3}' "$tmpd/insert" |
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
        error("moving a ROM from low addresses to high addresses: " id)
    }
' "$tmpd/insert"

if [ "$(tail -1 "$tmpd/insert" | cut -f4)" -ge $PAGESIZE ]; then
    echo "test_insertupdate did not respect PAGESIZE" >&2
    exit 1
fi

awk -f validateupdate.awk "$tmpd/image" "$tmpd/update" \
    > "$tmpd/validateupdate"
cmp "$tmpd/insert" "$tmpd/validateupdate"

cat "$tmpd/insert"
