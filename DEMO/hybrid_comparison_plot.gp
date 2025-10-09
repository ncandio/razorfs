set terminal pngcairo enhanced font 'Arial Bold,12' size 1000,650 background rgb "#f5f5f5"
set output 'plots/hybrid_storage_comparison.png'

set title "Hybrid Storage Performance Comparison\\n{/*0.7 Local vs S3 vs Hybrid Storage Performance}" font "Arial Bold,16" textcolor rgb "#2c3e50"

set xlabel "Storage Type" font "Arial Bold,12" textcolor rgb "#34495e"
set ylabel "Average Access Time (milliseconds)" font "Arial Bold,12" textcolor rgb "#34495e"

set ytics nomirror font "Arial,10"
set xtics ("Local" 0, "S3" 1, "Hybrid" 2)

set style fill solid 0.8 border -1
set boxwidth 0.6 relative
set grid ytics lw 1 lc rgb "#bdc3c7"

set key outside right top box font "Arial,10"

# Plot for small files (1MB)
plot '-' using 1:2 title 'Small File (1MB)' with boxes lc rgb "#3498db", \
     '' using 1:2:(sprintf("%.0f ms", $2)) with labels offset 0,1 font "Arial Bold,10" notitle
0 2
1 45
2 5
eod