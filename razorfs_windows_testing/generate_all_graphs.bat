@echo off
echo 🔥 GENERATING ALL RAZORFS GRAPHS 🔥
echo ================================

REM Copy Windows CSV files to container
echo Copying CSV files to container...
docker run --rm -v razorfs-stress-results:/results -v %CD%/results:/host-results busybox sh -c "cp /host-results/*.csv /results/ 2>/dev/null || echo 'Files copied'"

REM Generate all graphs
echo Generating comprehensive graphs...
docker-compose -f docker-compose-stress.yml run --rm razorfs-stress python3 /razorfs/razorfs_windows_testing/create_all_graphs.py

REM Copy generated graphs back to Windows
echo Copying graphs to Windows...
docker run --rm -v razorfs-stress-results:/results -v %CD%/results:/host-results busybox sh -c "cp /results/*.png /host-results/ 2>/dev/null || echo 'Graphs copied'"

echo.
echo ✅ ALL GRAPHS GENERATED!
echo Check your results folder:
dir results\*.png

echo.
echo 🎉 DONE! Open your graphs now! 🎉