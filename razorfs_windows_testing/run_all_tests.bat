@echo off
REM RAZORFS Complete Test Suite - ALL TESTS IN ONE SHOT
echo ================================================================
echo 🚀 RAZORFS COMPLETE TEST SUITE - ALL TESTS + ALL GRAPHS 🚀
echo ================================================================
echo.
echo This will run ALL tests and generate ALL graphs:
echo ✅ Diagnostics Test
echo ✅ Throughput Comparison (RAZOR vs EXT4 vs BTRFS)  
echo ✅ Metadata Performance Test
echo ✅ Fast 6-Filesystem Analysis
echo ✅ Generate ALL Graphs
echo.
set /p confirm="Continue with complete test suite? This will take 20-30 minutes. (y/N): "
if /i not "%confirm%"=="y" (
    echo Test suite cancelled.
    goto end
)

echo.
echo 🔍 Phase 1: Running Diagnostics Test...
echo ========================================
docker-compose -f docker-compose-stress.yml run --rm razorfs-stress diagnostics

echo.
echo 📊 Phase 2: Throughput Comparison Test...
echo =========================================
docker-compose -f docker-compose-stress.yml run --rm razorfs-stress throughput-comparison-test

echo.
echo 🗃️ Phase 3: Metadata Performance Test...
echo ========================================
docker-compose -f docker-compose-stress.yml run --rm razorfs-stress metadata-test

echo.
echo ⚡ Phase 4: Fast 6-Filesystem Analysis...
echo ========================================
docker-compose -f docker-compose-stress.yml run --rm razorfs-stress fast-test

echo.
echo 📈 Phase 5: Generating ALL Graphs...
echo ===================================
echo Copying all CSV files to container...
docker run --rm -v razorfs-stress-results:/results -v %CD%/results:/host-results busybox sh -c "cp /host-results/*.csv /results/ 2>/dev/null || echo 'Files copied'"

echo Generating comprehensive graphs...
docker-compose -f docker-compose-stress.yml run --rm razorfs-stress python3 /razorfs/razorfs_windows_testing/create_all_graphs.py

echo Copying all graphs back to Windows...
docker run --rm -v razorfs-stress-results:/results -v %CD%/results:/host-results busybox sh -c "cp /results/*.png /host-results/ 2>/dev/null || echo 'Graphs copied'"

echo.
echo 🎉 COMPLETE TEST SUITE FINISHED! 🎉
echo ==================================
echo.
echo Results Summary:
echo ================
echo CSV Data Files:
dir results\*.csv 2>nul || echo No CSV files found

echo.
echo PNG Graph Files:
dir results\*.png 2>nul || echo No PNG files found

echo.
echo 📊 Opening Performance Dashboard...
echo ==================================
for %%f in (results\complete_throughput_comparison.png results\complete_metadata_analysis.png results\fast_analysis_dashboard.png results\filesystem_comparison_chart.png) do (
    if exist "%%f" (
        echo Opening %%f...
        start "" "%%f"
        timeout /t 2 /nobreak >nul
    )
)

echo.
echo ✅ ALL TESTS COMPLETE! ✅
echo =========================
echo Check the results\ folder for all generated files.
echo Performance graphs have been opened automatically.
echo.

:end