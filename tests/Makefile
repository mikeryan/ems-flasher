CFLAGS = -g -std=c99 -pedantic -Wall

ALL = test-flash1 test-flash2 test-flash3 test-flash4 test-updates test-insertupdate

all: $(ALL)

../flash.o ../progress.o ../updates.o ../insert.o ../update.o:
	@echo '$@ missing. Please build ems-flasher or ems-flasher-file.' >&2
	@exit 1

test-flash1 test-flash2 test-flash3 test-flash4 test-updates: test.o common.o
test-updates: ../updates.o
test-flash1 test-flash2 test-flash3 test-flash4: ../flash.o
test-flash4: ../progress.o
test-insertupdate: ../insert.o ../update.o

test: $(ALL)
	prove ./test-flash[1234] ./test-updates ./test-idu.sh 2>/dev/null

clean-tmp:
	@rm -f .tmp_*

clean: clean-tmp
	@rm -f $(ALL) test.o common.o test-flash[1234].o test-updates.o test-insertupdate.o

.SUFFIXES:
.SUFFIXES: .o .c
