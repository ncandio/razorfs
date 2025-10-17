#!/bin/bash
# cache_friendlyness_benchmark.sh

# Colors for output
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${BLUE}Starting cache friendliness benchmark...${NC}"

# Setup
mkdir -p /tmp/razorfs_cache_test /tmp/ext4_cache_test

# Build RazorFS
cd /home/nico/WORK_ROOT/razorfs
echo -e "${BLUE}Building RazorFS...${NC}"
make clean && make

# Mount RazorFS in background
echo -e "${BLUE}Mounting RazorFS...${NC}"
./razorfs /tmp/razorfs_cache_test &

# Wait a moment for mount to complete
sleep 2

# Function to run directory traversal benchmark
run_dir_traversal_benchmark() {
    local fs_type=$1
    local mount_point=$2
    local num_dirs=${3:-100}
    
    echo -e "${BLUE}Running directory traversal benchmark on $fs_type with $num_dirs directories...${NC}"
    
    # Create test directories
    for i in $(seq 1 $num_dirs); do
        mkdir -p "$mount_point/dir_$i" 2>/dev/null
        echo "content_$i" > "$mount_point/dir_$i/file.txt" 2>/dev/null
    done
    
    # Measure cache performance during sequential access
    echo "Running perf analysis for $fs_type directory traversal..."
    perf stat -e cache-references,cache-misses,instructions,cycles \
        -o "${fs_type}_dir_traversal.perf" \
        -x, \
        bash -c "for i in \$(seq 1 $num_dirs); do ls $mount_point/dir_$i 2>/dev/null > /dev/null; done" 2>&1 | grep -E "(cache|instructions|cycles|seconds time)"
    
    # Also measure simple read performance
    perf stat -e cache-references,cache-misses,instructions,cycles \
        -o "${fs_type}_read.perf" \
        -x, \
        bash -c "for i in \$(seq 1 $num_dirs); do cat $mount_point/dir_$i/file.txt 2>/dev/null > /dev/null; done" 2>&1 | grep -E "(cache|instructions|cycles|seconds time)"
    
    # Clean up
    rm -rf "$mount_point"/*
}

# Function to run random access benchmark
run_random_access_benchmark() {
    local fs_type=$1
    local mount_point=$2
    local num_files=${3:-50}
    
    echo -e "${BLUE}Running random access benchmark on $fs_type with $num_files files...${NC}"
    
    # Create test files
    for i in $(seq 1 $num_files); do
        echo "content_$i_$(dd if=/dev/urandom bs=1 count=10 2>/dev/null | base64 | head -c 10)" > "$mount_point/file_$i.txt" 2>/dev/null
    done
    
    # Create random access sequence
    seq 1 $num_files | shuf > "$mount_point/access_order.txt"
    
    # Measure cache performance during random access
    echo "Running perf analysis for $fs_type random access..."
    perf stat -e cache-references,cache-misses,instructions,cycles \
        -o "${fs_type}_random_access.perf" \
        -x, \
        bash -c "while read -r line; do cat $mount_point/file_\$line.txt 2>/dev/null > /dev/null; done < $mount_point/access_order.txt" 2>&1 | grep -E "(cache|instructions|cycles|seconds time)"
    
    # Clean up
    rm -rf "$mount_point"/*
}

# Function to run tree depth benchmark
run_tree_depth_benchmark() {
    local mount_point=$1
    local max_depth=${2:-5}
    
    echo -e "${BLUE}Running tree depth benchmark on RazorFS...${NC}"
    
    # Build nested directory structure
    current_path="$mount_point"
    for i in $(seq 1 $max_depth); do
        new_dir="$current_path/depth_$i"
        mkdir -p "$new_dir"
        echo "depth_$i_content_$(dd if=/dev/urandom bs=1 count=8 2>/dev/null | base64 | head -c 8)" > "$new_dir/data.txt"
        current_path="$new_dir"
    done
    
    # Measure cache performance during deep tree traversal
    echo "Running perf analysis for RazorFS tree depth..."
    perf stat -e cache-references,cache-misses,instructions,cycles \
        -o "razorfs_tree_depth.perf" \
        -x, \
        bash -c "find $mount_point -type f -exec cat {} \; > /dev/null" 2>&1 | grep -E "(cache|instructions|cycles|seconds time)"
    
    # Clean up
    rm -rf "$mount_point"/*
}

# Run benchmarks for both filesystems
echo -e "${YELLOW}Running benchmarks for directory traversal...${NC}"
run_dir_traversal_benchmark "razorfs" "/tmp/razorfs_cache_test" 100
run_dir_traversal_benchmark "ext4" "/tmp/ext4_cache_test" 100

echo -e "${YELLOW}Running benchmarks for random access...${NC}"
run_random_access_benchmark "razorfs" "/tmp/razorfs_cache_test" 50
run_random_access_benchmark "ext4" "/tmp/ext4_cache_test" 50

echo -e "${YELLOW}Running benchmarks for tree depth (RazorFS only)...${NC}"
run_tree_depth_benchmark "/tmp/razorfs_cache_test" 5

# Unmount RazorFS
echo -e "${BLUE}Unmounting RazorFS...${NC}"
fusermount3 -u /tmp/razorfs_cache_test

# Process and display results
echo -e "${GREEN}Processing results...${NC}"

echo -e "\n${YELLOW}=== DIRECTORY TRAVERSAL RESULTS ===${NC}"
echo -e "${BLUE}RazorFS:${NC}"
if [ -f "razorfs_dir_traversal.perf" ]; then
    echo "Cache references, misses, instructions, cycles:"
    cat razorfs_dir_traversal.perf 2>/dev/null | grep -v "^#" | head -4
fi

echo -e "${BLUE}ext4:${NC}"
if [ -f "ext4_dir_traversal.perf" ]; then
    echo "Cache references, misses, instructions, cycles:"
    cat ext4_dir_traversal.perf 2>/dev/null | grep -v "^#" | head -4
fi

echo -e "\n${YELLOW}=== FILE READ RESULTS ===${NC}"
echo -e "${BLUE}RazorFS:${NC}"
if [ -f "razorfs_read.perf" ]; then
    echo "Cache references, misses, instructions, cycles:"
    cat razorfs_read.perf 2>/dev/null | grep -v "^#" | head -4
fi

echo -e "${BLUE}ext4:${NC}"
if [ -f "ext4_read.perf" ]; then
    echo "Cache references, misses, instructions, cycles:"
    cat ext4_read.perf 2>/dev/null | grep -v "^#" | head -4
fi

echo -e "\n${YELLOW}=== RANDOM ACCESS RESULTS ===${NC}"
echo -e "${BLUE}RazorFS:${NC}"
if [ -f "razorfs_random_access.perf" ]; then
    echo "Cache references, misses, instructions, cycles:"
    cat razorfs_random_access.perf 2>/dev/null | grep -v "^#" | head -4
fi

echo -e "${BLUE}ext4:${NC}"
if [ -f "ext4_random_access.perf" ]; then
    echo "Cache references, misses, instructions, cycles:"
    cat ext4_random_access.perf 2>/dev/null | grep -v "^#" | head -4
fi

echo -e "\n${YELLOW}=== TREE DEPTH RESULTS (RazorFS only) ===${NC}"
if [ -f "razorfs_tree_depth.perf" ]; then
    echo "Cache references, misses, instructions, cycles:"
    cat razorfs_tree_depth.perf 2>/dev/null | grep -v "^#" | head -4
fi

echo -e "\n${GREEN}Cache friendliness benchmark complete.${NC}"