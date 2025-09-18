#!/bin/bash
# Quick filesystem comparison - basic operations test
# Inspired by existing RAZOR testing framework

set -e

RESULT_DIR=$1
if [[ -z "$RESULT_DIR" ]]; then
    echo "Usage: $0 <result_directory>"
    exit 1
fi

# Test configuration
FILESYSTEMS=("razorfs" "ext4" "btrfs" "reiserfs")
MOUNT_POINTS=("/mnt/razorfs" "/mnt/ext4" "/mnt/btrfs" "/mnt/reiserfs")
TEST_FILES=(100 500 1000)  # Number of files to test
FILE_SIZE="1K"  # Size of each test file

echo "=== Quick Filesystem Comparison ==="
echo "Testing filesystems: ${FILESYSTEMS[*]}"
echo "Result directory: $RESULT_DIR"

# Create CSV header
CSV_FILE="$RESULT_DIR/quick_comparison.csv"
echo "filesystem,operation,file_count,time_seconds,throughput_ops_per_sec,memory_kb" > "$CSV_FILE"

# Function to measure memory usage
get_memory_usage() {
    ps aux | grep -E "(razorfs_fuse|mount)" | grep -v grep | awk '{sum += $6} END {print sum+0}'
}

# Function to run test on a filesystem
run_filesystem_test() {
    local fs_name=$1
    local mount_point=$2
    local file_count=$3

    echo "Testing $fs_name with $file_count files..."

    # Create test directory
    test_dir="$mount_point/quick_test_$$"
    mkdir -p "$test_dir"

    # Test 1: File Creation
    echo "  Testing file creation..."
    memory_before=$(get_memory_usage)
    start_time=$(date +%s.%N)

    for i in $(seq 1 $file_count); do
        echo "test content $i" > "$test_dir/file_$i.txt"
    done

    end_time=$(date +%s.%N)
    memory_after=$(get_memory_usage)

    creation_time=$(echo "$end_time - $start_time" | bc)
    creation_throughput=$(echo "scale=2; $file_count / $creation_time" | bc)
    memory_used=$(echo "$memory_after - $memory_before" | bc)

    echo "$fs_name,file_creation,$file_count,$creation_time,$creation_throughput,$memory_used" >> "$CSV_FILE"

    # Test 2: File Reading
    echo "  Testing file reading..."
    start_time=$(date +%s.%N)

    for i in $(seq 1 $file_count); do
        cat "$test_dir/file_$i.txt" > /dev/null
    done

    end_time=$(date +%s.%N)

    read_time=$(echo "$end_time - $start_time" | bc)
    read_throughput=$(echo "scale=2; $file_count / $read_time" | bc)

    echo "$fs_name,file_reading,$file_count,$read_time,$read_throughput,0" >> "$CSV_FILE"

    # Test 3: Directory Listing
    echo "  Testing directory listing..."
    start_time=$(date +%s.%N)

    ls "$test_dir" > /dev/null

    end_time=$(date +%s.%N)

    list_time=$(echo "$end_time - $start_time" | bc)
    list_throughput=$(echo "scale=2; $file_count / $list_time" | bc)

    echo "$fs_name,directory_listing,$file_count,$list_time,$list_throughput,0" >> "$CSV_FILE"

    # Test 4: File Deletion
    echo "  Testing file deletion..."
    start_time=$(date +%s.%N)

    for i in $(seq 1 $file_count); do
        rm "$test_dir/file_$i.txt"
    done

    end_time=$(date +%s.%N)

    delete_time=$(echo "$end_time - $start_time" | bc)
    delete_throughput=$(echo "scale=2; $file_count / $delete_time" | bc)

    echo "$fs_name,file_deletion,$file_count,$delete_time,$delete_throughput,0" >> "$CSV_FILE"

    # Cleanup
    rmdir "$test_dir"

    echo "  $fs_name test completed"
}

