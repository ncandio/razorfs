#!/bin/bash

echo "=== SIMPLE PERSISTENCE TEST ==="

# Clean start
rm -f /tmp/razorfs.dat
mkdir -p /tmp/simple_mount

echo "Step 1: Creating filesystem and data..."
./fuse/razorfs_fuse /tmp/simple_mount &
FUSE_PID=$!
sleep 2

# Create test structure
mkdir /tmp/simple_mount/dir1
echo "Test content" > /tmp/simple_mount/dir1/file1.txt

echo "Created structure:"
ls -la /tmp/simple_mount/
ls -la /tmp/simple_mount/dir1/

# Graceful shutdown
echo "Step 2: Shutting down..."
kill -TERM $FUSE_PID
wait $FUSE_PID

echo "Step 3: Restarting filesystem..."
./fuse/razorfs_fuse /tmp/simple_mount &
FUSE_PID=$!
sleep 3

echo "Step 4: Checking if data persisted..."
echo "Root directory:"
ls -la /tmp/simple_mount/ || echo "Failed to list root"

echo "Subdirectory:"
ls -la /tmp/simple_mount/dir1/ || echo "Failed to list dir1"

echo "File content:"
cat /tmp/simple_mount/dir1/file1.txt || echo "Failed to read file"

# Cleanup
kill -TERM $FUSE_PID 2>/dev/null
fusermount3 -u /tmp/simple_mount 2>/dev/null
rmdir /tmp/simple_mount 2>/dev/null