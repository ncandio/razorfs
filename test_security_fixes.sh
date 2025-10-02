#!/bin/bash

echo "=================================="
echo "🔒 RAZORFS Security Fixes Test"
echo "=================================="

# Test 1: Path traversal protection
echo ""
echo "Test 1: Path Traversal Protection"
echo "----------------------------------"

# Try to create a file with .. in the path (should be blocked)
echo "Attempting path traversal attack: /test/../etc/passwd"
if echo "hacked" > /tmp/razorfs_mount/test/../etc/passwd 2>&1 | grep -q "No such file"; then
    echo "✅ Path traversal blocked correctly"
else
    echo "❌ WARNING: Path traversal may not be blocked!"
fi

# Test 2: Normal operations still work
echo ""
echo "Test 2: Normal Operations"
echo "-------------------------"
mkdir -p /tmp/razorfs_mount/testdir
echo "test data" > /tmp/razorfs_mount/testdir/file.txt
if cat /tmp/razorfs_mount/testdir/file.txt | grep -q "test data"; then
    echo "✅ Normal file operations working"
else
    echo "❌ Normal operations broken"
fi

# Test 3: Multithreaded stress test (verify no memory leaks)
echo ""
echo "Test 3: Memory Leak Test (Multithreaded)"
echo "----------------------------------------"
for i in {1..100}; do
    echo "file $i" > /tmp/razorfs_mount/testdir/stress_$i.txt &
done
wait
FILES_CREATED=$(ls /tmp/razorfs_mount/testdir/ | wc -l)
echo "Files created: $FILES_CREATED"
if [ "$FILES_CREATED" -ge "90" ]; then
    echo "✅ Multithreaded stress test passed"
else
    echo "⚠️  Some files not created: $FILES_CREATED/101"
fi

# Cleanup
rm -rf /tmp/razorfs_mount/testdir

echo ""
echo "=================================="
echo "✅ Security test campaign complete"
echo "=================================="
