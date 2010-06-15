PROG = ems
OBJS = ems.o

CFLAGS  = -g
CFLAGS += `pkg-config --cflags libusb-1.0`

all: $(PROG)

$(PROG): $(OBJS)
	$(CC) -o $(PROG) $(OBJS) `pkg-config --libs libusb-1.0`

clean:
	rm -f $(PROG) $(OBJS)
