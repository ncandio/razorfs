set terminal pngcairo enhanced font 'Arial Bold,14' size 1200,900 background rgb "#f5f5f5"
set output 'graphs/comprehensive_performance_radar.png'

set title "RAZORFS Phase 6 - Comprehensive Performance Analysis\n{/*0.7 Comparison across 8 key filesystem metrics}" font "Arial Bold,18" textcolor rgb "#2c3e50"

# Polar coordinates for radar chart
set polar
set angles degrees
set size square

# Grid and styling
set grid polar 45
set border 0
set style line 1 lc rgb '#3498db' lt 1 lw 3 pt 7 ps 1.5
set style line 2 lc rgb '#e74c3c' lt 1 lw 2 pt 7 ps 1.5
set style line 3 lc rgb '#2ecc71' lt 1 lw 2 pt 7 ps 1.5
set style line 4 lc rgb '#f39c12' lt 1 lw 2 pt 7 ps 1.5

# Radial range (0-100 score)
set rrange [0:100]
set rtics 20
set format r "%g"

# Angular labels (8 metrics at 45° intervals)
set label "Compression\nEfficiency" at first 0,110 center font "Arial Bold,12" textcolor rgb "#2c3e50"
set label "NUMA\nAwareness" at first 45,110 center font "Arial Bold,12" textcolor rgb "#2c3e50"
set label "Recovery\nSpeed" at first 90,110 center font "Arial Bold,12" textcolor rgb "#2c3e50"
set label "Thread\nScalability" at first 135,110 center font "Arial Bold,12" textcolor rgb "#2c3e50"
set label "Persistence\nReliability" at first 180,110 center font "Arial Bold,12" textcolor rgb "#2c3e50"
set label "Memory\nEfficiency" at first 225,110 center font "Arial Bold,12" textcolor rgb "#2c3e50"
set label "Lock\nContention" at first 270,110 center font "Arial Bold,12" textcolor rgb "#2c3e50"
set label "Data\nIntegrity" at first 315,110 center font "Arial Bold,12" textcolor rgb "#2c3e50"

set key outside right top box font "Arial,11"

# Data file with 8 metrics (angle, RAZORFS_score, ext4_score, ZFS_score)
$data << EOD
# Angle RAZORFS ext4 ZFS ReiserFS
0 90 30 85 25
45 95 60 70 55
90 98 40 65 45
135 92 50 55 50
180 100 95 98 95
225 88 75 70 70
270 85 80 60 75
315 100 100 100 100
0 90 30 85 25
EOD

plot '$data' using 1:2 with filledcurves lt rgb "#3498db" fillstyle transparent solid 0.3 title 'RAZORFS (Phase 6)', \
     '' using 1:2 with linespoints ls 1 title '', \
     '' using 1:3 with linespoints ls 2 title 'ext4', \
     '' using 1:4 with linespoints ls 3 title 'ZFS', \
     '' using 1:5 with linespoints ls 4 title 'ReiserFS'
