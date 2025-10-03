# RAZORFS Comprehensive Performance Summary
set terminal pngcairo size 2000,1400 enhanced font 'Arial,16'
set output './analysis_charts/razorfs_comprehensive_summary.png'

set multiplot layout 3,2 title "RAZORFS Comprehensive Performance Analysis Summary" font ",20"

# Summary 1: Feature Performance Radar
set title "RAZORFS Feature Performance Score"
set xlabel "Feature"
set ylabel "Performance Score (0-100)"
set style data histograms
set style histogram clustered gap 1
set style fill solid border -1
set boxwidth 0.8
set xtic rotate by -45 scale 0
set grid y
set yrange [0:100]

plot './analysis_charts/razorfs_features.dat' using 2:xtic(1) title "Performance Score" 

# Summary 2: Memory Efficiency
set title "Memory Efficiency by Feature"
set ylabel "Memory Efficiency (0-100)"

plot './analysis_charts/razorfs_features.dat' using 3:xtic(1) title "Memory Efficiency" 

# Summary 3: Scalability Analysis
set title "Scalability by Feature"
set ylabel "Scalability Score (0-100)"

plot './analysis_charts/razorfs_features.dat' using 4:xtic(1) title "Scalability" 

# Summary 4: Filesystem Comparison Overview
set title "Filesystem Comparison Overview"
set ylabel "Overall Score"
set xlabel "Filesystem"

plot './analysis_charts/compression_comparison.dat' using (($2 * 15) + (120 - $3) + (35 - $4)):xtic(1) title "Combined Score" 

# Summary 5: O(log n) Advantage Visualization
set title "O(log n) Performance Advantage"
set xlabel "Directory Size"
set ylabel "Performance Ratio vs Linear"
set logscale x
set grid

plot './analysis_charts/ologn_performance.dat' using 1:(($1*0.0004)/$2) with linespoints    title "RAZORFS vs Linear O(n)" 

# Summary 6: Multi-Core Scaling Summary
set title "Multi-Core Performance Summary"
set xlabel "CPU Cores"
set ylabel "Normalized Performance"
unset logscale x

plot './analysis_charts/numa_cache_performance.dat' using 1:($2/100 + $4/100 + $5/100)/3 with linespoints    title "Average Performance" 

unset multiplot
