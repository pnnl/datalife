# Makefile to compile io_test.c and fio_test.c

CC = gcc
CFLAGS = -Wall -Wextra -O2
TARGETS = io_test fio_test

all: $(TARGETS)

io_test: io_test.c
	$(CC) $(CFLAGS) -o io_test io_test.c

fio_test: fio_test.c
	$(CC) $(CFLAGS) -o fio_test fio_test.c

clean:
	rm -f $(TARGETS)