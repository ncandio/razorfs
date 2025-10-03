@echo off
REM Optimized RazorFS Performance Testing Script for Windows - Corrected Version
REM Tests O(log n) improvements vs EXT4 vs BTRFS

echo ===============================================
echo RazorFS O(log n) Performance Testing - Corrected Version
echo ===============================================
echo.

REM Check if Docker is running
docker info >nul 2>&1
if %errorlevel% neq 0 (
    echo ERROR: Docker is not running or not accessible
    echo Please start Docker Desktop and try again
    pause
    exit /b 1
)

echo Docker is running...
echo.

REM Create results directory
if not exist "results" mkdir results

echo Building corrected optimized test environment...
docker-compose -f docker-compose-optimized-corrected.yml build --no-cache

if %errorlevel% neq 0 (
    echo ERROR: Failed to build Docker image
    echo Check the debug output above to identify the issue
    pause
    exit /b 1
)

echo.
echo Starting performance comparison tests...
echo This will test RazorFS vs EXT4 vs BTRFS with different directory sizes
echo.
echo Tests include:
echo - File creation performance
echo - Lookup performance (O(log n) validation)
echo - Directory listing speed
echo - File deletion performance
echo.
echo This may take 10-15 minutes...
echo.

REM Run the tests
docker-compose -f docker-compose-optimized-corrected.yml up --force-recreate

if %errorlevel% neq 0 (
    echo ERROR: Tests failed
    echo Check the Docker logs for details
    pause
    exit /b 1
)

echo.
echo ===============================================
echo Performance Tests Complete!
echo ===============================================
echo.

REM Check if results were generated
if exist "results\optimized_filesystem_comparison.png" (
    echo ✅ Performance charts generated successfully
    echo 📊 Chart: results\optimized_filesystem_comparison.png
) else (
    echo ⚠️  Performance charts not found
)

if exist "results\OPTIMIZED_PERFORMANCE_REPORT.md" (
    echo ✅ Performance report generated successfully
    echo 📄 Report: results\OPTIMIZED_PERFORMANCE_REPORT.md
) else (
    echo ⚠️  Performance report not found
)

if exist "results\optimized_comparison_results.csv" (
    echo ✅ Raw performance data available
    echo 📋 Data: results\optimized_comparison_results.csv
) else (
    echo ⚠️  Performance data not found
)

echo.
echo Results are available in the 'results' directory:
dir results
echo.

echo Key validation points:
echo 1. Check if RazorFS shows constant lookup times (O(1))
echo 2. Compare against EXT4/BTRFS linear growth (O(N))
echo 3. Look for 10x-1000x improvements in large directories
echo.

REM Cleanup containers
echo Cleaning up Docker containers...
docker-compose -f docker-compose-optimized-corrected.yml down

echo.
echo 🚀 Test complete! Review the results to validate O(log n) optimization.
echo.
pause