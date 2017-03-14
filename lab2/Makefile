SRC := pipe-shmipe.c
obj-m := $(SRC:.c=.o)
KDIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

all:
	$(MAKE) -C $(KDIR) M=$$PWD

check:
	@echo "[CPPCHECK]"
	@cppcheck --enable=all --inconclusive --std=posix --std=c99 ${SRC}
	@echo "\n[CHECKPATCH]"
	@${KDIR}/scripts/checkpatch.pl --no-tree -f ${SRC}
