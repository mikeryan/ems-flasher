PROG = ems-flasher
OBJS = ems.o main.o

MANDIR = /usr/share/man

CFLAGS  = -g -Wall -Werror -pedantic -std=c99
CFLAGS += `pkg-config --cflags libusb-1.0`

all: $(PROG)

$(PROG): $(OBJS)
	$(CC) -o $(PROG) $(OBJS) `pkg-config --libs libusb-1.0`

install: $(PROG)
	install ems-flasher /usr/local/bin
	install ems-flasher.1 $(MANDIR)/man1/

clean:
	rm -f $(PROG) $(OBJS)
