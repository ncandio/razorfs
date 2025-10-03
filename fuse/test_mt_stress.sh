#!/bin/bash
# Multithreaded stress test for RAZORFS FUSE

set -e

MOUNT_POINT="/tmp/razorfs_mount"
NUM_THREADS=10
FILES_PER_THREAD=10

echo "=== RAZORFS Multithreaded Stress Test ==="
echo "Threads: $NUM_THREADS"
echo "Files per thread: $FILES_PER_THREAD"
echo

# Worker function
worker() {
    local thread_id=$1
    local thread_dir="$MOUNT_POINT/thread_$thread_id"

    # Create thread directory
    mkdir -p "$thread_dir" || exit 1

    # Create files
    for i in $(seq 1 $FILES_PER_THREAD); do
        echo "Thread $thread_id - File $i content" > "$thread_dir/file_$i.txt" || exit 1
    done

    # Read back files to verify
    for i in $(seq 1 $FILES_PER_THREAD); do
        cat "$thread_dir/file_$i.txt" > /dev/null || exit 1
    done
}

echo "Starting $NUM_THREADS concurrent threads..."
start_time=$(date +%s%N)

# Launch workers in parallel
for t in $(seq 1 $NUM_THREADS); do
    worker $t &
done

# Wait for all threads
wait

end_time=$(date +%s%N)
duration_ms=$(( (end_time - start_time) / 1000000 ))

echo
echo "✅ Stress test complete"
echo "   Duration: ${duration_ms}ms"
echo "   Total operations: $((NUM_THREADS * FILES_PER_THREAD * 2)) (create + read)"
echo "   Operations/sec: $(( (NUM_THREADS * FILES_PER_THREAD * 2 * 1000) / duration_ms ))"

echo
echo "Verifying filesystem state..."
total_files=$(find "$MOUNT_POINT" -type f | wc -l)
total_dirs=$(find "$MOUNT_POINT" -type d | wc -l)
echo "   Total files: $total_files (expected $((NUM_THREADS * FILES_PER_THREAD)))"
echo "   Total dirs: $total_dirs (expected $((NUM_THREADS + 1)))"

if [ $total_files -eq $((NUM_THREADS * FILES_PER_THREAD)) ]; then
    echo "✅ All files created successfully"
else
    echo "❌ File count mismatch"
    exit 1
fi
