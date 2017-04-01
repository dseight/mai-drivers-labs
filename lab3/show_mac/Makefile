obj-m := e1000_show_mac.o
e1000_show_mac-objs := main.o eeprom.o

KDIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

all:
	$(MAKE) -C $(KDIR) M=$$PWD

check:
	@echo "[CPPCHECK]"
	@cppcheck --enable=all --inconclusive --std=posix --std=c99 \
		${e1000_show_mac-objs:.o=.c}
	@echo "\n[CHECKPATCH]"
	@${KDIR}/scripts/checkpatch.pl --no-tree -f ${e1000_show_mac-objs:.o=.c}
