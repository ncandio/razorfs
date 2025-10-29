# razorfsck Makefile

CC = gcc
CFLAGS = -Wall -Wextra -I../../src -g -O2
LDFLAGS = -lpthread -lrt -lz

SRCS = razorfsck.c
OBJS = $(SRCS:.c=.o)
TARGET = razorfsck

# Link with RAZORFS object files
RAZORFS_OBJS = ../../src/nary_tree_mt.o ../../src/string_table.o \
               ../../src/shm_persist.o ../../src/numa_support.o \
               ../../src/compression.o ../../src/wal.o ../../src/recovery.o

all: $(TARGET)

$(TARGET): $(OBJS) $(RAZORFS_OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

install: $(TARGET)
	install -m 755 $(TARGET) /usr/local/bin/

.PHONY: all clean install
