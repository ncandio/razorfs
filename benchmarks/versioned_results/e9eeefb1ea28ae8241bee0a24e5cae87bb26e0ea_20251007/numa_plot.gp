set terminal pngcairo enhanced font 'Arial,12' size 800,600
set output 'graphs/numa_comparison.png'
set title "NUMA Friendliness & Memory Locality\n{/*0.8 Higher NUMA score = better memory locality, Lower latency = faster access}"
set ylabel "NUMA Score (0-100)"
set xlabel "Filesystem"
set y2label "Access Latency (nanoseconds)"
set ytics nomirror
set y2tics
set yrange [0:100]
set style data histogram
set style histogram cluster gap 1
set style fill solid border -1
set boxwidth 0.9
set grid y
set key outside right top

plot 'data/numa_20251007_220422.dat' using 2:xtic(1) title 'NUMA Score' axes x1y1 linecolor rgb "#f39c12", \
     '' using 3:xtic(1) title 'Access Latency (ns)' axes x1y2 linecolor rgb "#e67e22"
