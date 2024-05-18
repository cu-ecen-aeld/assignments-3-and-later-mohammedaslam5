
CC=

SRCS := $(wildcard *.c)
OUTFILES := $(SRCS:%.c=%)

all: ${OUTFILES}

%: %.c
	$(CC) $< -o $@

clean:
	@echo "Cleaning up...";
	rm -rvf *.o ${OUTFILES}

