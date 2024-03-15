
CROSS_COMPILE=

SRCS := $(wildcard *.c)
OUTFILES := $(SRCS:%.c=%)

all: ${OUTFILES}

%: %.c
	$(CROSS_COMPILE)gcc $< -o $@

clean:
	@echo "Cleaning up...";
	rm -rvf *.o ${OUTFILES}

