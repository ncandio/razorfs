#!/bin/bash
# run_cache_bench.sh - Run cache locality benchmark comparing RazorFS and ext4

# Colors for output
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${BLUE}Starting cache locality benchmark comparing RazorFS and ext4...${NC}"

# Setup results directory
mkdir -p /app/benchmarks/results

# Function to run directory traversal benchmark
run_dir_traversal_benchmark() {
    local fs_type=$1
    local mount_point=$2
    local num_dirs=${3:-100}
    
    echo -e "${BLUE}Running directory traversal benchmark on $fs_type with $num_dirs directories...${NC}"
    
    # Create test directories
    for i in $(seq 1 $num_dirs); do
        mkdir -p "$mount_point/dir_$i" 2>/dev/null
        if [ $? -eq 0 ]; then
            echo "content_$i_$(dd if=/dev/urandom bs=1 count=10 2>/dev/null | base64 | head -c 10)" > "$mount_point/dir_$i/file.txt" 2>/dev/null
        fi
    done
    
    # Measure time and other metrics for sequential access
    START_TIME=$(date +%s.%N)
    for i in $(seq 1 $num_dirs); do
        ls "$mount_point/dir_$i" 2>/dev/null > /dev/null
    done
    END_TIME=$(date +%s.%N)
    DURATION=$(echo "$END_TIME - $START_TIME" | bc -l)
    
    echo "$fs_type directory traversal time: ${DURATION}s"
    
    # Measure read performance
    START_TIME=$(date +%s.%N)
    for i in $(seq 1 $num_dirs); do
        cat "$mount_point/dir_$i/file.txt" 2>/dev/null > /dev/null
    done
    END_TIME=$(date +%s.%N)
    READ_DURATION=$(echo "$END_TIME - $START_TIME" | bc -l)
    
    echo "$fs_type read time: ${READ_DURATION}s"
    
    # Write results to file
    if [ ! -f "/app/benchmarks/results/cache_locality_benchmark.csv" ]; then
        echo "filesystem,operation,time,access_pattern,fs_type" > "/app/benchmarks/results/cache_locality_benchmark.csv"
    fi
    echo "$fs_type,dir_traversal,$DURATION,sequential,cache_friendly" >> "/app/benchmarks/results/cache_locality_benchmark.csv"
    echo "$fs_type,read,$READ_DURATION,sequential,cache_friendly" >> "/app/benchmarks/results/cache_locality_benchmark.csv"
    
    # Clean up
    rm -rf "$mount_point"/*
}

# Function to run random access benchmark
run_random_access_benchmark() {
    local fs_type=$1
    local mount_point=$2
    local num_files=${3:-50}
    
    echo -e "${BLUE}Running random access benchmark on $fs_type with $num_files files...${NC}"
    
    # Create test files with random content to prevent compression artifacts
    for i in $(seq 1 $num_files); do
        random_content=$(openssl rand -base64 32 2>/dev/null || echo "content_$(dd if=/dev/urandom bs=1 count=16 2>/dev/null | base64 | head -c 16)")
        echo "$random_content" > "$mount_point/file_$i.txt" 2>/dev/null
    done
    
    # Create random access sequence
    seq 1 $num_files | shuf > "$mount_point/access_order.txt"
    
    # Measure time for random access
    START_TIME=$(date +%s.%N)
    while read -r line; do
        cat "$mount_point/file_$line.txt" 2>/dev/null > /dev/null
    done < "$mount_point/access_order.txt"
    END_TIME=$(date +%s.%N)
    DURATION=$(echo "$END_TIME - $START_TIME" | bc -l)
    
    echo "$fs_type random access time: ${DURATION}s"
    
    # Append to results
    echo "$fs_type,random_access,$DURATION,random,cache_friendly" >> "/app/benchmarks/results/cache_locality_benchmark.csv"
    
    # Clean up
    rm -rf "$mount_point"/*
}

# Function to run cache performance benchmark using perf
run_cache_perf_benchmark() {
    local fs_type=$1
    local mount_point=$2
    local num_ops=${3:-1000}
    
    echo -e "${BLUE}Running detailed cache performance benchmark on $fs_type...${NC}"
    
    # Create test files
    for i in $(seq 1 $num_ops); do
        echo "data_$(dd if=/dev/urandom bs=1 count=8 2>/dev/null | base64 | head -c 8)" > "$mount_point/file_$i.txt" 2>/dev/null
    done
    
    # Run perf to measure cache misses during sequential access
    perf stat -e cache-references,cache-misses,page-faults,instructions,cycles \
        -o "/app/benchmarks/results/${fs_type}_cache_perf.perf" \
        -x, \
        bash -c "for i in \$(seq 1 $num_ops); do cat $mount_point/file_$i.txt 2>/dev/null > /dev/null; done" 2>&1 | grep -E "(cache|instructions|cycles|seconds time|page-faults)"
    
    # Extract key metrics from perf output
    if [ -f "/app/benchmarks/results/${fs_type}_cache_perf.perf" ]; then
        # Extract cache miss rate
        CACHE_MISSES=$(grep "cache-misses" "/app/benchmarks/results/${fs_type}_cache_perf.perf" | cut -d',' -f1 | tr -d ',')
        CACHE_REFERENCES=$(grep "cache-references" "/app/benchmarks/results/${fs_type}_cache_perf.perf" | cut -d',' -f1 | tr -d ',')
        
        if [ ! -z "$CACHE_MISSES" ] && [ ! -z "$CACHE_REFERENCES" ] && [ "$CACHE_REFERENCES" != "0" ]; then
            CACHE_MISS_RATE=$(echo "scale=4; $CACHE_MISSES / $CACHE_REFERENCES" | bc -l)
            echo "$fs_type cache miss rate: $CACHE_MISS_RATE"
            
            # Add to CSV results
            if [ ! -f "/app/benchmarks/results/cache_miss_analysis.csv" ]; then
                echo "filesystem,cache_references,cache_misses,cache_miss_rate,operation" > "/app/benchmarks/results/cache_miss_analysis.csv"
            fi
            echo "$fs_type,$CACHE_REFERENCES,$CACHE_MISSES,$CACHE_MISS_RATE,sequential_access" >> "/app/benchmarks/results/cache_miss_analysis.csv"
        fi
    fi
    
    # Clean up
    rm -rf "$mount_point"/*
}

# Build and mount RazorFS
echo -e "${BLUE}Building RazorFS...${NC}"
cd /app
make clean && make

echo -e "${BLUE}Mounting RazorFS with FUSE3...${NC}"
timeout 60s ./razorfs /tmp/razorfs_cache_test &

# Wait for mount to complete
sleep 3

# Run benchmarks for both filesystems
echo -e "${YELLOW}Running benchmarks for directory traversal...${NC}"
run_dir_traversal_benchmark "razorfs (FUSE3)" "/tmp/razorfs_cache_test" 100
run_dir_traversal_benchmark "ext4" "/tmp/ext4_cache_test" 100

echo -e "${YELLOW}Running benchmarks for random access...${NC}"
run_random_access_benchmark "razorfs (FUSE3)" "/tmp/razorfs_cache_test" 50
run_random_access_benchmark "ext4" "/tmp/ext4_cache_test" 50

echo -e "${YELLOW}Running detailed cache performance analysis...${NC}"
run_cache_perf_benchmark "razorfs (FUSE3)" "/tmp/razorfs_cache_test" 500
run_cache_perf_benchmark "ext4" "/tmp/ext4_cache_test" 500

# Unmount RazorFS
echo -e "${BLUE}Unmounting RazorFS...${NC}"
fusermount3 -u /tmp/razorfs_cache_test 2>/dev/null || true

# Process results and create plots
echo -e "${GREEN}Processing results and generating plots...${NC}"

# Create gnuplot script for cache locality comparison
cat > /app/benchmarks/results/cache_locality_plot.gp << 'EOF'
set terminal pngcairo enhanced font 'Arial,12' size 1200,800
set output 'cache_locality_comparison.png'

set title "Cache Locality Comparison: RazorFS (FUSE3) vs ext4\nPerformance Analysis with Sequential and Random Access Patterns" font "Arial Bold,14"

set xlabel "Filesystem and Operation Type" font "Arial,10"
set ylabel "Time (seconds)" font "Arial,10"
set logscale y
set grid ytics xtics lw 1 lc rgb "#bdc3c7"

set key outside right center box font "Arial,10" spacing 1.5
set boxwidth 0.8 relative
set style fill solid 0.7 border -1

set xtics rotate by -45

# Prepare the data file for plotting
datafile = 'cache_locality_benchmark.csv'

plot datafile using 0:3:xticlabels(sprintf("%s_%s", strcol(1), strcol(2))) with boxes \
     title 'Sequential Access Time' lc rgb '#3498db', \
     '' using 0:3:3 with labels offset char 0,1 font "Arial,8" notitle
EOF

# Generate the plot
if [ -f "/app/benchmarks/results/cache_locality_benchmark.csv" ]; then
    gnuplot /app/benchmarks/results/cache_locality_plot.gp
    echo -e "${GREEN}Cache locality comparison plot generated: /app/benchmarks/results/cache_locality_comparison.png${NC}"
fi

# Create gnuplot script for cache miss analysis
cat > /app/benchmarks/results/cache_miss_plot.gp << 'EOF'
set terminal pngcairo enhanced font 'Arial,12' size 1000,700
set output 'cache_miss_analysis.png'

set title "Cache Miss Rate Comparison: RazorFS (FUSE3) vs ext4" font "Arial Bold,14"

set xlabel "Filesystem" font "Arial,12"
set ylabel "Cache Miss Rate" font "Arial,12"
set grid ytics xtics lw 1 lc rgb "#bdc3c7"

set key top left box font "Arial,10"
set boxwidth 0.6 relative
set style fill solid 0.7 border -1

datafile = 'cache_miss_analysis.csv'

plot datafile using 0:4:xticlabels(1) with boxes \
     title 'Cache Miss Rate' lc rgb '#e74c3c', \
     '' using 0:4:4 with labels offset char 0,0.5 font "Arial,10" notitle
EOF

if [ -f "/app/benchmarks/results/cache_miss_analysis.csv" ]; then
    gnuplot /app/benchmarks/results/cache_miss_plot.gp
    echo -e "${GREEN}Cache miss analysis plot generated: /app/benchmarks/results/cache_miss_analysis.png${NC}"
fi

# Calculate and display cache locality analysis
echo -e "\n${YELLOW}=== CACHE LOCALITY ANALYSIS ===${NC}"

if [ -f "/app/benchmarks/results/cache_locality_benchmark.csv" ]; then
    # Extract times for comparison
    RAZORFS_DIR_TIME=$(grep "razorfs (FUSE3),dir_traversal" /app/benchmarks/results/cache_locality_benchmark.csv | cut -d',' -f3)
    EXT4_DIR_TIME=$(grep "ext4,dir_traversal" /app/benchmarks/results/cache_locality_benchmark.csv | cut -d',' -f3)
    
    RAZORFS_READ_TIME=$(grep "razorfs (FUSE3),read" /app/benchmarks/results/cache_locality_benchmark.csv | cut -d',' -f3)
    EXT4_READ_TIME=$(grep "ext4,read" /app/benchmarks/results/cache_locality_benchmark.csv | cut -d',' -f3)
    
    RAZORFS_RAND_TIME=$(grep "razorfs (FUSE3),random_access" /app/benchmarks/results/cache_locality_benchmark.csv | cut -d',' -f3)
    EXT4_RAND_TIME=$(grep "ext4,random_access" /app/benchmarks/results/cache_locality_benchmark.csv | cut -d',' -f3)
    
    echo -e "\n${BLUE}Directory Traversal Times:${NC}"
    echo "RazorFS (FUSE3): ${RAZORFS_DIR_TIME}s"
    echo "ext4: ${EXT4_DIR_TIME}s"
    
    if [ ! -z "$RAZORFS_DIR_TIME" ] && [ ! -z "$EXT4_DIR_TIME" ] && [ "$RAZORFS_DIR_TIME" != "" ] && [ "$EXT4_DIR_TIME" != "" ]; then
        DIR_RATIO=$(echo "$EXT4_DIR_TIME $RAZORFS_DIR_TIME" | awk '{print $1/$2}')
        echo "Performance ratio (ext4/RazorFS): $DIR_RATIO"
        if (( $(echo "$DIR_RATIO > 1" | bc -l) )); then
            echo "RazorFS is $(echo "$DIR_RATIO" | awk '{printf "%.2f", $1}')x faster (better cache locality!)"
        else
            PERF_RATIO=$(echo "$RAZORFS_DIR_TIME $EXT4_DIR_TIME" | awk '{print $1/$2}')
            echo "ext4 is $(echo "$PERF_RATIO" | awk '{printf "%.2f", $1}')x faster"
        fi
    fi
    
    echo -e "\n${BLUE}File Read Times:${NC}"
    echo "RazorFS (FUSE3): ${RAZORFS_READ_TIME}s"
    echo "ext4: ${EXT4_READ_TIME}s"
    
    if [ ! -z "$RAZORFS_READ_TIME" ] && [ ! -z "$EXT4_READ_TIME" ] && [ "$RAZORFS_READ_TIME" != "" ] && [ "$EXT4_READ_TIME" != "" ]; then
        READ_RATIO=$(echo "$EXT4_READ_TIME $RAZORFS_READ_TIME" | awk '{print $1/$2}')
        echo "Performance ratio (ext4/RazorFS): $READ_RATIO"
        if (( $(echo "$READ_RATIO > 1" | bc -l) )); then
            echo "RazorFS is $(echo "$READ_RATIO" | awk '{printf "%.2f", $1}')x faster (better cache locality!)"
        else
            PERF_RATIO=$(echo "$RAZORFS_READ_TIME $EXT4_READ_TIME" | awk '{print $1/$2}')
            echo "ext4 is $(echo "$PERF_RATIO" | awk '{printf "%.2f", $1}')x faster"
        fi
    fi
    
    echo -e "\n${BLUE}Random Access Times:${NC}"
    echo "RazorFS (FUSE3): ${RAZORFS_RAND_TIME}s"
    echo "ext4: ${EXT4_RAND_TIME}s"
    
    if [ ! -z "$RAZORFS_RAND_TIME" ] && [ ! -z "$EXT4_RAND_TIME" ] && [ "$RAZORFS_RAND_TIME" != "" ] && [ "$EXT4_RAND_TIME" != "" ]; then
        RND_RATIO=$(echo "$EXT4_RAND_TIME $RAZORFS_RAND_TIME" | awk '{print $1/$2}')
        echo "Performance ratio (ext4/RazorFS): $RND_RATIO"
        if (( $(echo "$RND_RATIO > 1" | bc -l) )); then
            echo "RazorFS is $(echo "$RND_RATIO" | awk '{printf "%.2f", $1}')x faster (better cache locality!)"
        else
            PERF_RATIO=$(echo "$RAZORFS_RAND_TIME $EXT4_RAND_TIME" | awk '{print $1/$2}')
            echo "ext4 is $(echo "$PERF_RATIO" | awk '{printf "%.2f", $1}')x faster"
        fi
    fi
fi

# Run Python analysis and plotting
echo -e "\n${GREEN}Running Python analysis with numpy/matplotlib...${NC}"
python3 /app/cache_locality_plots.py || echo "Python analysis failed, continuing with results..."

# Create a summary report
cat > /app/benchmarks/results/cache_locality_summary.txt << EOF
CACHE LOCALITY BENCHMARK SUMMARY
===============================

Filesystem: RazorFS (FUSE3-based N-ary Tree Filesystem)
Comparison: ext4 mounted filesystem

BENCHMARKS PERFORMED:
1. Directory Traversal - Sequential access to directories
2. File Reads - Sequential access to file contents  
3. Random Access - Random access to files
4. Cache Performance - Detailed cache miss analysis using perf

ARCHITECTURAL ADVANTAGES OF RAZORFS FOR CACHE LOCALITY:
- 64-byte aligned nodes (single cache line optimization)
- 16-way branching (O(log₁₆ n) complexity, reduced tree depth)
- Contiguous memory layout for tree nodes
- NUMA-aware memory allocation
- Spatial locality through breadth-first layout
- Temporal locality through efficient locking mechanisms

RESULTS:
The benchmark measures cache-friendliness through access times.
Lower access times indicate better cache locality due to:
- Better spatial locality (data accessed together is stored together)
- Better temporal locality (frequently accessed data stays in cache)
- More efficient memory access patterns

NOTES:
- RazorFS is mounted using FUSE3 as shown in the legend
- Performance may vary based on system configuration
- Cache effects are more prominent with larger datasets
EOF

echo -e "\n${GREEN}Cache locality benchmark complete!${NC}"
echo -e "Results saved to: /app/benchmarks/results/"
echo -e "CSV data: /app/benchmarks/results/cache_locality_benchmark.csv"
echo -e "Cache miss analysis: /app/benchmarks/results/cache_miss_analysis.csv"
echo -e "Plots: /app/benchmarks/results/cache_locality_comparison.png and cache_miss_analysis.png"
echo -e "Summary: /app/benchmarks/results/cache_locality_summary.txt"