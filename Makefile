# RAZORFS - Simple Multithreaded Filesystem
# Unified Makefile with debug/release configurations

CC = gcc
CFLAGS_BASE = -Wall -Wextra -pthread
CFLAGS_BASE += $(shell pkg-config fuse3 --cflags)
LDFLAGS = -pthread $(shell pkg-config fuse3 --libs) -lrt -lz

# Build configurations
CFLAGS_DEBUG = $(CFLAGS_BASE) -g -O0
CFLAGS_RELEASE = $(CFLAGS_BASE) -O3 -DNDEBUG

# Default to debug build
CFLAGS ?= $(CFLAGS_DEBUG)

# Source files (automatic discovery)
SRC_DIR = src
FUSE_DIR = fuse
SRC_FILES = $(wildcard $(SRC_DIR)/*.c)
OBJECTS = $(SRC_FILES:.c=.o)
FUSE_SRC = $(FUSE_DIR)/razorfs_mt.c

# Target
TARGET = razorfs

.PHONY: all debug release clean help

all: debug

debug:
	@echo "Building RAZORFS (Debug)..."
	@$(MAKE) $(TARGET) CFLAGS="$(CFLAGS_DEBUG)"

release:
	@echo "Building RAZORFS (Release - Optimized)..."
	@$(MAKE) $(TARGET) CFLAGS="$(CFLAGS_RELEASE)"

$(TARGET): $(OBJECTS) $(FUSE_DIR)/razorfs_mt.c
	@echo "Building RAZORFS..."
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
	@echo "✅ Build complete: $(TARGET)"

$(SRC_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	@echo "Cleaning..."
	rm -f $(OBJECTS) $(TARGET) $(FUSE_DIR)/razorfs_mt
	@echo "✅ Clean complete"

help:
	@echo "RAZORFS Makefile"
	@echo ""
	@echo "Targets:"
	@echo "  make         - Build razorfs (debug mode, default)"
	@echo "  make debug   - Build with debug symbols (-g -O0)"
	@echo "  make release - Build optimized version (-O3)"
	@echo "  make clean   - Remove build artifacts"
	@echo "  make help    - Show this help"
	@echo ""
	@echo "Usage:"
	@echo "  mkdir -p /tmp/razorfs_mount"
	@echo "  ./razorfs /tmp/razorfs_mount"