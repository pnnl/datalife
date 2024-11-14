# Makefile to compile io_test.c

CC = gcc
CFLAGS = -Wall -Wextra -O2
TARGET = io_test

all: $(TARGET)

$(TARGET): io_test.c
	$(CC) $(CFLAGS) -o $(TARGET) io_test.c

clean:
	rm -f $(TARGET)