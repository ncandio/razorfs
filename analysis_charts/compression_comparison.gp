# RAZORFS vs Traditional Filesystems - Compression/Memory/Metadata Analysis
set terminal pngcairo size 1600,1200 enhanced font 'Arial,14'
set output './analysis_charts/compression_memory_metadata_comparison.png'

set multiplot layout 2,2 title "RAZORFS vs Traditional Filesystems: Compression, Memory & Metadata Analysis" font ",18"

# Chart A1: Compression Ratio Comparison
set title "Compression Effectiveness"
set ylabel "Compression Ratio (x)"
set xlabel "Filesystem"
set style data histograms
set style histogram clustered gap 1
set style fill solid border -1
set boxwidth 0.8
set xtic rotate by -45 scale 0
set grid y
set yrange [0:3]

plot './analysis_charts/compression_comparison.dat' using 2:xtic(1) title "Compression Ratio" 

# Chart A2: Memory Usage Comparison
set title "Memory Usage Efficiency"
set ylabel "Memory Usage (Relative %)"
set xlabel "Filesystem"
set yrange [0:160]

plot './analysis_charts/compression_comparison.dat' using 3:xtic(1) title "Memory Usage" 

# Chart A3: Metadata Overhead
set title "Metadata Overhead"
set ylabel "Metadata Overhead (%)"
set xlabel "Filesystem"
set yrange [0:30]

plot './analysis_charts/compression_comparison.dat' using 4:xtic(1) title "Metadata Overhead" 

# Chart A4: Combined Efficiency Score
set title "Overall Efficiency Score"
set ylabel "Efficiency Score"
set xlabel "Filesystem"
set yrange [0:100]

# Calculate efficiency score: (compression_ratio * 20) + (100 - memory_usage) + (30 - metadata_overhead)
plot './analysis_charts/compression_comparison.dat' using (($2 * 20) + (100 - $3) + (30 - $4)):xtic(1) title "Overall Score" 

unset multiplot
