set terminal pngcairo enhanced font 'Verdana,12' size 800,600
set output '/tmp/razorfs-results/razorfs_ologn_validation.png'
set title "RAZORFS O(log n) Scalability Validation"
set xlabel "Number of Files"
set ylabel "Lookup Time per Operation (μs)"
set grid
set datafile separator ","
set key top left
plot '/tmp/razorfs-results/ologn_razorfs.csv' using 1:2 with linespoints \
     title 'RAZORFS (O(log₁₆ n))' linewidth 3 pointtype 7 pointsize 1.5 linecolor rgb "#27AE60"
