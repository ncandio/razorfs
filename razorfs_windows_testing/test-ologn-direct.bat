@echo off
echo ========================================
echo RAZORFS O(log n) Complexity Test Suite
echo ========================================

REM Set variables
set RESULTS_VOLUME=razorfs-stress-results

REM Parse command line arguments
set MODE=%1
if "%MODE%"=="" set MODE=analyze

echo Running mode: %MODE%

REM Run different test modes
if "%MODE%"=="analyze" (
    echo Analyzing existing results and generating O(log n) graphs...
    docker run --rm -v %RESULTS_VOLUME%:/results razorfs_windows_testing-razorfs-stress /bin/bash -c "cd /razorfs/razorfs_windows_testing && python3 generate_ologn_graphs.py"
    goto :end
)

if "%MODE%"=="quick" (
    echo Running quick O(log n) test (scales up to 10,000 files)...
    docker run --rm -v %RESULTS_VOLUME%:/results --cap-add SYS_ADMIN --device /dev/fuse --security-opt apparmor:unconfined razorfs_windows_testing-razorfs-stress /bin/bash -c "cd /razorfs/razorfs_windows_testing && ./ologn_complexity_test.sh quick"
    goto :end
)

echo Unknown mode: %MODE%
echo.
echo Available modes:
echo   quick   - Quick test with smaller scales (up to 10K files)
echo   analyze - Generate graphs from existing results
echo.
echo Example: %0 quick
exit /b 1

:end
echo.
echo Test completed.