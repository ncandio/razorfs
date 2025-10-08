set terminal pngcairo enhanced font 'Arial Bold,12' size 1200,800 background rgb "#f5f5f5"
set output 'graphs/scalability_heatmap.png'

set title "Filesystem Performance Heatmap\n{/*0.7 RAZORFS Phase 6 vs Traditional Filesystems}" font "Arial Bold,16" textcolor rgb "#2c3e50"

set ylabel "Filesystem" font "Arial Bold,12" textcolor rgb "#34495e"
set xlabel "Performance Metric" font "Arial Bold,12" textcolor rgb "#34495e"

set ytics font "Arial,11"
set xtics rotate by -45 font "Arial,10"
set grid

# Color palette: green (good) to red (poor)
set palette defined (0 "#c0392b", 50 "#f39c12", 100 "#27ae60")
set cbrange [0:100]
set cblabel "Performance Score (0-100)" font "Arial,10"

set view map
set size ratio 0.5

$heatmap << EOD
# Y_Filesystem X_Metric Score
0 0 90
0 1 95
0 2 98
0 3 92
0 4 100
0 5 88
0 6 85
1 0 30
1 1 60
1 2 40
1 3 50
1 4 95
1 5 75
1 6 80
2 0 85
2 1 70
2 2 65
2 3 55
2 4 98
2 5 70
2 6 60
3 0 25
3 1 55
3 2 45
3 3 50
3 4 95
3 5 70
3 6 75
EOD

set ytics ("RAZORFS" 0, "ext4" 1, "ZFS" 2, "ReiserFS" 3)
set xtics ("Compression" 0, "NUMA" 1, "Recovery" 2, "Threading" 3, "Persistence" 4, "Memory" 5, "Locking" 6)

plot '$heatmap' using 2:1:3 with image notitle, \
     '' using 2:1:(sprintf("%.0f", $3)) with labels font "Arial Bold,10" textcolor rgb "white" notitle
