obj-m := kbd_irq.o

KDIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

all:
	$(MAKE) -C $(KDIR) M=$$PWD

check:
	@echo "[CPPCHECK]"
	@cppcheck --enable=all --inconclusive --std=posix --std=c99 \
		${obj-m:.o=.c}
	@echo "\n[CHECKPATCH]"
	@${KDIR}/scripts/checkpatch.pl --no-tree -f ${obj-m:.o=.c}
