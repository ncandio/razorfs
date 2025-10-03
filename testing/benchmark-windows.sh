#!/bin/bash
# RAZORFS Windows Benchmark (outputs to mounted Windows paths)

set -e

DATA_DIR="/data"
RESULTS_DIR="/results"
IMAGE_SIZE="100M"

# Create disk images for traditional filesystems
create_fs_images() {
    echo "📦 Creating filesystem images..."
    dd if=/dev/zero of=/tmp/ext4.img bs=1M count=100 2>/dev/null
    mkfs.ext4 -F /tmp/ext4.img >/dev/null 2>&1

    dd if=/dev/zero of=/tmp/reiserfs.img bs=1M count=100 2>/dev/null
    mkfs.reiserfs -f /tmp/reiserfs.img >/dev/null 2>&1

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

    /razorfs/razorfs /mnt/razorfs -f &
    RAZORFS_PID=$!
    sleep 2
    echo "✅ All filesystems mounted"
}

# Benchmark functions
benchmark_metadata() {
    local fs=$1
    local mount=$2

    echo "📊 Benchmarking metadata on $fs..."

    # Create
    start=$(date +%s%N)
    for i in {1..1000}; do touch "$mount/file_$i.txt" 2>/dev/null || true; done
    end=$(date +%s%N)
    create_time=$(( (end - start) / 1000000 ))

    # Stat
    start=$(date +%s%N)
    for i in {1..1000}; do stat "$mount/file_$i.txt" >/dev/null 2>&1 || true; done
    end=$(date +%s%N)
    stat_time=$(( (end - start) / 1000000 ))

    # Delete
    start=$(date +%s%N)
    for i in {1..1000}; do rm -f "$mount/file_$i.txt" 2>/dev/null || true; done
    end=$(date +%s%N)
    delete_time=$(( (end - start) / 1000000 ))

    echo "$fs,$create_time,$stat_time,$delete_time" >> "$DATA_DIR/metadata_${fs}.dat"
    echo "  Create: ${create_time}ms, Stat: ${stat_time}ms, Delete: ${delete_time}ms"
}

benchmark_ologn() {
    local fs=$1
    local mount=$2

    echo "📈 Testing O(log n) on $fs..."

    for scale in 10 50 100 500 1000; do
        for i in $(seq 1 $scale); do
            echo "data" > "$mount/scale_${scale}_file_$i.txt" 2>/dev/null || true
        done

        start=$(date +%s%N)
        for i in $(seq 1 $scale); do
            cat "$mount/scale_${scale}_file_$i.txt" >/dev/null 2>&1 || true
        done
        end=$(date +%s%N)
        lookup_time=$(( (end - start) / scale / 1000 ))

        echo "$scale $lookup_time" >> "$DATA_DIR/ologn_${fs}.dat"
        rm -rf "$mount/scale_${scale}_"* 2>/dev/null || true
    done
}

benchmark_io() {
    local fs=$1
    local mount=$2

    echo "💾 Testing I/O on $fs..."

    # Write
    start=$(date +%s%N)
    dd if=/dev/zero of="$mount/iotest.dat" bs=1M count=10 2>/dev/null || true
    sync
    end=$(date +%s%N)
    write_time=$(( (end - start) / 1000000 ))
    write_mb_s=$(echo "scale=2; 10000 / $write_time" | bc 2>/dev/null || echo "0")

    # Read
    start=$(date +%s%N)
    dd if="$mount/iotest.dat" of=/dev/null bs=1M 2>/dev/null || true
    end=$(date +%s%N)
    read_time=$(( (end - start) / 1000000 ))
    read_mb_s=$(echo "scale=2; 10000 / $read_time" | bc 2>/dev/null || echo "0")

    echo "$fs,$write_mb_s,$read_mb_s" >> "$DATA_DIR/io_${fs}.dat"
    echo "  Write: ${write_mb_s} MB/s, Read: ${read_mb_s} MB/s"

    rm -f "$mount/iotest.dat" 2>/dev/null || true
}

# Cleanup
cleanup() {
    killall razorfs 2>/dev/null || true
    sleep 1
    umount /mnt/ext4 2>/dev/null || true
    umount /mnt/reiserfs 2>/dev/null || true
    umount /mnt/btrfs 2>/dev/null || true
    fusermount3 -u /mnt/razorfs 2>/dev/null || true
    rm -f /tmp/*.img /dev/shm/razorfs_* 2>/dev/null || true
}

# Main
main() {
    echo "========================================"
    echo "  RAZORFS Windows Benchmark"
    echo "========================================"

    create_fs_images
    mount_filesystems

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
        benchmark_io "$fs" "$mount"
    done

    # Create summary
    echo "RAZORFS 1.0" > "$RESULTS_DIR/summary.txt"
    echo "Benchmark completed: $(date)" >> "$RESULTS_DIR/summary.txt"
    echo "Results in: $DATA_DIR" >> "$RESULTS_DIR/summary.txt"

    cleanup
    echo ""
    echo "✅ Benchmarks complete! Check Windows folders."
}

trap cleanup EXIT
main
