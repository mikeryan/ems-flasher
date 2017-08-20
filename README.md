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


Building and Installing
-----------------------

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

On Linux, udev rules ensuring access to users to the USB device without
requiring root privileges will be installed on make install.
Use make install-udevrules to install only the rules, without the sofware.

Running
-------

Please consult the manual (ems-flasher(1)) for usage instructions.
You can read the manual without installing it with: `man -l ./ems-flasher.1`.

Bugs
----

Preferably use the bug tracker on GitHub:
   https://github.com/mikeryan/ems-flasher

You can also send em to mikeryan \at lacklustre.net


Game Boy Camera
---------------

Extract images from Game Boy Camera saves using
[rgbcdumper](https://github.com/Rombusevil/rgbcdumper)
