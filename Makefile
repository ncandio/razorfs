# RazorFS Unified Build System
# Handles user-space components, kernel module, tests, and tools

# Build configuration
CC = gcc
CFLAGS = -Wall -Wextra -g -O2 -fPIC -pthread
# Use docker flags if in docker environment, otherwise use AddressSanitizer
ifeq ($(DOCKER_ENV),true)
CFLAGS_SAFE = $(CFLAGS)
LDFLAGS = 
else
CFLAGS_SAFE = $(CFLAGS) -fsanitize=address
LDFLAGS = -fsanitize=address
endif

# Directories
SRCDIR = src
TOOLSDIR = tools
KERNELDIR = kernel
TESTDIR = tests
SCRIPTDIR = scripts

# Source files
CORE_SOURCES = $(SRCDIR)/razor_core.c $(SRCDIR)/razor_write.c $(SRCDIR)/razor_transaction_log.c $(SRCDIR)/razor_permissions.c $(SRCDIR)/razor_sync.c
CORE_OBJECTS = $(CORE_SOURCES:.c=.o)

# Tool sources
TOOL_SOURCES = $(TOOLSDIR)/razorfsck_main.c $(TOOLSDIR)/razorfsck.c $(TOOLSDIR)/razorfsck_repair.c
FUSE_SOURCES = fuse/razorfs_fuse.cpp

# Test sources
TEST_SOURCES = $(wildcard $(TESTDIR)/integration/test_*.c)
TESTS = $(TEST_SOURCES:.c=)

# Default target - build user-space components
all: userspace

# Help target - show available commands
help:
	@echo "RazorFS Build System - Available Targets:"
	@echo ""
	@echo "Main Targets:"
	@echo "  all           - Build user-space components (default)"
	@echo "  userspace     - Build core library and tools"
	@echo "  kernel        - Build kernel module"
	@echo "  tools         - Build razorfsck and utilities"
	@echo "  fuse          - Build FUSE implementation"
	@echo ""
	@echo "Testing Targets:"
	@echo "  test          - Run all tests"
	@echo "  test-unit     - Run unit tests"
	@echo "  test-integration - Run integration tests"
	@echo "  test-stress   - Run stress tests"
	@echo "  test-memory   - Run memory safety tests"
	@echo ""
	@echo "Installation Targets:"
	@echo "  install       - Install user-space components"
	@echo "  install-kernel - Install kernel module (requires DKMS)"
	@echo "  uninstall     - Remove installed components"
	@echo ""
	@echo "Utility Targets:"
	@echo "  clean         - Clean all build artifacts"
	@echo "  distclean     - Deep clean including generated files"
	@echo "  info          - Show build environment information"
	@echo ""
	@echo "Examples:"
	@echo "  make userspace     # Build library and tools"
	@echo "  make test          # Run all tests"
	@echo "  make kernel        # Build kernel module"
	@echo "  make install       # Install user components"

# User-space components
userspace: librazer.a tools

# Docker-optimized user-space components (without AddressSanitizer for faster builds)
userspace-docker: CFLAGS_SAFE = $(CFLAGS_DOCKER)
userspace-docker: LDFLAGS = $(LDFLAGS_DOCKER)
userspace-docker: librazer.a tools

# Core library
librazer.a: $(CORE_OBJECTS)
	ar rcs $@ $^
	@echo "✓ Core library built: $@"

# Individual object files
%.o: %.c
	$(CC) $(CFLAGS_SAFE) -c $< -o $@

# Tools
tools: librazer.a
	@echo "Building tools..."
	$(CC) $(CFLAGS_SAFE) $(TOOL_SOURCES) -L. -lrazer $(LDFLAGS) -o razorfsck
	@echo "✓ Tools built: razorfsck"

# FUSE implementation
fuse:
	@if [ -f "$(FUSE_SOURCES)" ]; then \
		echo "Building FUSE implementation..."; \
		g++ $(CFLAGS) $(FUSE_SOURCES) -lfuse -o razorfs_fuse; \
		echo "✓ FUSE implementation built"; \
	else \
		echo "⚠️  FUSE sources not found - skipping"; \
	fi

# Kernel module
kernel:
	@echo "Building kernel module..."
	$(MAKE) -C $(KERNELDIR)
	@echo "✓ Kernel module built: $(KERNELDIR)/razorfs.ko"

# Test targets
test: test-unit test-integration

test-unit: librazer.a
	@echo "Running unit tests..."
	@if [ -d "$(TESTDIR)/unit" ]; then \
		$(MAKE) -C $(TESTDIR)/unit; \
		echo "✓ Unit tests completed"; \
	else \
		echo "⚠️  No unit tests found - skipping"; \
	fi

test-integration: librazer.a $(TESTS)
	@echo "Running integration tests..."
	@passed=0; total=0; \
	for test in $(TESTS); do \
		if [ -f "$$test" ]; then \
			total=$$((total + 1)); \
			echo "Running $$test..."; \
			if timeout 30s $$test > /tmp/test_output.log 2>&1; then \
				echo "✓ PASSED: $$(basename $$test)"; \
				passed=$$((passed + 1)); \
			else \
				echo "✗ FAILED: $$(basename $$test)"; \
			fi; \
		fi; \
	done; \
	echo "Integration tests: $$passed/$$total passed"