# Function to test path resolution performance (RazorFS specialty)
test_path_resolution() {
    echo "=== Path Resolution Performance Test ==="

    # Create deep directory structure
    for i in "${!FILESYSTEMS[@]}"; do
        fs_name="${FILESYSTEMS[$i]}"
        mount_point="${MOUNT_POINTS[$i]}"

        echo "Testing path resolution on $fs_name..."

        # Create nested directories (10 levels deep)
        deep_path="$mount_point/level1/level2/level3/level4/level5/level6/level7/level8/level9/level10"
        mkdir -p "$deep_path"

        # Time path resolution
        start_time=$(date +%s.%N)

        for j in $(seq 1 100); do
            test -d "$deep_path"
        done

        end_time=$(date +%s.%N)

        resolution_time=$(echo "$end_time - $start_time" | bc)
        resolution_throughput=$(echo "scale=2; 100 / $resolution_time" | bc)

        echo "$fs_name,path_resolution,100,$resolution_time,$resolution_throughput,0" >> "$CSV_FILE"

        # Cleanup
        rm -rf "$mount_point/level1"

        echo "  $fs_name path resolution test completed"
    done
}

# Main test execution
for file_count in "${TEST_FILES[@]}"; do
    echo "=== Testing with $file_count files ==="

    for i in "${!FILESYSTEMS[@]}"; do
        fs_name="${FILESYSTEMS[$i]}"
        mount_point="${MOUNT_POINTS[$i]}"

        if [[ -d "$mount_point" ]] && mountpoint -q "$mount_point" 2>/dev/null || [[ "$fs_name" == "razorfs" ]] && pgrep -f "razorfs_fuse.*$mount_point" >/dev/null; then
            run_filesystem_test "$fs_name" "$mount_point" "$file_count"
        else
            echo "Skipping $fs_name - not mounted at $mount_point"
        fi
    done
done

# Run path resolution test
test_path_resolution

# Generate summary report
echo "=== Quick Comparison Summary ===" > "$RESULT_DIR/quick_summary.txt"
echo "Test completed at: $(date)" >> "$RESULT_DIR/quick_summary.txt"
echo "" >> "$RESULT_DIR/quick_summary.txt"

# Analyze results with Python if available
if command -v python3 >/dev/null; then
    python3 << EOF
import csv
import sys
from collections import defaultdict

try:
    # Read results
    results = defaultdict(lambda: defaultdict(list))

    with open('$CSV_FILE', 'r') as f:
        reader = csv.DictReader(f)
        for row in reader:
            fs = row['filesystem']
            op = row['operation']
            throughput = float(row['throughput_ops_per_sec'])
            results[fs][op].append(throughput)

    # Calculate averages and write summary
    with open('$RESULT_DIR/quick_summary.txt', 'a') as f:
        f.write("Average Throughput (operations/second):\n")
        f.write("=" * 50 + "\n")

        operations = ['file_creation', 'file_reading', 'directory_listing', 'file_deletion', 'path_resolution']

        for op in operations:
            f.write(f"\n{op.replace('_', ' ').title()}:\n")
            for fs in ['razorfs', 'ext4', 'btrfs', 'reiserfs']:
                if fs in results and op in results[fs]:
                    avg_throughput = sum(results[fs][op]) / len(results[fs][op])
                    f.write(f"  {fs:10}: {avg_throughput:8.2f} ops/sec\n")
                else:
                    f.write(f"  {fs:10}: Not tested\n")

        # Identify winners
        f.write(f"\nPerformance Leaders:\n")
        f.write("=" * 20 + "\n")

        for op in operations:
            best_fs = None
            best_score = 0

            for fs in ['razorfs', 'ext4', 'btrfs', 'reiserfs']:
                if fs in results and op in results[fs]:
                    avg = sum(results[fs][op]) / len(results[fs][op])
                    if avg > best_score:
                        best_score = avg
                        best_fs = fs

            if best_fs:
                f.write(f"{op.replace('_', ' ').title():20}: {best_fs} ({best_score:.2f} ops/sec)\n")

except Exception as e:
    with open('$RESULT_DIR/quick_summary.txt', 'a') as f:
        f.write(f"Error generating summary: {e}\n")
EOF
fi

echo "=== Quick Comparison Complete ==="
echo "Results saved to: $RESULT_DIR"
echo "Summary: $RESULT_DIR/quick_summary.txt"
echo "Raw data: $CSV_FILE"