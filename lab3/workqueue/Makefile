CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -g
LIBS = -lpthread

OBJS = main.o workqueue.o

all: main

main: $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o main $(LIBS)

.c.o:
	$(CC) $(CFLAGS) -c $< -o $@

check:
	@echo "[CPPCHECK]"
	@cppcheck --enable=all --inconclusive --std=posix --std=c99 ${OBJS:.o=.c}
	@echo "\n[CHECKPATCH]"
	@/lib/modules/$(shell uname -r)/build/scripts/checkpatch.pl \
		--no-tree -f ${OBJS:.o=.c}

clean:
	rm -f $(OBJS) main
