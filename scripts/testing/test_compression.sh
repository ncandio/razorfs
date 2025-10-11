#!/bin/bash
# Test RAZORFS Compression

echo "=== RAZORFS Compression Test ==="
echo ""

fusermount3 -u /tmp/razorfs_mount 2>/dev/null
rm -f /dev/shm/razorfs_* 2>/dev/null
mkdir -p /tmp/razorfs_mount

echo "🚀 Starting filesystem..."
./razorfs /tmp/razorfs_mount -f &
PID=$!
sleep 2

# Create small file (won't compress - < 512 bytes)
echo "📝 Test 1: Small file (no compression expected)"
echo "Small file content" > /tmp/razorfs_mount/small.txt
ls -lh /tmp/razorfs_mount/small.txt

# Create large compressible file (> 512 bytes, repetitive)
echo ""
echo "📝 Test 2: Large compressible file"
for i in {1..100}; do
    echo "This is a repetitive line that should compress well - line $i"
done > /tmp/razorfs_mount/large.txt

ls -lh /tmp/razorfs_mount/large.txt
echo "Original size: $(wc -c < /tmp/razorfs_mount/large.txt) bytes"

# Read it back
echo ""
echo "📖 Reading file back..."
head -5 /tmp/razorfs_mount/large.txt
echo "..."
tail -2 /tmp/razorfs_mount/large.txt

# Create random file (won't compress well)
echo ""
echo "📝 Test 3: Random data (compression not beneficial)"
dd if=/dev/urandom bs=1024 count=2 2>/dev/null > /tmp/razorfs_mount/random.dat
ls -lh /tmp/razorfs_mount/random.dat

echo ""
echo "✅ Compression test complete"
kill $PID
fusermount3 -u /tmp/razorfs_mount 2>/dev/null
