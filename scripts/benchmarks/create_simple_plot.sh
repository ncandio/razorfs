#!/bin/bash
# create_simple_plot.sh - Create a plot from the cache benchmark results

# Create gnuplot script for the benchmark results
cat > /tmp/cache_comparison.gp << 'EOF'
set terminal pngcairo enhanced font 'Arial,12' size 1000,800
set output 'cache_benchmark_results.png'

set title "Cache Locality Comparison: RazorFS vs ext4\n(Log scale for better visualization)" font "Arial Bold,14"

set xlabel "Operation Type" font "Arial,12"
set ylabel "Time (seconds)" font "Arial,12"
set logscale y
set grid ytics xtics lw 1 lc rgb "#bdc3c7"

set key inside right top box font "Arial,10" spacing 1.5
set boxwidth 0.6 relative
set style fill solid 0.7 border -1

# Define colors
set style line 1 lc rgb '#3498db' 
set style line 2 lc rgb '#e74c3c' 

# Create inline data for plotting
$data << EOD
operation razorfs_time ext4_time
dir_traversal 0.235357497 0.191631697
file_read 0.196691697 0.141040498
random_access 0.001700200 0.073614999
EOD

set xtics rotate by -45

# Plot the data - RazorFS and ext4 times for each operation
plot $data using 2:xticlabels(1) with boxes title "razorfs (FUSE3)" ls 1, \
     $data using 3:xticlabels(1) with boxes title "ext4" ls 2
EOF

# Generate the plot
gnuplot /tmp/cache_comparison.gp
echo "Plot generated: cache_benchmark_results.png"

# Move to benchmarks directory
if [ -d "benchmarks/results" ]; then
    mv cache_benchmark_results.png benchmarks/results/ 2>/dev/null || true
    echo "Plot moved to benchmarks/results/cache_benchmark_results.png"
else
    mkdir -p benchmarks/results
    mv cache_benchmark_results.png benchmarks/results/ 2>/dev/null || true
    echo "Plot moved to benchmarks/results/cache_benchmark_results.png"
fi