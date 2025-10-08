set terminal pngcairo enhanced font 'Arial,12' size 800,600
set output 'graphs/compression_comparison.png'
set title "Filesystem Compression Efficiency\n{/*0.8 Test: 1MB+ compressed archive (git-2.43.0.tar.gz)}"
set ylabel "Disk Usage (MB)"
set xlabel "Filesystem"
set y2label "Compression Ratio"
set ytics nomirror
set y2tics
set style data histogram
set style histogram cluster gap 1
set style fill solid border -1
set boxwidth 0.9
set grid y
set key outside right top

plot 'data/compression_20251007_220422.dat' using 2:xtic(1) title 'Disk Usage (MB)' axes x1y1 linecolor rgb "#3498db", \
     '' using 3:xtic(1) title 'Compression Ratio' axes x1y2 linecolor rgb "#e74c3c"
