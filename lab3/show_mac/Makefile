SOURCES := main.c eeprom.c

obj-m := e1000_show_mac.o
e1000_show_mac-objs := ${SOURCES:.c=.o}

KDIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)


all:
	$(MAKE) -C $(KDIR) M=$$PWD

check:
	@echo "[CPPCHECK]"
	@cppcheck --enable=all --inconclusive --std=posix --std=c99 ${SOURCES}
	@echo "\n[CHECKPATCH]"
	@${KDIR}/scripts/checkpatch.pl --no-tree -f ${SOURCES}
