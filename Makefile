AWK=/usr/bin/awk
LIBEXECDIR=/usr/local/lib/$(PROG)
DATADIR=/usr/local/share/$(PROG)
MENUDIR=$(DATADIR)

PROG = ems-flasher
OBJS = ems.o main.o header.o cmd.o flash.o

PROGEMSFILE = ems-flasher-file
OBJSEMSFILE = ems-file.o main.o header.o cmd.o flash.o

MANDIR = /usr/share/man

CFLAGS  = -g -Wall -Werror -pedantic -std=c99 \
	-DAWK="\"${AWK}\"" \
	-DLIBEXECDIR="\"${LIBEXECDIR}\"" \
	-DMENUDIR="\"${MENUDIR}\""

all: $(PROG) menu

$(PROG): $(OBJS)
	$(CC) -o $(PROG) $(OBJS) `pkg-config --libs libusb-1.0`

ems.o: ems.c
	$(CC) $(CFLAGS) `pkg-config --cflags libusb-1.0` -c $<

$(PROGEMSFILE): $(OBJSEMSFILE)
	$(CC) -o $(PROGEMSFILE) $(OBJSEMSFILE)

menu: FORCE
	cd menu && make
	cp menu/menu.gb .
	cp menu.gb menucs.gb
	rgbfix -c -s -l 0x33 -v menucs.gb 
	cp menu.gb menuc.gb
	rgbfix -c -v menuc.gb
	cp menu.gb menus.gb
	rgbfix -s -l 0x33 -v menus.gb

install: $(PROG) menu
	install ems-flasher /usr/local/bin
	mkdir -p "${LIBEXECDIR}"
	install insert.awk update.awk "${LIBEXECDIR}"
	mkdir -p "${DATADIR}"
	install menu.gb menucs.gb menuc.gb menus.gb "${DATADIR}"
	install ems-flasher.1 $(MANDIR)/man1/

clean:
	rm -f $(PROG) $(OBJS) $(PROGEMSFILE) $(OBJSEMSFILE)
	cd menu && make clean

.SUFFIXES: .c .o

FORCE:
