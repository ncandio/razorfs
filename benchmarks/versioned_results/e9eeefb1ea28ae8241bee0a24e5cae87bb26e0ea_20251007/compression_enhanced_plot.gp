set terminal pngcairo enhanced font 'Arial Bold,12' size 1000,650 background rgb "#f5f5f5"
set output 'graphs/compression_effectiveness.png'

set title "Compression Effectiveness Comparison\n{/*0.7 Real-world test: 10MB Git archive (git-2.43.0.tar.gz)}" font "Arial Bold,16" textcolor rgb "#2c3e50"

set ylabel "Disk Space Usage (MB)" font "Arial Bold,12" textcolor rgb "#34495e"
set xlabel "Filesystem" font "Arial Bold,12" textcolor rgb "#34495e"
set y2label "Space Savings (%)" font "Arial,11" textcolor rgb "#7f8c8d"

set ytics nomirror font "Arial,10"
set y2tics font "Arial,10"
set xtics font "Arial,11"

set style fill solid 0.8 border -1
set boxwidth 0.6 relative
set grid ytics lw 1 lc rgb "#bdc3c7"

set key outside right top box font "Arial,10"

# Read actual data from benchmark
plot 'data/compression_20251007_220422.dat' using 2:xtic(1) title 'Disk Usage (MB)' with boxes lc rgb "#3498db", \
     '' using ($0):($2):(sprintf("%.1f MB", $2)) with labels offset 0,1 font "Arial Bold,10" notitle
