set terminal pngcairo enhanced font 'Arial Bold,12' size 1000,650 background rgb "#f5f5f5"
set output 'graphs/s3_upload_performance.png'

set title "S3 Upload Performance Comparison\\n{/*0.7 Simulated test data for demonstration}" font "Arial Bold,16" textcolor rgb "#2c3e50"

set xlabel "File Size" font "Arial Bold,12" textcolor rgb "#34495e"
set ylabel "Upload Time (milliseconds)" font "Arial Bold,12" textcolor rgb "#34495e"
set y2label "Throughput (Mbps)" font "Arial,11" textcolor rgb "#7f8c8d"

set ytics nomirror font "Arial,10"
set y2tics font "Arial,10"
set xtics font "Arial,11"

set style fill solid 0.8 border -1
set boxwidth 0.6 relative
set grid ytics lw 1 lc rgb "#bdc3c7"

set key outside right top box font "Arial,10"

# Plot upload time and throughput
plot 'data/s3_upload_sample.dat' using 2:xtic(1) title 'Upload Time (ms)' with boxes lc rgb "#3498db", \
     '' using ($0):($2):(sprintf("%.0f ms", $2)) with labels offset 0,1 font "Arial Bold,10" notitle, \
     '' using 3 axes x1y2 title 'Throughput (Mbps)' with linespoints lc rgb "#e74c3c" pt 7 ps 1.2 lw 2
