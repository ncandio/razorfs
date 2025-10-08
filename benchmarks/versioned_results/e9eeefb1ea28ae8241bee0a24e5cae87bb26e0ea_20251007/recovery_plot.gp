set terminal pngcairo enhanced font 'Arial,12' size 800,600
set output 'graphs/recovery_comparison.png'
set title "Filesystem Recovery Performance\n{/*0.8 Simulation: 10-second crash recovery test with data integrity check}"
set ylabel "Recovery Time (milliseconds)"
set xlabel "Filesystem"
set y2label "Success Rate (%)"
set ytics nomirror
set y2tics
set y2range [0:100]
set style data histogram
set style histogram cluster gap 1
set style fill solid border -1
set boxwidth 0.9
set grid y
set key outside right top

plot 'data/recovery_20251007_220422.dat' using 2:xtic(1) title 'Recovery Time (ms)' axes x1y1 linecolor rgb "#9b59b6", \
     '' using 3:xtic(1) title 'Success Rate (%)' axes x1y2 linecolor rgb "#2ecc71"
