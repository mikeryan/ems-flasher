ems-flasher
===========

EMS flasher for the _GB USB smart card 64M_, a flash cartridge for GameBoy.

The hardware can be obtained from: http://store.kitsch-bent.com/product/usb-64m-smart-card

Original URL: http://lacklustre.net/gb/ems/


Original about
--------------

The EMS flasher is a simple command line flasher for the 64 Mbit EMS USB
flash cart for Game Boy.

This software was written by Mike Ryan <mikeryan \at lacklustre.net> and others
(see the AUTHORS file)

For more information, see the web site at:
http://lacklustre.net/gb/ems/


Prerequisites
-------------

To build ems-flasher pkg-config and libusb are needed. Additionally, OS
X requires coreutils and gawk.

On OS X, install the prerequisites using:

```
brew install pkg-config libusb coreutils gawk
```

(thanks to hyarion for this info)

On Ubuntu/Debian you can install them by:
```
sudo apt-get install pkg-config libusb-1.0-0-dev
```


Building
--------

```
./config.sh --prefix=/usr
make
sudo make install
```

After running `./config.sh`, make sure the installation path of the binary
(BINDIR), the menu ROMs (DATADIR) and the manual page (MANDIR) suit you. If
not, you must run the tool again by specifying the desired paths. Use the
--help option to get the list of all options. If ems-flasher is ran from the
build directory, it will use the menu ROMs located in the same directory, not
the one selected with `config.sh` so you can use the software without
installing it.

Running
-------

The software has four major modes of operation:
  * write ROM(s) to cart
  * read ROM from cart
  * read title of ROM on cart
  * delete ROMs

To write use --write, to read use --read, and to get the title listing use
--title. You can specify the page (1 or 2) with --bank PAGENB. If no page is
specified, page 1 is assumed.

Write mode will write the ROM files specified on the command line to 
the selected page to the cart. Read mode will read the entirety of the page
(32 megabits / 4 megabytes) into the ROM file specified. Specifying multiple
ROMs will cause all of the ROMs to be written to the specified bank along with a
menu to choose from them. Some ROMs support enhancements of the Super Game Boy
or the Game Boy Color or both. For maximum compatibility, you shouldn't mix ROMs
with different enhancements support on the same page. However, if you don't
intend to use a Super Game Boy or a Game Boy Color, you can mix ROMs that
support, resp., SGB or CGB enhancements with others that don't with the --force
option. This option is not necessary if the page already contains mixed ROMs.

Title mode does not require a file argument, and will print the ROM
title listing to stdout.

The --delete command will delete the ROMs whose bank numbers are specified in
parmeters.
The --format command will delete all the ROMs located on the chosen page.

Additionally, all modes take a --verbose flag for giving more output.
You can also adjust the block size, but it is recommended you leave this
to the default of 4096 bytes for writing and 32 bytes for reading (used
by the Windows software).

For a full list of options, run the command with the --help flag.

The MENUDIR environment variable may used to override the location of the menu
ROMs selected by `config.sh`.

Examples
--------

```
# write the ROM to the cart
./ems-flasher --write totally_legit_rom.gb

# write several ROMs to the cart
./ems-flash --write copyright.gb infringement.gb

# saves the contents of the cart into the file; print some extra info
./ems-flasher --verbose --read not_warez.gb

# print out the title
./ems-flasher --title
```

Bugs
----

In this version, the --read command dumps an entire page. It is not possible to
dump a particular ROM.

Preferably use the bug tracker found at the web site (at the top of this
doc) to report any bugs.

You can also send em to mikeryan \at lacklustre.net


Game Boy Camera
---------------

Extract images from Game Boy Camera saves using
[rgbcdumper](https://github.com/Rombusevil/rgbcdumper)
