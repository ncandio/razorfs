# RAZORFS - Simple Multithreaded Filesystem
# Unified Makefile with debug/release configurations

# Toolchain (supports cross-compilation)
CC ?= gcc
AR ?= ar
STRIP ?= strip
PKG_CONFIG ?= pkg-config

# AWS SDK Configuration
AWS_SDK_AVAILABLE := $(shell pkg-config --exists aws-sdk-cpp-s3 && echo YES)

# Conditionally check for FUSE3, unless cleaning or asking for help
ifneq ($(filter $(MAKECMDGOALS),clean help install-aws-sdk),)
    FUSE_CFLAGS =
    FUSE_LIBS =
else
    ifeq ($(shell $(PKG_CONFIG) --exists fuse3 && echo yes),yes)
        FUSE_CFLAGS = $(shell $(PKG_CONFIG) fuse3 --cflags)
        FUSE_LIBS = $(shell $(PKG_CONFIG) fuse3 --libs)
    else
        $(error FUSE3 not found. Install with: sudo apt-get install libfuse3-dev)
    endif
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

# AWS SDK Libraries (if available)
ifeq ($(AWS_SDK_AVAILABLE),YES)
    AWS_LIBS = $(shell pkg-config aws-sdk-cpp-s3 --libs)
    AWS_CFLAGS = $(shell pkg-config aws-sdk-cpp-s3 --cflags)
    CFLAGS_BASE += -DHAS_AWS_SDK
else
    AWS_LIBS = 
    AWS_CFLAGS = 
    $(info AWS SDK not found - S3 integration will be disabled)
endif

# Security hardening flags (production-grade)
HARDENING_FLAGS = -fstack-protector-strong -D_FORTIFY_SOURCE=2 -fPIE
HARDENING_LDFLAGS = -Wl,-z,relro,-z,now -Wl,-z,noexecstack -pie

# Additional security flags for extra hardening
EXTRA_HARDENING_FLAGS = -Wformat -Wformat-security -Werror=format-security

# Build configurations
CFLAGS_DEBUG = $(CFLAGS_BASE) $(AWS_CFLAGS) -g -O0 $(HARDENING_FLAGS)
CFLAGS_RELEASE = $(CFLAGS_BASE) $(AWS_CFLAGS) -O3 -DNDEBUG $(HARDENING_FLAGS) $(EXTRA_HARDENING_FLAGS)

# Default to debug build
CFLAGS ?= $(CFLAGS_DEBUG)

