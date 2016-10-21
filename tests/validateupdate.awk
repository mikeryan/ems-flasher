#!/usr/bin/awk -f

# Recreate the target image from the initial image and update instructions.
# The result is yet to be compared to the output of test-insertupdate.c.

BEGIN {
    FS = OFS = "\t"
    BLOCKSIZE = 128*1024
    PAGESIZE = 4*1024*1024
    lastoffset = 0
    input = "image"
    for (i = 0; i <= 8; i++)
        validsizes[32768 * 2^i] = 1
}

/^[a-z]/ {
    input = "updates"
}

input == "image" {
    id=$1; offset=$2; size=$3

    roms[offset, "id"] = id
    roms[offset, "origoffset"] = offset
    roms[offset, "size"] = size
    image[offset] = offset
}

input == "updates" {
    cmd = $1; dest = $2; size = $3; src = $4

    if (cmd == "writef") {
        write(src, dest, size, "")
    } else if (cmd == "move") {
        read(id, src, size)
        write(id, dest, size, roms[src, "origoffset"])
        delete image[src]
    } else if (cmd == "read") {
        read(id, src, size)
        if (dest < 0 || dest > 2)
            error("bad slot address")
        if ((dest, "id") in slot)
            error("slot already in use")
        slot[dest, "id"] = id
        slot[dest, "origoffset"] = src
        slot[dest, "size"] = size
    } else if (cmd == "write") {
        if (src < 0 || src > 2)
            error("bad slot address")
        if (!((src, "id") in slot))
            error("slot not assigned")
        if (slot[src, "size"] != size || slot[src, "id"] != id)
            error("slot does not match")
        if (slot[src, "origoffset"] != "" &&
            block(slot[src, "origoffset"]) != block(dest)) {
                error("trying to \"write\" a small ROM on a different block")
        }
        write(id, dest, size, slot[src, "origoffset"])
        delete slot[src, "id"]
    } else if (cmd == "erase") {
        eraseblock(dest)
    } else {
        error("bad command")
    }
}

END {
    for (offset = 0; offset < PAGESIZE; offset += 32768) {
        if (offset in image) {
            print roms[offset, "id"], roms[offset, "origoffset"],
                roms[offset, "size"], offset
        }
    }
}

function error(msg) {
    print msg > "/dev/stderr"
    exit 1
}

function block(offset) {
    return int(offset/BLOCKSIZE)
}

function write(id, offset, size, origoffset) {
    if (!validsizes[size])
        error("invalid size")

    if (offset%size != 0)
        error("offset not aligned to the size")

    if (offset < lastoffset)
        error("offset < lastoffset")

    if (offset + size > PAGESIZE)
        error("offset + size > PAGESIZE")

    if (size < BLOCKSIZE) {
        if (offset%BLOCKSIZE != 0) {
            if (eblockoffset == "" || block(eblockoffset) != block(offset) ||
                offset < eblockoffset) {
                    error("writing to non erased area")
            }
            eblockoffset = offset + size
        } else {
            eraseblock(offset)
        }
    }

    if (offset in image)
        error("destination not empty" offset "-" id "-" roms[offset, "id"])

    roms[offset, "id"] = id
    roms[offset, "origoffset"] = origoffset
    roms[offset, "size"] = size
    image[offset] = offset

    lastoffset = offset + size
}

function read(id, offset, size) {
    if (!(offset in image) || roms[offset, "size"] != size ||
        roms[offset, "origoffset"] != offset ||
        roms[offset, "id"] != id) {
            error("source does not match or does not exist (id="id")")
    }
    if (offset < lastoffset)
        error("offset < lastoffset (id="id")")
}

function eraseblock(blockoffset,    offset) {
    if (blockoffset%BLOCKSIZE != 0)
        error("eraseblock")

    if (blockoffset < lastoffset)
        error("blockoffset < lastoffset (blockoffset="blockoffset" offset="offset")")

    eblockoffset = blockoffset + 64

    for (offset in image) {
        offset = offset+0

        if (roms[offset, "size"] > BLOCKSIZE &&
            block(blockoffset) >= block(offset) && 
            block(blockoffset) <= block(offset+roms[offset, "size"]-1)) {
                error("trying to erase a block belonging to an existing ROM")
        }

        if (block(offset) == block(blockoffset))
            delete image[offset]
    }
}
