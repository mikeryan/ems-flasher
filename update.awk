#!/usr/bin/awk -f

# Takes the output of insert.awk and generates I/O instructions to be applied to
# the original image to obtain the new image.
#
# Stdin: see the output of insert.awk
#
# Stdout:
#    A list of I/O instructions. There are five tab-separated fields: cmd, id,
#    dest, size and src. All fields are not used by all commands.
#
#    An offset is expressed in bytes from the beginning of the page.
#    "id" is copied from the source and is mandatory for entries representing
#    a new ROM.
#
#    Commands:
#       writef
#          Write the file of "size" bytes represented by "src" to the offset
#          "dest". "src" is in fact a copy of "id" which may be a filename or
#          an index of an array.
#
#       move
#          Move a ROM of "size" bytes from offset "src" to offset "dest".
#
#       read
#          Read a ROM of "size" bytes located at offset "src" in the temporary
#          location "dest". "dest" is a number between 0 and 2. The ROM size
#          will not be higher than 64 KB.
#
#       write
#          Write a ROM of "size" bytes saved in the temporary location "src" to
#          offset "dest".
#
#       erase
#          Erase the erase-block starting at offset "src". This will writes
#          some bytes (32 or 64) at the beginning of the block. "id" is not set.

BEGIN {
    FS=OFS="\t"
    BLOCKSIZE = 128*1024
    smallroms_count = 0
}

{
    id = $1; origoffset = $2; size = $3; offset = $4

    if (block(lastoffset) != block(offset)) {
        if (blockdirty)
            updatesmallroms()
        smallroms_count = 0
    }

    if (size >= BLOCKSIZE) {
         if (origoffset != offset) {
             if (origoffset == "")
                 print "writef", id, offset, size, id
             else
                 print "move", id, offset, size, origoffset
         }
    } else {
        smallroms[smallroms_count, "id"] = id
        smallroms[smallroms_count, "origoffset"] = origoffset
        smallroms[smallroms_count, "size"] = size
        smallroms[smallroms_count, "offset"] = offset
        smallroms_count++

        if (origoffset != offset)
            blockdirty = 1
    }

    lastoffset = offset 
}

END {
    if (blockdirty)
        updatesmallroms()
}

function block(offset) {
    return int(offset/BLOCKSIZE)
}

function updatesmallroms(    i, slot) {
    slot = 0
    for (i = 0; i < smallroms_count; i++) {
        if (smallroms[i, "origoffset"] != "" &&
            block(smallroms[i, "origoffset"]) == block(smallroms[i, "offset"])) {
                print "read", smallroms[i, "id"], slot++, smallroms[i, "size"],
                    smallroms[i, "origoffset"]
        }
    }

    if (smallroms[0, "offset"]%BLOCKSIZE != 0) {
        print "erase", "",
            smallroms[0, "offset"] - smallroms[0, "offset"]%BLOCKSIZE
    }

    slot = 0
    for (i = 0; i < smallroms_count; i++) {
        if (smallroms[i, "origoffset"] == "") {
            print "writef", smallroms[i, "id"], smallroms[i, "offset"],
                smallroms[i, "size"], smallroms[i, "id"]
        } else {
            if (block(smallroms[i, "origoffset"]) \
                == block(smallroms[i, "offset"])) {
                    print "write", smallroms[i, "id"], smallroms[i, "offset"],
                        smallroms[i, "size"], slot++
            } else {
                print "move", smallroms[i, "id"], smallroms[i, "offset"],
                    smallroms[i, "size"], smallroms[i, "origoffset"]
            }
        }
    }

    blockdirty = 0
    smallroms_count = 0
}