# Source files (automatic discovery)
SRC_DIR = src
FUSE_DIR = fuse
SRC_FILES = $(wildcard $(SRC_DIR)/*.c)
# Exclude S3 backend from main build (it's optional)
MAIN_SRC_FILES = $(filter-out $(SRC_DIR)/s3_backend.c, $(SRC_FILES))
S3_OBJECT = $(SRC_DIR)/s3_backend.o
OBJECTS = $(MAIN_SRC_FILES:.c=.o)
FUSE_SRC = $(FUSE_DIR)/razorfs_mt.c

# Target
TARGET = razorfs
TEST_S3_TARGET = test_s3_backend

.PHONY: all debug release clean help test install-aws-sdk

all: debug

debug:
	@echo "Building RAZORFS (Debug)..."
	@$(MAKE) $(TARGET) CFLAGS="$(CFLAGS_DEBUG)" LDFLAGS="$(LDFLAGS_BASE) $(AWS_LIBS) $(HARDENING_LDFLAGS)"

release:
	@echo "Building RAZORFS (Release - Optimized)..."
	@$(MAKE) $(TARGET) CFLAGS="$(CFLAGS_RELEASE)" LDFLAGS="$(LDFLAGS_BASE) $(AWS_LIBS) $(HARDENING_LDFLAGS)"

hardened:
	@echo "Building RAZORFS (Hardened Release - Security Optimized)..."
	@$(MAKE) clean
	@$(MAKE) $(TARGET) CFLAGS="$(CFLAGS_RELEASE)" LDFLAGS="$(LDFLAGS_BASE) $(AWS_LIBS) $(HARDENING_LDFLAGS)"
	@$(STRIP) $(TARGET)
	@echo "✅ Hardened build complete (stripped symbols)"
	@echo "Security features:"
	@command -v checksec >/dev/null 2>&1 && checksec --file=$(TARGET) || echo "  (install checksec to verify security features)"

$(TARGET): $(OBJECTS) $(FUSE_DIR)/razorfs_mt.c
	@echo "Building RAZORFS..."
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
	@echo "✅ Build complete: $(TARGET)"

# S3-enabled build target
$(TARGET)_s3: $(OBJECTS) $(S3_OBJECT) $(FUSE_DIR)/razorfs_mt.c
	@echo "Building RAZORFS with S3 support..."
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(AWS_LIBS)
	@echo "✅ S3-enabled build complete: $(TARGET)_s3"

$(TEST_S3_TARGET): $(OBJECTS) $(S3_OBJECT) test_s3_backend.c
	@echo "Building S3 Backend Test..."
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(AWS_LIBS)
	@echo "✅ S3 Backend Test build complete: $(TEST_S3_TARGET)"

$(SRC_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	@echo "Cleaning..."
	rm -f $(OBJECTS) $(TARGET) $(TEST_S3_TARGET) $(FUSE_DIR)/razorfs_mt
	@echo "✅ Clean complete"

# Install AWS SDK
install-aws-sdk:
	@echo "Installing AWS SDK dependencies..."
	sudo apt-get update
	sudo apt-get install -y libssl-dev libcurl4-openssl-dev libexpat1-dev cmake
	cd /tmp && git clone https://github.com/aws/aws-sdk-cpp.git
	cd /tmp/aws-sdk-cpp && mkdir build && cd build
	cmake .. -DBUILD_ONLY="s3" -DCUSTOM_MEMORY_MANAGEMENT=OFF
	make -j$(nproc)
	sudo make install
	sudo ldconfig
	@echo "✅ AWS SDK installed"

test:
	@echo "Running comprehensive test suite..."
	@./scripts/run_all_tests.sh

test-unit:
	@echo "Running unit tests only..."
	@./scripts/testing/run_tests.sh --unit-only

test-integration:
	@echo "Running integration tests only..."
	@./scripts/testing/run_tests.sh --integration-only

test-static:
	@echo "Running static analysis..."
	@./scripts/testing/run_tests.sh --unit-only --no-dynamic

test-valgrind:
	@echo "Running valgrind memory checks..."
	@./scripts/testing/run_tests.sh --unit-only --no-static

test-all:
	@echo "Running complete test suite..."
	@./scripts/testing/run_tests.sh

test-coverage:
	@echo "Running tests with code coverage..."
	@./scripts/testing/run_tests.sh --coverage

help:
	@echo "RAZORFS Makefile"
	@echo ""
	@echo "Build Targets:"
	@echo "  make          - Build razorfs (debug mode, default)"
	@echo "  make debug    - Build with debug symbols (-g -O0)"
	@echo "  make release  - Build optimized version (-O3)"
	@echo "  make hardened - Build hardened release (Full RELRO, PIE, stack canary, stripped)"
	@echo "  make clean    - Remove build artifacts"
	@echo "  make install-aws-sdk - Install AWS SDK for S3 integration"
	@echo "  make test_s3_backend - Build S3 backend test program"
	@echo ""
	@echo "AWS SDK Status: $(if $(AWS_SDK_AVAILABLE),✅ Available,⚠️  Not installed)"
	@echo ""
	@echo "Security Features (always enabled):"
	@echo "  -fstack-protector-strong  - Stack canary protection"
	@echo "  -D_FORTIFY_SOURCE=2       - Fortified libc functions"
	@echo "  -fPIE -pie                - Position Independent Executable"
	@echo "  -Wl,-z,relro,-z,now       - Full RELRO (immediate binding)"
	@echo "  -Wl,-z,noexecstack        - Non-executable stack"
	@echo "  -Wformat-security         - Format string vulnerability detection"
	@echo ""
	@echo "Cross-Compilation Support (Datacenter Architectures):"
	@echo "  make CC=aarch64-linux-gnu-gcc         - ARM64 (AWS Graviton, Ampere Altra)"
	@echo "  make CC=arm-linux-gnueabihf-gcc       - ARM32 (legacy ARM servers)"
	@echo "  make CC=powerpc64le-linux-gnu-gcc     - PowerPC64LE (IBM POWER9/POWER10)"
	@echo "  make CC=riscv64-linux-gnu-gcc         - RISC-V 64-bit (emerging datacenter)"
	@echo "  make CC=x86_64-linux-gnu-gcc          - x86_64 (Intel Xeon, AMD EPYC)"
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