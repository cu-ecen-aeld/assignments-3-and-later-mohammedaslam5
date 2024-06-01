CC=
CFLAGS += -g -Wall -Werror

SRCS := $(wildcard *.c)
OUTFILES := $(SRCS:%.c=%)

all: ${OUTFILES}

%: %.c
	$(CC) $< -o $@ $(CFLAGS)

clean:
	@echo "Cleaning up...";
	rm -rvf *.o ${OUTFILES}
