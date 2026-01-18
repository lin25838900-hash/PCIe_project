CC ?= gcc
CFLAGS ?= -O2 -Wall -Wextra

all: acq_read

acq_read: acq_read.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f acq_read
