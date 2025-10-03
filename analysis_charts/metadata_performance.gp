# Metadata Performance: ext4 vs RAZORFS
set terminal pngcairo size 1600,1200 enhanced font 'Arial,14'
set output './analysis_charts/metadata_performance_comparison.png'

set multiplot layout 2,2 title "Metadata Performance Analysis: ext4 vs RAZORFS" font ",18"

# Chart C1: Metadata Operation Time
set title "Metadata Operation Time Comparison"
set xlabel "Number of Files"
set ylabel "Time (milliseconds)"
set logscale y
set grid
set key right bottom

plot './analysis_charts/metadata_performance.dat' using 1:2 with linespoints    title "ext4" , \
     './analysis_charts/metadata_performance.dat' using 1:3 with linespoints    title "RAZORFS" 

# Chart C2: Memory Usage for Metadata
set title "Metadata Memory Usage"
set xlabel "Number of Files"
set ylabel "Memory Usage (MB)"
set grid
unset logscale y

plot './analysis_charts/metadata_performance.dat' using 1:4 with linespoints    title "ext4 Memory" , \
     './analysis_charts/metadata_performance.dat' using 1:5 with linespoints    title "RAZORFS Memory" 

# Chart C3: Performance Improvement Ratio
set title "RAZORFS Performance Advantage"
set xlabel "Number of Files"
set ylabel "Speed Improvement Ratio (ext4/RAZORFS)"
set grid
set yrange [1:6]

plot './analysis_charts/metadata_performance.dat' using 1:($2/$3) with linespoints    title "Speed Improvement" 

# Chart C4: Memory Efficiency Ratio
set title "RAZORFS Memory Efficiency"
set xlabel "Number of Files"
set ylabel "Memory Efficiency Ratio (ext4/RAZORFS)"
set grid
set yrange [1:4]

plot './analysis_charts/metadata_performance.dat' using 1:($4/$5) with linespoints    title "Memory Efficiency" 

unset multiplot
