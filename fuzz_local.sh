#!/bin/bash
# Local AFL++ Fuzzing Script for RAZORFS

set -e

echo "=== RAZORFS Local Fuzzing Setup ==="

# Check if running as root (needed for core_pattern)
if [ "$EUID" -ne 0 ]; then 
    echo "Please run with sudo for core_pattern configuration"
    echo "Usage: sudo ./fuzz_local.sh"
    exit 1
fi

# Configure system for AFL
echo "Configuring system for AFL fuzzing..."
echo core > /proc/sys/kernel/core_pattern
echo performance | tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor 2>/dev/null || true

# Check if AFL++ is installed
if ! command -v afl-fuzz &> /dev/null; then
    echo "AFL++ not found. Installing..."
    apt-get update
    apt-get install -y afl++
fi

# Create fuzz directory
FUZZ_DIR="fuzz-build"
rm -rf "$FUZZ_DIR"
mkdir -p "$FUZZ_DIR"
cd "$FUZZ_DIR"

echo "Building fuzzing target..."

# Create path fuzzer
cat > path_fuzzer.c << 'FUZZER_CODE'
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "../src/nary_tree_mt.h"

int main(int argc, char **argv) {
    struct nary_tree_mt tree;
    nary_tree_mt_init(&tree);
    
    if (argc > 1) {
        // Fuzz path lookup function for security issues
        uint16_t result = nary_path_lookup_mt(&tree, argv[1]);
        (void)result; // Suppress unused warning
    }
    
    nary_tree_mt_destroy(&tree);
    return 0;
}
FUZZER_CODE

# Compile with AFL instrumentation
echo "Compiling with AFL instrumentation..."
afl-clang-fast -D_FILE_OFFSET_BITS=64 \
    -I../src \
    -I/usr/include/fuse3 \
    path_fuzzer.c \
    ../src/nary_tree_mt.c \
    ../src/string_table.c \
    -o path_fuzzer \
    -lpthread -lz -lfuse3

# Create input corpus
echo "Creating input corpus..."
mkdir -p input_corpus
echo "/test" > input_corpus/test1
echo "/test/../etc/passwd" > input_corpus/test2
echo "/././test" > input_corpus/test3
echo "/test/../../../" > input_corpus/test4
echo "" > input_corpus/empty
echo "../../../../../../../../../../../../etc/passwd" > input_corpus/test5
echo "/home/user/documents/file.txt" > input_corpus/normal_path
echo "//multiple///slashes" > input_corpus/slashes
echo "/with\nnewline" > input_corpus/newline
echo "/very/deep/nested/path/with/many/components/to/test/depth" > input_corpus/deep

echo ""
echo "=== Fuzzing Setup Complete ==="
echo ""
echo "Starting AFL fuzzer..."
echo "Press Ctrl+C to stop fuzzing"
echo ""
echo "Results will be saved to: $(pwd)/findings/"
echo ""

# Run AFL fuzzer
afl-fuzz -i input_corpus -o findings -m none ./path_fuzzer @@

