PROG = ems-flasher
OBJS = ems.o main.o

CFLAGS  = -g -Wall -Werror
CFLAGS += `pkg-config --cflags libusb-1.0`

all: $(PROG)

$(PROG): $(OBJS)
	$(CC) -o $(PROG) $(OBJS) `pkg-config --libs libusb-1.0`

clean:
	rm -f $(PROG) $(OBJS)
