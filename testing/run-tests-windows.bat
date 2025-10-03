@echo off
REM RAZORFS Windows Test Runner
REM Run from: C:\Users\liber\Desktop\Testing-Razor-FS

echo =========================================
echo   RAZORFS Testing - Windows Docker
echo =========================================
echo.

REM Set paths
set RESULTS_DIR=%cd%\results
set CHARTS_DIR=%cd%\charts
set DATA_DIR=%cd%\data

REM Create output directories
mkdir %RESULTS_DIR% 2>nul
mkdir %CHARTS_DIR% 2>nul
mkdir %DATA_DIR% 2>nul

echo Building Docker image...
docker build -t razorfs-test -f Dockerfile .

echo.
echo Running benchmarks...
docker run --rm --privileged ^
    -v %cd%:/testing ^
    -v %RESULTS_DIR%:/results ^
    -v %CHARTS_DIR%:/charts ^
    -v %DATA_DIR%:/data ^
    razorfs-test bash /testing/benchmark-windows.sh

echo.
echo Generating graphs...
docker run --rm ^
    -v %cd%:/testing ^
    -v %DATA_DIR%:/data ^
    -v %CHARTS_DIR%:/charts ^
    razorfs-test gnuplot /testing/visualize-windows.gnuplot

echo.
echo ========================================
echo   Tests Complete!
echo ========================================
echo   Results:  %RESULTS_DIR%
echo   Graphs:   %CHARTS_DIR%
echo   Data:     %DATA_DIR%
echo ========================================
echo.

pause
