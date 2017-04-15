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
--title. You can specify the page (1 or 2) with --page PAGENB. If no page is
specified, page 1 is assumed.

Write mode will write the ROM files specified on the command line to the
selected page to the cart. Specifying multiple ROMs will cause all of the ROMs
to be written to the specified page along with a menu to choose from them.
If you intend to use the card on a Super Game Boy or a Game Boy Color, you
should isolate, resp., the Super Game Boy ROMs or the Game Boy Color ROMS on a
same page and put the other ROMs on the other page. E.g., if you own a Game Boy
Color, you could put all the Game Boy Color ROMs in page 1 and the Super Game
Boy and Classic Game Boy ROMs in the other. Use --force to write ROMs from
different models on the same page. This option is not necessary if the page
already contains mixed ROMs.

Read mode will read the ROMs specified on the command line from the specified
page into files. ROMs are identified by the number of their first bank as
printed by the --title command.

It is also possible to backup an entire flash page or the Save RAM to a file
with the --dump command. The command --restore can be used to restore an image
taken with --dump back to the card. If these commands must apply on the Save
RAM, use the --save option or end the filename by ".sav". Note that the Save
RAM is not paged.

Title mode does not require a file argument, and will print the ROM title
listing to stdout.

The --delete command will delete the ROMs whose bank numbers are specified in
parameters.
The --format command will delete all the ROMs located on the chosen page.

Additionally, all modes take a --verbose flag for giving more output.

For a full list of options, run the command with the --help flag.

The MENUDIR environment variable may used to override the location of the menu
ROMs selected by `config.sh`.

Examples
--------

```
# write the ROM to the cart
./ems-flasher --write totally_legit_rom.gb

# write several ROMs to the cart
./ems-flasher --write copyright.gb infringement.gb

# saves the contents of the cart into the file; print some extra info
./ems-flasher --verbose --dump not_warez.gb

# print out the title
./ems-flasher --title
```

Bugs
----

Preferably use the bug tracker found at the web site (at the top of this
doc) to report any bugs.

You can also send em to mikeryan \at lacklustre.net


Game Boy Camera
---------------

Extract images from Game Boy Camera saves using
[rgbcdumper](https://github.com/Rombusevil/rgbcdumper)
