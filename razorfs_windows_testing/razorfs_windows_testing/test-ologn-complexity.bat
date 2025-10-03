@echo off
setlocal enabledelayedexpansion

REM RAZORFS O(log n) Complexity Test Suite for Windows
REM This batch file runs comprehensive O(log n) complexity tests in Docker

echo ========================================
echo RAZORFS O(log n) Complexity Test Suite
echo ========================================
echo.

REM Check if Docker is running
docker version >nul 2>&1
if errorlevel 1 (
    echo ERROR: Docker is not running. Please start Docker Desktop.
    exit /b 1
)

REM Set variables
set COMPOSE_FILE=docker-compose-stress.yml
set CONTAINER_NAME=razorfs-ologn-test
set RESULTS_VOLUME=razorfs-stress-results

REM Parse command line arguments
set MODE=%1
if "%MODE%"=="" set MODE=full

echo Running mode: %MODE%
echo.

REM Build Docker image
echo Building Docker image...
docker-compose -f %COMPOSE_FILE% build
if errorlevel 1 (
    echo ERROR: Failed to build Docker image
    exit /b 1
)

REM Run different test modes
if "%MODE%"=="quick" (
    echo Running quick O(log n) test (scales up to 10,000 files)...
    docker run --rm ^
        -v %RESULTS_VOLUME%:/results ^
        --cap-add SYS_ADMIN ^
        --device /dev/fuse ^
        --security-opt apparmor:unconfined ^
        razorfs_windows_testing-razorfs-stress ^
        /bin/bash -c "cd /razorfs/razorfs_windows_testing && ./ologn_complexity_test.sh quick"

) else if "%MODE%"=="full" (
    echo Running full O(log n) test (scales up to 100,000 files)...
    docker run --rm ^
        -v %RESULTS_VOLUME%:/results ^
        --cap-add SYS_ADMIN ^
        --device /dev/fuse ^
        --security-opt apparmor:unconfined ^
        razorfs_windows_testing-razorfs-stress ^
        /bin/bash -c "cd /razorfs/razorfs_windows_testing && chmod +x ologn_complexity_test.sh && ./ologn_complexity_test.sh"

) else if "%MODE%"=="analyze" (
    echo Analyzing existing results and generating graphs...
    docker run --rm ^
        -v %RESULTS_VOLUME%:/results ^
        razorfs_windows_testing-razorfs-stress ^
        /bin/bash -c "cd /razorfs/razorfs_windows_testing && python3 generate_ologn_graphs.py"

) else if "%MODE%"=="full-with-graphs" (
    echo Running full test with automatic graph generation...

    REM Run the test
    docker run --rm ^
        -v %RESULTS_VOLUME%:/results ^
        --cap-add SYS_ADMIN ^
        --device /dev/fuse ^
        --security-opt apparmor:unconfined ^
        razorfs_windows_testing-razorfs-stress ^
        /bin/bash -c "cd /razorfs/razorfs_windows_testing && chmod +x ologn_complexity_test.sh && ./ologn_complexity_test.sh"

    if errorlevel 1 (
        echo WARNING: Test encountered errors, but continuing with graph generation...
    )

    REM Generate graphs
    echo.
    echo Generating analysis graphs...
    docker run --rm ^
        -v %RESULTS_VOLUME%:/results ^
        razorfs_windows_testing-razorfs-stress ^
        /bin/bash -c "cd /razorfs/razorfs_windows_testing && python3 generate_ologn_graphs.py"

) else if "%MODE%"=="benchmark" (
    echo Running comparative benchmark against theoretical complexities...
    docker run --rm ^
        -v %RESULTS_VOLUME%:/results ^
        --cap-add SYS_ADMIN ^
        --device /dev/fuse ^
        --security-opt apparmor:unconfined ^
        razorfs_windows_testing-razorfs-stress ^
        /bin/bash -c "cd /razorfs/razorfs_windows_testing && chmod +x ologn_complexity_test.sh && ./ologn_complexity_test.sh benchmark"

) else (
    echo Unknown mode: %MODE%
    echo.
    echo Available modes:
    echo   quick           - Quick test with smaller scales (up to 10K files)
    echo   full            - Full test with all scales (up to 100K files)
    echo   analyze         - Generate graphs from existing results
    echo   full-with-graphs - Run full test and generate graphs
    echo   benchmark       - Run comparative benchmark
    echo.
    echo Example: %0 full-with-graphs
    exit /b 1
)

if errorlevel 1 (
    echo.
    echo Test completed with warnings. Check results for details.
) else (
    echo.
    echo Test completed successfully!
)

REM Copy results to Windows
echo.
echo Copying results to Windows...
docker run --rm ^
    -v %RESULTS_VOLUME%:/source ^
    -v "%CD%\results":/destination ^
    busybox sh -c "cp -r /source/* /destination/ 2>/dev/null || true"

echo Results copied to: %CD%\results\

REM List generated files
echo.
echo Generated files:
dir /b "%CD%\results\ologn_*.csv" 2>nul
dir /b "%CD%\results\ologn_*.log" 2>nul
dir /b "%CD%\results\ologn_*.png" 2>nul

echo.
echo ========================================
echo O(log n) Complexity Test Complete!
echo ========================================

REM Display quick summary if CSV exists
for /f "delims=" %%i in ('dir /b /od "%CD%\results\ologn_complexity_*.csv" 2^>nul') do set LATEST_CSV=%%i
if not "%LATEST_CSV%"=="" (
    echo.
    echo Quick Summary from %LATEST_CSV%:
    type "%CD%\results\%LATEST_CSV%" | findstr /n "^" | findstr "^[1-6]:"
    echo.
    echo Run with 'analyze' mode to generate detailed graphs.
)

endlocal