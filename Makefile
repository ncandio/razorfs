# RAZORFS - Simple Multithreaded Filesystem
# Unified Makefile - No optimizations, maximum simplicity

CC = gcc
CFLAGS = -Wall -Wextra -g -pthread
CFLAGS += $(shell pkg-config fuse3 --cflags)
LDFLAGS = -pthread $(shell pkg-config fuse3 --libs) -lrt -lz

# Source files
SRC_DIR = src
FUSE_DIR = fuse
SOURCES = $(SRC_DIR)/nary_tree_mt.c $(SRC_DIR)/string_table.c $(SRC_DIR)/shm_persist.c $(SRC_DIR)/numa_support.c $(SRC_DIR)/compression.c $(FUSE_DIR)/razorfs_mt.c
OBJECTS = $(SRC_DIR)/nary_tree_mt.o $(SRC_DIR)/string_table.o $(SRC_DIR)/shm_persist.o $(SRC_DIR)/numa_support.o $(SRC_DIR)/compression.o

# Target
TARGET = razorfs

.PHONY: all clean help

all: $(TARGET)

$(TARGET): $(OBJECTS) $(FUSE_DIR)/razorfs_mt.c
	@echo "Building RAZORFS..."
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
	@echo "✅ Build complete: $(TARGET)"

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	@echo "Cleaning..."
	rm -f $(OBJECTS) $(TARGET) $(FUSE_DIR)/razorfs_mt
	@echo "✅ Clean complete"

help:
	@echo "RAZORFS Makefile"
	@echo ""
	@echo "Targets:"
	@echo "  make       - Build razorfs"
	@echo "  make clean - Remove build artifacts"
	@echo "  make help  - Show this help"
	@echo ""
	@echo "Usage:"
	@echo "  mkdir -p /tmp/razorfs_mount"
	@echo "  ./razorfs /tmp/razorfs_mount"