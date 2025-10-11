#!/bin/bash
# Test RAZORFS Phase 6 Persistence

echo "=== RAZORFS Persistence Test ==="
echo ""

# Clean start
fusermount3 -u /tmp/razorfs_mount 2>/dev/null
rm -f /dev/shm/razorfs_* 2>/dev/null
mkdir -p /tmp/razorfs_mount

echo "1️⃣  First mount - creating files..."
./razorfs /tmp/razorfs_mount -f &
PID=$!
sleep 2

# Create test files
echo "test content 1" > /tmp/razorfs_mount/file1.txt
echo "test content 2" > /tmp/razorfs_mount/file2.txt
mkdir /tmp/razorfs_mount/testdir
echo "nested content" > /tmp/razorfs_mount/testdir/nested.txt

echo "Files created:"
ls -la /tmp/razorfs_mount/
cat /tmp/razorfs_mount/file1.txt

echo ""
echo "2️⃣  Unmounting..."
kill $PID
sleep 2
fusermount3 -u /tmp/razorfs_mount 2>/dev/null

echo ""
echo "3️⃣  Second mount - checking persistence..."
./razorfs /tmp/razorfs_mount -f &
PID=$!
sleep 2

echo "Files after remount:"
ls -la /tmp/razorfs_mount/
echo "Content of file1.txt:"
cat /tmp/razorfs_mount/file1.txt

echo ""
echo "✅ Persistence test complete"
kill $PID
fusermount3 -u /tmp/razorfs_mount 2>/dev/null
