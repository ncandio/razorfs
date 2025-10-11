set terminal pngcairo enhanced font 'Arial Bold,12' size 1000,650 background rgb "#f5f5f5"
set output 'plots/s3_upload_performance.png'

set title "S3 Upload Performance Comparison\\n{/*0.7 Simulated test data for demonstration}" font "Arial Bold,16" textcolor rgb "#2c3e50"

set xlabel "File Size" font "Arial Bold,12" textcolor rgb "#34495e"
set ylabel "Upload Time (milliseconds)" font "Arial Bold,12" textcolor rgb "#34495e"
set y2label "Throughput (Mbps)" font "Arial,11" textcolor rgb "#7f8c8d"

set ytics nomirror font "Arial,10"
set y2tics font "Arial,10"
set xtics ("1MB" 0, "10MB" 1, "100MB" 2, "1GB" 3)
set xrange [-0.5:3.5]

set style fill solid 0.8 border -1
set boxwidth 0.6 relative
set grid ytics lw 1 lc rgb "#bdc3c7"

set key outside right top box font "Arial,10"

# Plot upload time
plot '-' using 1:2 title 'Upload Time (ms)' with boxes lc rgb "#3498db", \
     '' using 1:2:(sprintf("%.0f ms", $2)) with labels offset 0,1 font "Arial Bold,10" notitle
0 45
1 180
2 1200
3 8500
eod