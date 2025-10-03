# RAZORFS O(log n) Performance Analysis
set terminal pngcairo size 1600,1200 enhanced font 'Arial,14'
set output './analysis_charts/ologn_features_analysis.png'

set multiplot layout 2,2 title "RAZORFS O(log n) N-ary Tree Performance Analysis" font ",18"

# Chart B1: Lookup Time vs File Count (Log Scale)
set title "Directory Lookup Performance (Logarithmic Scaling)"
set xlabel "Number of Files in Directory"
set ylabel "Lookup Time (milliseconds)"
set logscale x
set grid
set key right bottom

plot './analysis_charts/ologn_performance.dat' using 1:2 with linespoints title "RAZORFS Actual", \
     './analysis_charts/ologn_performance.dat' using 1:3 with lines title "Theoretical O(log n)"

# Chart B2: Performance Scaling Factor
set title "RAZORFS vs Theoretical O(log n) Scaling"
set xlabel "Number of Files"
set ylabel "Performance Ratio (Actual/Theoretical)"
set logscale x
set grid
set yrange [0.5:1.5]

plot './analysis_charts/ologn_performance.dat' using 1:($2/$3) with linespoints title "RAZORFS Efficiency"

# Chart B3: Lookup Time Linear View
set title "Lookup Time Growth (Linear View)"
set xlabel "Number of Files"
set ylabel "Lookup Time (ms)"
unset logscale x
set grid

plot './analysis_charts/ologn_performance.dat' using 1:2 with linespoints title "RAZORFS Performance", \
     './analysis_charts/ologn_performance.dat' using 1:($1*0.0004) with lines title "Linear O(n)"

# Chart B4: N-ary Tree Efficiency
set title "N-ary Tree Structure Efficiency"
set xlabel "Directory Size"
set ylabel "Operations per Second"
set grid

plot './analysis_charts/ologn_performance.dat' using 1:(1000/$2) with linespoints title "Ops/Second"

unset multiplot
