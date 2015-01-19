AWK=/usr/bin/awk
LIBEXECDIR=/usr/local/lib/$(PROG)

PROG = ems-flasher
OBJS = ems.o main.o header.o cmd.o flash.o

MANDIR = /usr/share/man

CFLAGS  = -g -Wall -Werror -pedantic -std=c99 \
	-DAWK="\"${AWK}\"" -DLIBEXECDIR="\"${LIBEXECDIR}\""

all: $(PROG)

$(PROG): $(OBJS)
	$(CC) -o $(PROG) $(OBJS) `pkg-config --libs libusb-1.0`

install: $(PROG)
	install ems-flasher /usr/local/bin
	mkdir -p "${LIBEXECDIR}"
	install insert.awk update.awk "${LIBEXECDIR}"
	install ems-flasher.1 $(MANDIR)/man1/

clean:
	rm -f $(PROG) $(OBJS)

.c.o:
	$(CC) $(CFLAGS) `pkg-config --cflags libusb-1.0` -c $<

.SUFFIXES: .c .o
