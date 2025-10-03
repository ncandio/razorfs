@echo off
echo ========================================
echo RAZORFS O(log n) Complexity Test Suite
echo ========================================

REM Set variables
set RESULTS_VOLUME=razorfs-stress-results
set MODE=%1
if "%MODE%"=="" set MODE=quick

echo Running mode: %MODE%

if "%MODE%"=="analyze" (
    echo Analyzing existing results and generating graphs...
    docker run --rm -v %RESULTS_VOLUME%:/results razorfs_windows_testing-razorfs-stress python3 /razorfs/razorfs_windows_testing/generate_ologn_graphs.py
    goto end
)

if "%MODE%"=="quick" (
    echo Running quick O(log n) test...
    docker run --rm -v %RESULTS_VOLUME%:/results --cap-add SYS_ADMIN --device /dev/fuse --security-opt apparmor:unconfined razorfs_windows_testing-razorfs-stress bash -c "cd /razorfs && chmod +x /razorfs/razorfs_windows_testing/ologn_complexity_test.sh && /razorfs/razorfs_windows_testing/ologn_complexity_test.sh quick"
    goto end
)

echo Unknown mode: %MODE%
echo.
echo Available modes:
echo   quick   - Quick test with smaller scales
echo   analyze - Generate graphs from existing results
echo.
echo Example: %0 quick
exit /b 1

:end
echo Test completed.