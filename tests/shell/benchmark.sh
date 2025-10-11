#!/bin/bash
# RAZORFS Comprehensive Benchmark Suite
# Compares: RAZORFS vs ext4 vs reiserfs vs btrfs

set -e

RESULTS_DIR="/results"
TESTS_DIR="/testing"
IMAGE_SIZE="100M"

# Create disk images for traditional filesystems
create_fs_images() {
    echo "📦 Creating filesystem images..."

    # EXT4
    dd if=/dev/zero of=/tmp/ext4.img bs=1M count=100 2>/dev/null
    mkfs.ext4 -F /tmp/ext4.img >/dev/null 2>&1

    # ReiserFS
    dd if=/dev/zero of=/tmp/reiserfs.img bs=1M count=100 2>/dev/null
    mkfs.reiserfs -f /tmp/reiserfs.img >/dev/null 2>&1

    # Btrfs
    dd if=/dev/zero of=/tmp/btrfs.img bs=1M count=100 2>/dev/null
    mkfs.btrfs -f /tmp/btrfs.img >/dev/null 2>&1

    echo "✅ Filesystem images created"
}

# Mount filesystems
mount_filesystems() {
    echo "🔧 Mounting filesystems..."

    mount -o loop /tmp/ext4.img /mnt/ext4
    mount -o loop /tmp/reiserfs.img /mnt/reiserfs
    mount -o loop /tmp/btrfs.img /mnt/btrfs

    # Start RAZORFS
    /razorfs/razorfs /mnt/razorfs -f &
    RAZORFS_PID=$!
    sleep 2

    echo "✅ All filesystems mounted"
}

# Metadata operations benchmark
benchmark_metadata() {
    local fs=$1
    local mount=$2
    local result_file="$RESULTS_DIR/metadata_${fs}.dat"

    echo "📊 Benchmarking metadata operations on $fs..."

    # File creation
    start=$(date +%s%N)
    for i in {1..1000}; do
        touch "$mount/file_$i.txt" 2>/dev/null || true
    done
    end=$(date +%s%N)
    create_time=$(( (end - start) / 1000000 ))

    # Stat operations
    start=$(date +%s%N)
    for i in {1..1000}; do
        stat "$mount/file_$i.txt" >/dev/null 2>&1 || true
    done
    end=$(date +%s%N)
    stat_time=$(( (end - start) / 1000000 ))

    # Deletion
    start=$(date +%s%N)
    for i in {1..1000}; do
        rm -f "$mount/file_$i.txt" 2>/dev/null || true
    done
    end=$(date +%s%N)
    delete_time=$(( (end - start) / 1000000 ))

    echo "$fs $create_time $stat_time $delete_time" >> "$result_file"
    echo "  Create: ${create_time}ms, Stat: ${stat_time}ms, Delete: ${delete_time}ms"
}

# O(log n) scalability test
benchmark_ologn() {
    local fs=$1
    local mount=$2
    local result_file="$RESULTS_DIR/ologn_${fs}.dat"

    echo "📈 Testing O(log n) scalability on $fs..."

    for scale in 10 50 100 500 1000; do
        # Create files
        for i in $(seq 1 $scale); do
            echo "data" > "$mount/scale_${scale}_file_$i.txt" 2>/dev/null || true
        done

        # Measure lookup time
        start=$(date +%s%N)
        for i in $(seq 1 $scale); do
            cat "$mount/scale_${scale}_file_$i.txt" >/dev/null 2>&1 || true
        done
        end=$(date +%s%N)
        lookup_time=$(( (end - start) / scale / 1000 ))  # ns per operation

        echo "$scale $lookup_time" >> "$result_file"

        # Cleanup
        rm -rf "$mount/scale_${scale}_"* 2>/dev/null || true
    done
}

# Compression test
benchmark_compression() {
    local fs=$1
    local mount=$2
    local result_file="$RESULTS_DIR/compression_${fs}.dat"

    echo "🗜️  Testing compression on $fs..."

    # Create repetitive data (compressible)
    local data=""
    for i in {1..100}; do
        data+="This is a repetitive line that compresses well - line $i\n"
    done

    # Write and measure
    echo -e "$data" > "$mount/compressible.txt" 2>/dev/null || true
    original_size=$(stat -f %z "$mount/compressible.txt" 2>/dev/null || stat -c %s "$mount/compressible.txt" 2>/dev/null || echo "0")

    # For RAZORFS, compression is automatic
    # For others, measure actual disk usage
    if [ "$fs" = "razorfs" ]; then
        echo "$fs $original_size auto" >> "$result_file"
    else
        du_size=$(du -b "$mount/compressible.txt" 2>/dev/null | cut -f1 || echo "$original_size")
        ratio=$(echo "scale=2; $original_size / $du_size" | bc 2>/dev/null || echo "1.0")
        echo "$fs $original_size $du_size $ratio" >> "$result_file"
    fi

    rm -f "$mount/compressible.txt" 2>/dev/null || true
}

# I/O throughput benchmark
benchmark_io() {
    local fs=$1
    local mount=$2
    local result_file="$RESULTS_DIR/io_${fs}.dat"

    echo "💾 Testing I/O throughput on $fs..."

    # Sequential write
    start=$(date +%s%N)
    dd if=/dev/zero of="$mount/iotest.dat" bs=1M count=10 2>/dev/null || true
    sync
    end=$(date +%s%N)
    write_time=$(( (end - start) / 1000000 ))
    write_mb_s=$(echo "scale=2; 10000 / $write_time" | bc 2>/dev/null || echo "0")

    # Sequential read
    start=$(date +%s%N)
    dd if="$mount/iotest.dat" of=/dev/null bs=1M 2>/dev/null || true
    end=$(date +%s%N)
    read_time=$(( (end - start) / 1000000 ))
    read_mb_s=$(echo "scale=2; 10000 / $read_time" | bc 2>/dev/null || echo "0")

    echo "$fs $write_mb_s $read_mb_s" >> "$result_file"
    echo "  Write: ${write_mb_s} MB/s, Read: ${read_mb_s} MB/s"

    rm -f "$mount/iotest.dat" 2>/dev/null || true
}

# Cleanup
cleanup() {
    echo "🧹 Cleaning up..."

    killall razorfs 2>/dev/null || true
    sleep 1

    umount /mnt/ext4 2>/dev/null || true
    umount /mnt/reiserfs 2>/dev/null || true
    umount /mnt/btrfs 2>/dev/null || true
    fusermount3 -u /mnt/razorfs 2>/dev/null || true

    rm -f /tmp/*.img 2>/dev/null || true
    rm -f /dev/shm/razorfs_* 2>/dev/null || true
}

# Main execution
main() {
    echo "========================================"
    echo "  RAZORFS Comprehensive Benchmark Suite"
    echo "========================================"
    echo ""

    mkdir -p "$RESULTS_DIR"

    create_fs_images
    mount_filesystems

    # Run benchmarks on all filesystems
    for fs in ext4 reiserfs btrfs razorfs; do
        case $fs in
            ext4) mount="/mnt/ext4" ;;
            reiserfs) mount="/mnt/reiserfs" ;;
            btrfs) mount="/mnt/btrfs" ;;
            razorfs) mount="/mnt/razorfs" ;;
        esac

        echo ""
        echo "=== Testing $fs ==="
        benchmark_metadata "$fs" "$mount"
        benchmark_ologn "$fs" "$mount"
        benchmark_compression "$fs" "$mount"
        benchmark_io "$fs" "$mount"
    done

    cleanup

    echo ""
    echo "✅ Benchmarks complete! Results in $RESULTS_DIR"
}

trap cleanup EXIT
main
