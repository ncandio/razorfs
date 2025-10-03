@echo off
echo ========================================
echo RAZORFS O(log n) Analysis & Graphs
echo ========================================
echo.
echo Generating O(log n) complexity analysis graphs...
echo.

docker run --rm -v razorfs-stress-results:/results razorfs_windows_testing-razorfs-stress python3 /razorfs/razorfs_windows_testing/generate_ologn_graphs.py

echo.
echo Analysis completed. Check the results volume for generated graphs.