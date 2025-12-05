CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -Wpedantic -D_POSIX_C_SOURCE=200809L
LDFLAGS = -pthread
TARGET = server
SRC = server.c

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC) $(LDFLAGS)

clean:
	rm -f $(TARGET)
