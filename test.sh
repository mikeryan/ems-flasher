#!/bin/sh

# Tests insert.awk and update.awk
#
# Stdin and stdout: like insert.awk
#
# Environment variables:
#    AWK: used to invoke AWK. Will use "awk" if empty.
#    PAGESIZE: test image size (in bytes). 4 MB if empty.

set -e

tmpd=$(mktemp -d)
trap 'rm -rf "$tmpd"' EXIT
trap 'exit 1' INT TERM QUIT

if [ -z "$AWK" ]; then
    AWK="awk"
fi

if [ -z "$PAGESIZE" ]; then
    PAGESIZE=$((4<<20))
fi

"$AWK" -vtmpd="$tmpd" '
    BEGIN {FS = "\t"}
    $2 != "" {print >> (tmpd "/image")}
    $2 == "" {print >> (tmpd "/new")}
'

"$AWK" -f validateimage.awk "$tmpd/image"

"$AWK" -vPAGESIZE=$PAGESIZE -f insert.awk "$tmpd/image" "$tmpd/new" \
    > "$tmpd/insert"
./validateinsert.sh "$tmpd/image" "$tmpd/new" "$tmpd/insert"
if [ "$(tail -1 "$tmpd/insert" | cut -f4)" -ge $PAGESIZE ]; then
    echo "insert.awk did not respect PAGESIZE" >&2
    exit 1
fi

"$AWK" -f update.awk "$tmpd/insert" > "$tmpd/update"
"$AWK" -f validateupdate.awk "$tmpd/image" "$tmpd/update" \
    > "$tmpd/validateupdate"
cmp "$tmpd/insert" "$tmpd/validateupdate"

cat "$tmpd/insert"
