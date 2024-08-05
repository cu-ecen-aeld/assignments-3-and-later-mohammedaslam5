CC ?= $(CROSS_COMPILE)gcc
CFLAGS ?= -g -Wall -Werror
TARGET ?= aesdsocket
LDFLAGS ?= -lpthread -lrt

SRCS := $(wildcard *.c)
OUTFILES := $(SRCS:%.c=%)

all: ${OUTFILES}

%: %.c
	$(CC) $(CFLAGS) $^ -o $@ $(INCLUDES) $(LDFLAGS)

clean:
	@echo "Cleaning up...";
	rm -rvf *.o ${OUTFILES}
