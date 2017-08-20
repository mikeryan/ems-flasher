CFLAGS = -g -std=c99 -pedantic -Wall

ALL = test-flash1 test-flash2 test-flash3 test-flash4 test-updates test-insertupdate

all: $(ALL)

FLASH1_OBJS = test-flash1.o test.o common.o ../flash.o
test-flash1: $(FLASH1_OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(FLASH1_OBJS)

FLASH2_OBJS = test-flash2.o test.o common.o ../flash.o
test-flash2: $(FLASH2_OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(FLASH2_OBJS)

FLASH3_OBJS = test-flash3.o test.o common.o ../flash.o
test-flash3: $(FLASH3_OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(FLASH3_OBJS)

FLASH4_OBJS = test-flash4.o test.o common.o ../flash.o ../progress.o
test-flash4: $(FLASH4_OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(FLASH4_OBJS)

UPDATES_OBJS = test-updates.o test.o common.o ../updates.o
test-updates: $(UPDATES_OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(UPDATES_OBJS)

INSERTUPDATE_OBJS = test-insertupdate.o ../insert.o ../update.o
test-insertupdate: $(INSERTUPDATE_OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(INSERTUPDATE_OBJS)

../flash.o ../progress.o ../updates.o ../insert.o ../update.o:
	@echo '$@ missing. Please build ems-flasher or ems-flasher-file.' >&2
	@exit 1

test: $(ALL)
	prove ./test-flash[1234] ./test-updates ./test-idu.sh 2>/dev/null

clean-tmp:
	@rm -f .tmp_*

clean: clean-tmp
	@rm -f $(ALL) test.o common.o test-flash[1234].o test-updates.o test-insertupdate.o

.SUFFIXES:
.SUFFIXES: .o .c
