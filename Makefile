DATADIR=/usr/local/share/$(PROG)
MENUDIR=$(DATADIR)
MANDIR = /usr/share/man

MENUVARS = menu.gb menucs.gb menuc.gb menus.gb

CFLAGS  = -g -Wall -Werror -pedantic -std=c99 \
	-DMENUDIR="\"${MENUDIR}\""

PROG = ems-flasher
OBJS = ems.o main.o header.o cmd.o progress.o flash.o insert.o update.o

PROGEMSFILE = ems-flasher-file
OBJSEMSFILE = ems-file.o main.o header.o cmd.o progress.o flash.o insert.o \
              update.o

PROGTESTINSERT = test-insertupdate
OBJSTESTINSERT = test-insertupdate.o insert.o update.o

all: $(PROG) check-menu

$(PROG)-dev: $(PROG)
	rm -f cmd.o
	make $(PROG) MENUDIR=. DATADIR=.

$(PROGEMSFILE)-dev: $(PROGEMSFILE)
	rm -f cmd.o
	make $(PROGEMSFILE) MENUDIR=. DATADIR=.

$(PROG): $(OBJS)
	$(CC) -o $(PROG) $(OBJS) `pkg-config --libs libusb-1.0`

ems.o: ems.c
	$(CC) $(CFLAGS) `pkg-config --cflags libusb-1.0` -c $<

$(PROGEMSFILE): $(OBJSEMSFILE)
	$(CC) -o $(PROGEMSFILE) $(OBJSEMSFILE)

check-menu: $(MENUVARS)

menu.gb:
	@echo "Please install a menu:"
	@echo "  Type 'make menu-orig' to restore the precompiled menu."
	@echo "  Type 'make menu' to build the default menu from source."
	@exit 1

menucs.gb: menu.gb
	./updateheader.sh -c -s  < menu.gb > $@

menuc.gb: menu.gb
	./updateheader.sh -c < menu.gb > $@

menus.gb: menu.gb
	./updateheader.sh -s < menu.gb > $@

menu-orig:
	cp menu.gb.orig menu.gb
	make check-menu

menu: FORCE
	@if ! test -e menu/Makefile; then \
		echo "It seems that the menu submodule is missing."; \
		echo "Please type 'git submodule init &&" \
		     " git submodule update'."; \
		exit 1; \
	fi
	cd menu && make
	cp menu/menu.gb .
	rgbfix -v -t "MENU#" -l 0x33 -k "01" menu.gb
	make check-menu

$(PROGTESTINSERT): $(OBJSTESTINSERT)
	$(CC) -o $(PROGTESTINSERT) $(OBJSTESTINSERT)

test-insert: FORCE $(PROGTESTINSERT)
	./tests.sh

install: $(PROG) check-menu
	install ems-flasher /usr/local/bin
	mkdir -p "${DATADIR}"
	install menu.gb menucs.gb menuc.gb menus.gb "${DATADIR}"
	install ems-flasher.1 $(MANDIR)/man1/

clean:
	rm -f $(PROG) $(OBJS) $(PROGEMSFILE) $(OBJSEMSFILE)
	rm -f $(PROGTESTINSERT) $(OBJSTESTINSERT)

clean-menu:
	rm -f $(MENUVARS)
	cd menu && make clean

clean-all: clean clean-menu

.SUFFIXES: .c .o

FORCE:
