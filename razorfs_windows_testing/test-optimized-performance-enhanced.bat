@echo off
REM Optimized RazorFS Performance Testing Script for Windows
REM Tests O(log n) improvements vs EXT4 vs BTRFS

echo ===============================================
echo RazorFS O(log n) Performance Testing
echo ===============================================
echo.

echo Select testing option:
echo 1. Standard optimized build (default)
echo 2. Debug build (for troubleshooting Docker issues)
echo 3. Simple build (minimal dependencies)
echo 4. Corrected build v2 (fixed Makefile paths)
echo 5. Direct compilation build (no Makefile)
echo 6. Corrected build v3 (improved debugging)
echo 7. Corrected build v4 (explicit paths)
echo 8. Corrected build v5 (ultimate debugging)
echo 9. Corrected build v6 (explicit copy)
echo 10. Corrected build v7 (context debug)
echo 11. Corrected build v8 (final debug)
echo 12. Corrected final build (directory check)
echo 13. Thorough debug build (adaptive path detection)
echo.

choice /c 12345678910111213 /m "Select option"
if errorlevel 13 goto corrected_thorough
if errorlevel 12 goto corrected_dircheck
if errorlevel 11 goto corrected_v8
if errorlevel 10 goto corrected_v7
if errorlevel 9 goto corrected_v6
if errorlevel 8 goto corrected_v5
if errorlevel 7 goto corrected_v4
if errorlevel 6 goto corrected_v3
if errorlevel 5 goto direct
if errorlevel 4 goto corrected_v2
if errorlevel 3 goto simple
if errorlevel 2 goto debug
goto standard

:standard
echo Using standard optimized build...
set DOCKERFILE=Dockerfile.optimized
set BUILD_NAME=razorfs-optimized
set COMPOSE_FILE=docker-compose-optimized.yml
goto build

:debug
echo Using debug build...
set DOCKERFILE=Dockerfile.optimized.debug
set BUILD_NAME=razorfs-optimized-debug
set COMPOSE_FILE=docker-compose-optimized.yml
goto build

:simple
echo Using simple build...
set DOCKERFILE=Dockerfile.optimized.simple
set BUILD_NAME=razorfs-optimized-simple
set COMPOSE_FILE=docker-compose-optimized.yml
goto build

:corrected_v2
echo Using corrected v2 build...
set DOCKERFILE=Dockerfile.optimized.corrected.v2
set BUILD_NAME=razorfs-optimized-corrected-v2
set COMPOSE_FILE=docker-compose-optimized-corrected-v2.yml
goto build

:direct
echo Using direct compilation build...
set DOCKERFILE=Dockerfile.optimized.direct
set BUILD_NAME=razorfs-optimized-direct
set COMPOSE_FILE=docker-compose-optimized-direct.yml
goto build

:corrected_v3
echo Using corrected v3 build...
set DOCKERFILE=Dockerfile.optimized.corrected.v3
set BUILD_NAME=razorfs-optimized-corrected-v3
set COMPOSE_FILE=docker-compose-optimized-corrected-v3.yml
goto build

:corrected_v4
echo Using corrected v4 build...
set DOCKERFILE=Dockerfile.optimized.corrected.v4
set BUILD_NAME=razorfs-optimized-corrected-v4
set COMPOSE_FILE=docker-compose-optimized-corrected-v4.yml
goto build

:corrected_v5
echo Using corrected v5 build...
set DOCKERFILE=Dockerfile.optimized.corrected.v5
set BUILD_NAME=razorfs-optimized-corrected-v5
set COMPOSE_FILE=docker-compose-optimized-corrected-v5.yml
goto build

:corrected_v6
echo Using corrected v6 build...
set DOCKERFILE=Dockerfile.optimized.corrected.v6
set BUILD_NAME=razorfs-optimized-corrected-v6
set COMPOSE_FILE=docker-compose-optimized-corrected-v6.yml
goto build

:corrected_v7
echo Using corrected v7 build...
set DOCKERFILE=Dockerfile.optimized.corrected.v7
set BUILD_NAME=razorfs-optimized-corrected-v7
set COMPOSE_FILE=docker-compose-optimized-corrected-v7.yml
goto build

:corrected_v8
echo Using corrected v8 build...
set DOCKERFILE=Dockerfile.optimized.corrected.v8
set BUILD_NAME=razorfs-optimized-corrected-v8
set COMPOSE_FILE=docker-compose-optimized-corrected-v8.yml
goto build

:corrected_dircheck
echo Using corrected directory check build...
set DOCKERFILE=Dockerfile.optimized.corrected.dircheck
set BUILD_NAME=razorfs-optimized-corrected-dircheck
set COMPOSE_FILE=docker-compose-optimized-corrected-dircheck.yml
goto build

:corrected_thorough
echo Using thorough debug build...
set DOCKERFILE=Dockerfile.optimized.corrected.thorough
set BUILD_NAME=razorfs-optimized-corrected-thorough
set COMPOSE_FILE=docker-compose-optimized-corrected-thorough.yml
goto build

:build
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

echo Building %BUILD_NAME% test environment...
if "%COMPOSE_FILE%"=="" (
    docker build --no-cache -f %DOCKERFILE% -t %BUILD_NAME% .
) else (
    docker-compose -f %COMPOSE_FILE% build --no-cache
)

if %errorlevel% neq 0 (
    echo ERROR: Failed to build Docker image
    echo Check the debug output above to identify the issue
    if "%DOCKERFILE%"=="Dockerfile.optimized.debug" (
        echo The debug output should show exactly which files are present during the build
    )
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
if "%COMPOSE_FILE%"=="" (
    docker run --rm -v "%cd%\results:/results" %BUILD_NAME% /bin/bash -c "cd /razor_repo/razorfs_windows_testing && ./optimized_filesystem_comparison.sh"
) else (
    docker-compose -f %COMPOSE_FILE% up --force-recreate
)

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

echo 🚀 Test complete! Review the results to validate O(log n) optimization.
echo.

REM Cleanup containers
if not "%COMPOSE_FILE%"=="" (
    echo Cleaning up Docker containers...
    docker-compose -f %COMPOSE_FILE% down
)

pause