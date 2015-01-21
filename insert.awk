#!/usr/bin/awk -f

# Inserts one or more ROMs to the image of a page.
# Defragments the image if necessary. The defragmentation is made incrementally.
#
# Input variables:
#    PAGESIZE: The size of a page in bytes. 4 MB when not specified. Used for
#    testing.
#
# Stdin:
#    Accepts the list of ROMS of the image followed by the list of ROMs to add.
#    There are three tab-separated fields by record: "id", "offset" and "size".
#    The list of new ROMs starts at the first row where "offset" is empty.
#
#    The "id" is mandatory for the list of new ROMs. It may represent the name
#    of a file or an index in an array by example.
#
# Stdout:
#    Records of four tab-separated fields: id, origoffset, size, offset.
#    "id", "origoffset" and "size" are copied as is from the source.
#    "offset" is the offset of the ROM in the resulting image. This program
#    guarantees that ROMs are always moved from higher addresses to lower
#    addresses.

# Data structure
#    An image is represented by a doubly linked list. The ROM entries are
#    indexed by their offset. For convenience, the first element is indexed by
#    the empty string and the last element is indexed by the size of a page.

BEGIN {
    FS=OFS="\t"

    if (PAGESIZE == "")
        PAGESIZE = 4*1024*1024

    insertafter("", "", PAGESIZE)

    input = "image"
    retrynew_count = 0
}

{id = $1; offset = $2; size = $3}

offset == "" {
    input = "new"
}

input == "image" {
    insertafter(last(), id, offset, size, offset)
}

input == "new" {
    if (insert(id, size) == -1)
            retrynew[retrynew_count++] = $0
}

END {
    if (retrynew_count > 0) {
        for (i = 0; i < retrynew_count; i++) {
            split(retrynew[i], F)
            id = F[1]; size = F[3]
            if (insert(id, size) == -1) {
                if (defrag(F[3]) == -1) {
                    print "not enough free space" > "/dev/stderr"
                    exit 1
                }
                insert(id, size)
            }
       }
    }
    dumplist()
}

function dumplist(    cur) {
    for (cur = first(); cur != "" && cur <= last(); cur = roms[cur, "next"]) {
        print roms[cur, "id"], roms[cur, "origoffset"], roms[cur, "size"], cur
    }
}

function insertafter(after, id, offset, size, origoffset) {
    roms[offset, "id"] = id
    roms[offset, "origoffset"] = origoffset
    roms[offset, "size"] = size

    roms[offset, "prev"] = after
    roms[offset, "next"] = roms[after, "next"]
    roms[roms[after, "next"], "prev"] = offset
    roms[after, "next"] = offset
}

function remove(offset) {
    roms[roms[offset, "prev"], "next"] = roms[offset, "next"]
    roms[roms[offset, "next"], "prev"] = roms[offset, "prev"]

    delete roms[offset, "id"]
    delete roms[offset, "origoffset"]            
    delete roms[offset, "size"]
    delete roms[offset, "next"]
    delete roms[offset, "prev"]
}

function first() {
    return roms["", "next"]
}

function last() {
    return roms[PAGESIZE, "prev"]
}

# Inserts a ROM into the image using best fit to limit the fragmentation.
function insert(id, size, origoffset,    cur, offset, possize, posoffset,
    posprev, prev, s){

    # For each space between two ROMS (or between 0 and the first rom or
    # between the last ROM and PAGESIZE):
    for (cur = first(); cur != ""; cur = roms[cur, "next"]) {
        offset = prev + roms[prev, "size"]

        # Consider all free ROM (aligned) locations of this space. Always taking
        # the biggest possible ROM size for each location.
        while (cur-offset > 0) {
            for (s = PAGESIZE; s >= 32768; s/=2) {
                if (offset%s == 0 && cur-offset >= s) {
                    if (s >= size) {
                        if (possize == "" || possize > s) {
                                possize = s
                                posoffset = offset
                                posprev = prev
                        }
                    }
                    break
                }
            }
            offset += s
        }

        prev = cur
    }

    if (possize != "") {
        insertafter(posprev, id, posoffset, size, origoffset)
        return posoffset
    }

    return -1
}

# Incremental defragmenter.
# Move ROMs to make space for a ROM of size bytes and aligned to an offset
# multiple of its size.
#
# Returns -1 in case of error.
#
# Takes advantage of the buddy system.
# Operation:
#   - Find two free spaces for two ROMs of size/2 bytes.
#     If necessary, call this function recursively.
#   - Move the ROMs contained in the buddy of the second free ROM location into
#     the first free ROM location. This will free the buddy of the second free
#     ROM and then create a free location for a ROM of size bytes and aligned
#     correctly.
function defrag(size,    buddyoffset, firstoffset, firstrom, move, nxt,
    resoffset, secondoffset, startrom, temp) {

    if (size == 32768)
        return -1

    if ((firstoffset = insert("**FREE**", size/2)) == -1) {
        defrag(size/2)
        firstoffset = insert("**FREE**", size/2)
        if (firstoffset == -1)
            return -1
    }

    if ((secondoffset = insert("**FREE**", size/2)) == -1) {
        defrag(size/2)
        secondoffset = insert("**FREE**", size/2)
        if (secondoffset == -1) {
            remove(firstoffset);
            return -1
        }
    }

    if (secondoffset < firstoffset) {
        temp = firstoffset
        firstoffset = secondoffset
        secondoffset = temp
    }

    firstrom = roms[firstoffset, "prev"]
    remove(firstoffset)
    prev = roms[secondoffset, "prev"]
    nxt = roms[secondoffset, "next"]
    remove(secondoffset)

    # Remark: It could happen that the two free spaces are buddies. In this case
    # no ROM would be moved.

    # The buddy of the second free space is on its right (higher addresses).
    if (secondoffset % size == 0) {
        # Move the ROMs of the right buddy of the second free space into the
        # first free space
        move = nxt
        buddyoffset = secondoffset + size/2
        while (move < buddyoffset+size/2) {
            insertafter(firstrom, roms[move, "id"],
                firstoffset + move%(size/2),
                roms[move, "size"],
                roms[move, "origoffset"])
            firstrom = roms[firstrom, "next"]
            temp = move
            move = roms[move, "next"]
            remove(temp)
        }
        resoffset = secondoffset
    } else { # The buddy is on the left of the second free space.
        # Move the ROMs of the left buddy of the second free space into the
        # first free space
        move = prev
        buddyoffset = secondoffset - size/2
        while (move >= buddyoffset) {
            insertafter(firstrom, roms[move, "id"],
                firstoffset + move%(size/2),
                roms[move, "size"],
                roms[move, "origoffset"])
            temp = move
            move = roms[move, "prev"]
            remove(temp)
        }
        resoffset = buddyoffset
    }

    return resoffset
}
