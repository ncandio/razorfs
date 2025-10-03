# RAZORFS Visualization Scripts
# Generates comparison graphs for all benchmarks

set terminal pngcairo enhanced font 'Verdana,10' size 1200,800
set output '/results/razorfs_comparison.png'

set multiplot layout 2,2 title "RAZORFS vs Traditional Filesystems" font ",14"

# 1. Metadata Operations
set title "Metadata Operations Performance"
set ylabel "Time (ms)"
set xlabel "Operation"
set style data histogram
set style histogram cluster gap 1
set style fill solid border -1
set xtics rotate by -45
set grid y

plot '/results/metadata_ext4.dat' using 2:xtic(1) title 'EXT4' lc rgb "#1f77b4", \
     '/results/metadata_reiserfs.dat' using 2:xtic(1) title 'ReiserFS' lc rgb "#ff7f0e", \
     '/results/metadata_btrfs.dat' using 2:xtic(1) title 'Btrfs' lc rgb "#2ca02c", \
     '/results/metadata_razorfs.dat' using 2:xtic(1) title 'RAZORFS' lc rgb "#d62728"

# 2. O(log n) Scalability
set title "O(log n) Scalability Test"
set ylabel "Lookup Time (μs)"
set xlabel "Number of Files"
set key top left
set logscale x
set logscale y
unset xtics
set xtics
set grid

plot '/results/ologn_ext4.dat' with linespoints title 'EXT4' lw 2 lc rgb "#1f77b4", \
     '/results/ologn_reiserfs.dat' with linespoints title 'ReiserFS' lw 2 lc rgb "#ff7f0e", \
     '/results/ologn_btrfs.dat' with linespoints title 'Btrfs' lw 2 lc rgb "#2ca02c", \
     '/results/ologn_razorfs.dat' with linespoints title 'RAZORFS' lw 2 lc rgb "#d62728"

# 3. I/O Throughput
set title "I/O Throughput"
set ylabel "Throughput (MB/s)"
set xlabel "Filesystem"
set style data histogram
set style histogram cluster gap 1
unset logscale x
unset logscale y
set grid y

plot '/results/io_ext4.dat' using 2:xtic(1) title 'Write' lc rgb "#1f77b4", \
     '' using 3 title 'Read' lc rgb "#ff7f0e"

# 4. Summary Statistics
set title "Feature Comparison"
set ylabel "Score (normalized)"
set xlabel "Feature"
unset logscale
set yrange [0:1.2]
set style fill solid 0.5

plot '/results/summary.dat' using 2:xtic(1) with boxes title 'RAZORFS' lc rgb "#d62728"

unset multiplot
