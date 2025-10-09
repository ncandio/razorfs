set terminal pngcairo enhanced font 'Arial Bold,12' size 1000,650 background rgb "#f5f5f5"
set output 'graphs/hybrid_storage_comparison.png'

set title "Hybrid Storage Performance Comparison\\n{/*0.7 Local vs S3 vs Hybrid Storage Performance}" font "Arial Bold,16" textcolor rgb "#2c3e50"

set xlabel "Storage Type" font "Arial Bold,12" textcolor rgb "#34495e"
set ylabel "Average Access Time (milliseconds)" font "Arial Bold,12" textcolor rgb "#34495e"

set ytics nomirror font "Arial,10"
set xtics font "Arial,11"

set style fill solid 0.8 border -1
set boxwidth 0.6 relative
set grid ytics lw 1 lc rgb "#bdc3c7"

set key outside right top box font "Arial,10"

# Plot hybrid comparison
plot 'data/hybrid_comparison_sample.dat' using 2:xtic(1) title 'Small File (1MB)' with boxes lc rgb "#3498db", \
     '' using 3:xtic(1) title 'Medium File (10MB)' with boxes lc rgb "#e74c3c", \
     '' using 4:xtic(1) title 'Large File (100MB)' with boxes lc rgb "#2ecc71"
