set terminal pngcairo enhanced font 'Arial,12' size 1000,400
set output 'graphs/persistence_verification.png'
set title "File Persistence Verification (1MB Random Data)\n{/*0.8 Test: Create 1MB file → Unmount → Remount → Verify MD5 checksum}"
set xlabel "Mount State"
set ylabel "MD5 Checksum"
set style data boxes
set style fill solid 0.5
set grid y
set key off
set xtics rotate by -45
set yrange [0:1]
set format y ""

# This will need custom labels based on actual checksums
set label "RAZORFS\nPersistence\nTest" at screen 0.5, screen 0.5 center font "Arial,16"
