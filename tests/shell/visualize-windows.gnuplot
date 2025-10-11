#!/usr/bin/gnuplot
# RAZORFS Windows Comparison Visualization

set terminal pngcairo enhanced font 'Verdana,10' size 1200,800
set output '/charts/razorfs_comparison.png'

set multiplot layout 2,2 title "RAZORFS vs Traditional Filesystems Comparison" font ",14"

# ====== Panel 1: Metadata Operations ======
set title "Metadata Performance (lower is better)"
set ylabel "Time (ms)"
set style data histograms
set style fill solid 1.0 border -1
set xtic rotate by -45 scale 0
set grid y

plot '/data/metadata_ext4.dat' using 2:xtic(1) title 'ext4' linecolor rgb "#E74C3C", \
     '/data/metadata_reiserfs.dat' using 2:xtic(1) title 'reiserfs' linecolor rgb "#3498DB", \
     '/data/metadata_btrfs.dat' using 2:xtic(1) title 'btrfs' linecolor rgb "#F39C12", \
     '/data/metadata_razorfs.dat' using 2:xtic(1) title 'RAZORFS' linecolor rgb "#27AE60"

# ====== Panel 2: O(log n) Scalability ======
set title "O(log n) Lookup Time Scalability"
set xlabel "Number of Files"
set ylabel "Time per Operation (μs)"
set key top left
set grid
unset xtic

plot '/data/ologn_ext4.dat' using 1:2 with linespoints title 'ext4' linewidth 2 linecolor rgb "#E74C3C", \
     '/data/ologn_reiserfs.dat' using 1:2 with linespoints title 'reiserfs' linewidth 2 linecolor rgb "#3498DB", \
     '/data/ologn_btrfs.dat' using 1:2 with linespoints title 'btrfs' linewidth 2 linecolor rgb "#F39C12", \
     '/data/ologn_razorfs.dat' using 1:2 with linespoints title 'RAZORFS' linewidth 2 linecolor rgb "#27AE60"

# ====== Panel 3: I/O Throughput ======
set title "I/O Throughput (higher is better)"
set xlabel ""
set ylabel "MB/s"
set style data histograms
set style histogram cluster gap 1
set style fill solid 1.0 border -1
set xtic rotate by -45 scale 0
set key top left
set grid y

plot '/data/io_ext4.dat' using 2:xtic(1) title 'ext4' linecolor rgb "#E74C3C", \
     '/data/io_reiserfs.dat' using 2:xtic(1) title 'reiserfs' linecolor rgb "#3498DB", \
     '/data/io_btrfs.dat' using 2:xtic(1) title 'btrfs' linecolor rgb "#F39C12", \
     '/data/io_razorfs.dat' using 2:xtic(1) title 'RAZORFS' linecolor rgb "#27AE60"

# ====== Panel 4: Feature Summary ======
set title "RAZORFS Key Features"
set ylabel "Score (normalized)"
set style data histograms
set style fill solid 1.0 border -1
set xtic rotate by -45 scale 0
set yrange [0:100]
set grid y

# Create feature summary data inline
$features << EOD
Feature Score
O(log_n) 95
Compression 85
NUMA 80
MT_Safety 90
Cache_Opt 88
EOD

plot $features using 2:xtic(1) notitle linecolor rgb "#27AE60"

unset multiplot
