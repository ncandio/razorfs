@echo off
echo ========================================
echo RAZORFS O(log n) Analysis with GnuPlot
echo ========================================
echo.
echo Generating performance analysis graphs with gnuplot...
echo.

docker run --rm -v razorfs-stress-results:/results razorfs_windows_testing-razorfs-stress bash -c "./razorfs_windows_testing/generate_ologn_graphs_gnuplot.sh"

echo.
echo Analysis completed with gnuplot graphs!