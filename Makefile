# RAZORFS - Simple Multithreaded Filesystem
# Unified Makefile with debug/release configurations

# Toolchain (supports cross-compilation)
CC ?= gcc
AR ?= ar
STRIP ?= strip
PKG_CONFIG ?= pkg-config

# Dependency checks
ifeq ($(shell $(PKG_CONFIG) --exists fuse3 && echo yes),yes)
    FUSE_CFLAGS = $(shell $(PKG_CONFIG) fuse3 --cflags)
    FUSE_LIBS = $(shell $(PKG_CONFIG) fuse3 --libs)
else
    $(error FUSE3 not found. Install with: sudo apt-get install libfuse3-dev)
endif

ifeq ($(shell $(PKG_CONFIG) --exists zlib && echo yes),yes)
    ZLIB_LIBS = $(shell $(PKG_CONFIG) zlib --libs)
else
    ZLIB_LIBS = -lz
    $(warning zlib pkg-config not found, using -lz)
endif

# Base flags
CFLAGS_BASE = -Wall -Wextra -Werror=implicit-function-declaration -pthread
CFLAGS_BASE += $(FUSE_CFLAGS)
LDFLAGS_BASE = -pthread $(FUSE_LIBS) -lrt $(ZLIB_LIBS)
LDFLAGS = $(LDFLAGS_BASE) $(HARDENING_LDFLAGS)

# Security hardening flags (production-grade)
HARDENING_FLAGS = -fstack-protector-strong -D_FORTIFY_SOURCE=2 -fPIE
HARDENING_LDFLAGS = -Wl,-z,relro,-z,now -Wl,-z,noexecstack -pie

# Additional security flags for extra hardening
EXTRA_HARDENING_FLAGS = -Wformat -Wformat-security -Werror=format-security

# Build configurations
CFLAGS_DEBUG = $(CFLAGS_BASE) -g -O0 $(HARDENING_FLAGS)
CFLAGS_RELEASE = $(CFLAGS_BASE) -O3 -DNDEBUG $(HARDENING_FLAGS) $(EXTRA_HARDENING_FLAGS)

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

.PHONY: all debug release clean help test test-unit test-integration test-static test-valgrind

all: debug

debug:
	@echo "Building RAZORFS (Debug)..."
	@$(MAKE) $(TARGET) CFLAGS="$(CFLAGS_DEBUG)"

release:
	@echo "Building RAZORFS (Release - Optimized)..."
	@$(MAKE) $(TARGET) CFLAGS="$(CFLAGS_RELEASE)"

hardened:
	@echo "Building RAZORFS (Hardened Release - Security Optimized)..."
	@$(MAKE) clean
	@$(MAKE) $(TARGET) CFLAGS="$(CFLAGS_RELEASE)"
	@$(STRIP) $(TARGET)
	@echo "✅ Hardened build complete (stripped symbols)"
	@echo "Security features:"
	@command -v checksec >/dev/null 2>&1 && checksec --file=$(TARGET) || echo "  (install checksec to verify security features)"

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

# Test targets
test: test-unit test-integration

test-unit:
	@echo "Running unit tests..."
	@./run_tests.sh --unit-only

test-integration:
	@echo "Running integration tests..."
	@./run_tests.sh --no-static --no-dynamic

test-static:
	@echo "Running static analysis..."
	@./run_tests.sh --unit-only --no-dynamic

test-valgrind:
	@echo "Running valgrind memory checks..."
	@./run_tests.sh --unit-only --no-static

test-all:
	@echo "Running complete test suite..."
	@./run_tests.sh

test-coverage:
	@echo "Running tests with coverage..."
	@./run_tests.sh --coverage

help:
	@echo "RAZORFS Makefile"
	@echo ""
	@echo "Build Targets:"
	@echo "  make          - Build razorfs (debug mode, default)"
	@echo "  make debug    - Build with debug symbols (-g -O0)"
	@echo "  make release  - Build optimized version (-O3)"
	@echo "  make hardened - Build hardened release (Full RELRO, PIE, stack canary, stripped)"
	@echo "  make clean    - Remove build artifacts"
	@echo ""
	@echo "Security Features (always enabled):"
	@echo "  -fstack-protector-strong  - Stack canary protection"
	@echo "  -D_FORTIFY_SOURCE=2       - Fortified libc functions"
	@echo "  -fPIE -pie                - Position Independent Executable"
	@echo "  -Wl,-z,relro,-z,now       - Full RELRO (immediate binding)"
	@echo "  -Wl,-z,noexecstack        - Non-executable stack"
	@echo "  -Wformat-security         - Format string vulnerability detection"
	@echo ""
	@echo "Cross-Compilation Support:"
	@echo "  make CC=aarch64-linux-gnu-gcc    - Cross-compile for ARM64"
	@echo "  make CC=arm-linux-gnueabihf-gcc  - Cross-compile for ARM32"
	@echo ""
	@echo "Test Targets:"
	@echo "  make test              - Run unit and integration tests"
	@echo "  make test-unit         - Run unit tests only"
	@echo "  make test-integration  - Run integration tests only"
	@echo "  make test-static       - Run static analysis (cppcheck, clang)"
	@echo "  make test-valgrind     - Run valgrind memory checks"
	@echo "  make test-all          - Run complete test suite"
	@echo "  make test-coverage     - Run tests with code coverage"
	@echo ""
	@echo "Usage:"
	@echo "  mkdir -p /tmp/razorfs_mount"
	@echo "  ./razorfs /tmp/razorfs_mount"