# NUMA and Cache Performance Analysis
set terminal pngcairo size 1600,1200 enhanced font 'Arial,14'
set output './analysis_charts/numa_cache_performance_analysis.png'

set multiplot layout 2,2 title "RAZORFS NUMA Awareness & Cache-Friendly Performance Analysis" font ",18"

# Chart D1: Cache Hit Ratio vs Core Count
set title "Cache Hit Ratio Scaling"
set xlabel "Number of CPU Cores"
set ylabel "Cache Hit Ratio (%)"
set grid
set yrange [80:95]

plot './analysis_charts/numa_cache_performance.dat' using 1:2 with linespoints    title "Cache Hit Ratio" 

# Chart D2: Memory Bandwidth Utilization
set title "Memory Bandwidth Scaling"
set xlabel "Number of CPU Cores"
set ylabel "Memory Bandwidth (GB/s)"
set grid
set logscale y

plot './analysis_charts/numa_cache_performance.dat' using 1:3 with linespoints    title "Memory Bandwidth" 

# Chart D3: NUMA Efficiency
set title "NUMA Efficiency Scaling"
set xlabel "Number of CPU Cores"
set ylabel "NUMA Efficiency (%)"
set grid
set yrange [80:105]

plot './analysis_charts/numa_cache_performance.dat' using 1:4 with linespoints    title "NUMA Efficiency" 

# Chart D4: Cache Locality Performance
set title "Cache Locality Optimization"
set xlabel "Number of CPU Cores"
set ylabel "Cache Locality Score (%)"
set grid
set yrange [80:100]

plot './analysis_charts/numa_cache_performance.dat' using 1:5 with linespoints    title "Cache Locality" 

unset multiplot
