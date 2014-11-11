ems-flasher
===========

EMS flasher for the _GB USB smart card 64M_, a flash cartridge for GameBoy.

The hardware can be obtained from: http://store.kitsch-bent.com/product/usb-64m-smart-card

Original URL: http://lacklustre.net/gb/ems/


Original about
--------------

The EMS flasher is a simple command line flasher for the 64 Mbit EMS USB
flash cart for Game Boy.

This software was written by Mike Ryan <mikeryan \at lacklustre.net>

For more information, see the web site at:
http://lacklustre.net/gb/ems/


Prerequisites
-------------

To build ems-flasher pkg-config and libusb are needed.

On Mac you can install them by:

```
sudo port install pkgconfig
sudo port install libusb
```

(thanks to hyarion for this info)

On Ubuntu/Debian you can install them by:
```
sudo apt-get install pkg-config libusb-1.0-0-dev
```


Building
--------

Build is automated by a Makefile. To build simply run the following:

```
make
```


Running
-------

The software has three major modes of operation:
  * write ROM to cart
  * read ROM from cart
  * read title of ROM on cart

To write use --write, to read use --read, and to get the title use
--title.

Write mode will write the ROM specified on the command line to bank 1 on
the cart. Read mode will read the entirety of bank 1 (32 megabits / 4
megabytes) into the ROM file specified.

Title mode does not require a file argument, and will print the ROM
title to stdout.

BEWARE: if you give the EMS flasher a huge file for writing, it will
continue writing past the end of the cart and do unknown amounts of
damage. Please don't do this!

Additionally, all modes take a --verbose flag for giving more output.
You can also adjust the block size, but it is recommended you leave this
to the default of 4096 bytes for writing and 32 bytes for reading (used
by the Windows software).

For a full list of options, run the command with the --help flag.

Examples
--------

```
# write the ROM to the cart
./ems-flasher --write totally_legit_rom.gb

# saves the contents of the cart into the file; print some extra info
./ems-flasher --verbose --read not_warez.gb

# print out the title
./ems-flasher --title
```

Bugs
----

The software only handles a single ROM in the first bank of the cart.
Reading and writing of SRAM is not supported (yet). The commands to do
so have been discovered and they will appear in a future version.

Preferably use the bug tracker found at the web site (at the top of this
doc) to report any bugs.

You can also send em to mikeryan \at lacklustre.net


Game Boy Camera
---------------

Extract images from Game Boy Camera saves using
[rgbcdumper](https://github.com/Rombusevil/rgbcdumper)