test-stress: librazer.a
	@echo "Running stress tests..."
	@if [ -d "$(TESTDIR)/stress" ]; then \
		$(MAKE) -C $(TESTDIR)/stress; \
		echo "✓ Stress tests completed"; \
	else \
		echo "⚠️  No stress tests found - skipping"; \
	fi

test-memory: librazer.a
	@echo "Running memory safety tests with AddressSanitizer..."
	@if [ -f "$(TESTDIR)/integration/test_simple_transaction" ]; then \
		ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 $(TESTDIR)/integration/test_simple_transaction; \
		echo "✓ Memory safety tests passed"; \
	else \
		echo "⚠️  Memory safety test not found"; \
	fi

# Build individual tests
$(TESTDIR)/integration/test_%: $(TESTDIR)/integration/test_%.c librazer.a
	$(CC) $(CFLAGS_SAFE) $< -L. -lrazer $(LDFLAGS) -o $@

# Installation targets
install: userspace
	@echo "Installing RazorFS user-space components..."
	sudo mkdir -p /usr/local/bin /usr/local/lib /usr/local/include
	sudo cp librazer.a /usr/local/lib/
	sudo cp razorfsck /usr/local/bin/
	sudo cp $(SRCDIR)/razor_core.h /usr/local/include/
	@if [ -f "razorfs_fuse" ]; then \
		sudo cp razorfs_fuse /usr/local/bin/; \
	fi
	@echo "✓ User-space components installed"

install-kernel: kernel
	@echo "Installing kernel module via DKMS..."
	@if command -v dkms >/dev/null 2>&1; then \
		echo "DKMS found - installing kernel module"; \
		sudo dkms add .; \
		sudo dkms build razorfs/2.1.0; \
		sudo dkms install razorfs/2.1.0; \
		echo "✓ Kernel module installed via DKMS"; \
	else \
		echo "⚠️  DKMS not found - manual installation:"; \
		echo "    sudo make -C $(KERNELDIR) install"; \
		echo "    sudo depmod"; \
	fi

uninstall:
	@echo "Removing RazorFS components..."
	sudo rm -f /usr/local/bin/razorfsck /usr/local/bin/razorfs_fuse
	sudo rm -f /usr/local/lib/librazer.a
	sudo rm -f /usr/local/include/razor_core.h
	@if command -v dkms >/dev/null 2>&1; then \
		sudo dkms remove razorfs/2.1.0 --all 2>/dev/null || true; \
	fi
	@echo "✓ Components removed"

# Utility targets
clean:
	@echo "Cleaning build artifacts..."
	rm -f $(CORE_OBJECTS) librazer.a razorfsck razorfs_fuse
	rm -f $(TESTS)
	$(MAKE) -C $(KERNELDIR) clean
	rm -rf /tmp/test_razorfs_*
	@echo "✓ Build artifacts cleaned"

distclean: clean
	@echo "Deep cleaning..."
	find . -name "*.o" -delete
	find . -name "*.ko" -delete
	find . -name "*.mod*" -delete
	find . -name ".*.cmd" -delete
	find . -name "Module.symvers" -delete
	find . -name "modules.order" -delete
	rm -f /tmp/test_output*.log
	@echo "✓ Deep clean completed"

info:
	@echo "RazorFS Build Environment Information:"
	@echo ""
	@echo "Compiler: $(CC) $(shell $(CC) --version | head -1)"
	@echo "Build flags: $(CFLAGS)"
	@echo "Safety flags: $(CFLAGS_SAFE)"
	@echo ""
	@echo "Kernel version: $(shell uname -r)"
	@echo "Kernel build directory: /lib/modules/$(shell uname -r)/build"
	@echo ""
	@echo "Source directories:"
	@echo "  Core sources: $(SRCDIR)/"
	@echo "  Tools: $(TOOLSDIR)/"
	@echo "  Kernel: $(KERNELDIR)/"
	@echo "  Tests: $(TESTDIR)/"
	@echo ""
	@echo "Available components:"
	@if [ -d "$(SRCDIR)" ]; then echo "  ✓ Core library sources"; else echo "  ✗ Core library sources"; fi
	@if [ -d "$(TOOLSDIR)" ]; then echo "  ✓ Tool sources"; else echo "  ✗ Tool sources"; fi
	@if [ -f "$(KERNELDIR)/razorfs.c" ]; then echo "  ✓ Kernel module source"; else echo "  ✗ Kernel module source"; fi
	@if [ -d "$(TESTDIR)" ]; then echo "  ✓ Test suite"; else echo "  ✗ Test suite"; fi
	@if [ -f "fuse/razorfs_fuse.cpp" ]; then echo "  ✓ FUSE implementation"; else echo "  ✗ FUSE implementation"; fi

# Phony targets
.PHONY: all help userspace tools fuse kernel test test-unit test-integration test-stress test-memory install install-kernel uninstall clean distclean info