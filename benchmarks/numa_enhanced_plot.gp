set terminal pngcairo enhanced font 'Arial Bold,12' size 1000,650 background rgb "#f5f5f5"
set output 'graphs/memory_numa_analysis.png'

set title "NUMA Memory Locality Analysis\n{/*0.7 Lower latency = better memory placement on NUMA systems}" font "Arial Bold,16" textcolor rgb "#2c3e50"

set ylabel "Memory Access Latency (nanoseconds)" font "Arial Bold,12" textcolor rgb "#34495e"
set xlabel "Filesystem" font "Arial Bold,12" textcolor rgb "#34495e"
set y2label "NUMA Optimization Score (0-100)" font "Arial,11" textcolor rgb "#7f8c8d"

set ytics nomirror font "Arial,10"
set y2tics 0,20,100 font "Arial,10"
set xtics font "Arial,11"
set y2range [0:100]

set style data histogram
set style histogram cluster gap 1
set style fill solid 0.8 border -1
set boxwidth 0.9 relative

set grid ytics lw 1 lc rgb "#bdc3c7"
set key outside right top box font "Arial,10"

plot 'data/numa_.dat' using 3:xtic(1) title 'Access Latency (ns)' axes x1y1 lc rgb "#e74c3c", \
     '' using 2:xtic(1) title 'NUMA Score' axes x1y2 lc rgb "#27ae60"
