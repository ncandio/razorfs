# RAZORFS Multithreaded FUSE Makefile
# Simple build without optimizations for stability testing

CC = gcc
CFLAGS = -Wall -Wextra -g -pthread $(shell pkg-config fuse3 --cflags)
LDFLAGS = -pthread $(shell pkg-config fuse3 --libs)

# Source files - only what we need
SRC_DIR = ../src
SOURCES = $(SRC_DIR)/nary_tree_mt.c $(SRC_DIR)/string_table.c
OBJECTS = $(SOURCES:.c=.o)

# Target
TARGET = razorfs_mt

.PHONY: all clean

all: $(TARGET)

$(TARGET): razorfs_mt.c $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(TARGET) $(OBJECTS)

test: $(TARGET)
	@echo "Building RAZORFS multithreaded FUSE..."
	@echo "Mount with: mkdir -p /tmp/razorfs_mount && ./$(TARGET) /tmp/razorfs_mount"
