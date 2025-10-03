#!/bin/bash
# Simple FUSE multithreading test script

set -e

MOUNT_POINT="/tmp/razorfs_mount"
TEST_DIR="$MOUNT_POINT/test"

echo "=== RAZORFS MT FUSE Test ==="
echo

echo "1. Create test directory..."
mkdir -p "$TEST_DIR"
ls -ld "$TEST_DIR"

echo
echo "2. Create 10 files..."
for i in {1..10}; do
    echo "File content $i" > "$TEST_DIR/file_$i.txt"
done
ls -l "$TEST_DIR"

echo
echo "3. Read files..."
for i in {1..10}; do
    content=$(cat "$TEST_DIR/file_$i.txt" 2>/dev/null || echo "EMPTY")
    echo "File $i: $content"
done

echo
echo "4. Create subdirectories..."
mkdir -p "$TEST_DIR/subdir1/subdir2"
ls -lR "$TEST_DIR"

echo
echo "5. Test file operations..."
echo "original" > "$TEST_DIR/modify_test.txt"
echo "modified" >> "$TEST_DIR/modify_test.txt"
cat "$TEST_DIR/modify_test.txt"

echo
echo "6. Delete operations..."
rm "$TEST_DIR/file_1.txt"
rmdir "$TEST_DIR/subdir1/subdir2"
ls -l "$TEST_DIR"

echo
echo "✅ Basic FUSE operations complete"
