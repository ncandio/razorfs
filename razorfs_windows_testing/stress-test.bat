@echo off
REM RAZOR Filesystem Comprehensive Stress Test - Windows

echo === RAZOR Filesystem Comprehensive Stress Test ===
echo.

if "%1"=="clean" goto clean
if "%1"=="build" goto build
if "%1"=="stress" goto stress
if "%1"=="compression" goto compression
if "%1"=="comprehensive" goto comprehensive
if "%1"=="fast" goto fast
if "%1"=="metadata" goto metadata
if "%1"=="simple-metadata" goto simple_metadata
if "%1"=="metadata-comparison" goto metadata_comparison
if "%1"=="throughput-comparison" goto throughput_comparison
if "%1"=="real-filesystem" goto real_filesystem
if "%1"=="crash-recovery" goto crash_recovery
if "%1"=="combined" goto combined
if "%1"=="graphs" goto graphs
if "%1"=="simple-graphs" goto simple_graphs
if "%1"=="comp-graphs" goto comp_graphs
if "%1"=="fast-graphs" goto fast_graphs
if "%1"=="comp-full" goto comp_full
if "%1"=="full" goto full
if "%1"=="full-comp" goto full_comp
if "%1"=="full-real" goto full_real
if "%1"=="full-crash" goto full_crash
if "%1"=="full-combined" goto full_combined
if "%1"=="full-metadata" goto full_metadata
if "%1"=="full-metadata-comparison" goto full_metadata_comparison
if "%1"=="full-throughput-comparison" goto full_throughput_comparison
if "%1"=="diagnostics" goto diagnostics
if "%1"=="copy-results" goto copy_results
if "%1"=="" goto help

:help
echo Usage: stress-test.bat [command]
echo.
echo Commands:
echo   build                  Build Docker image with stress testing tools
echo   stress                 Run comprehensive stress tests
echo   compression            Run compression analysis vs ext4/btrfs
echo   comprehensive          Run 4-filesystem analysis (RAZOR/EXT4/ReiserFS/BTRFS)
echo   fast                   Run FAST 6-filesystem analysis (1MB/10MB/40MB) - QUICK!
echo   metadata               Run metadata and memory stress test
echo   simple-metadata        Run simple metadata test (works with current RAZOR)
echo   metadata-comparison    Run metadata comparison vs EXT4/BTRFS
echo   throughput-comparison  Run throughput comparison vs EXT4/BTRFS
echo   real-filesystem        Run real filesystem comparison test
echo   crash-recovery         Run crash and recovery test
echo   combined               Run complete test suite (performance + crash recovery)
echo   graphs                 Generate PNG graphs from results  
echo   simple-graphs          Generate simple graphs (no pandas conflicts)
echo   comp-graphs            Generate compression analysis graphs
echo   fast-graphs            Generate fast 6-filesystem graphs
echo   comp-full              Generate comprehensive 4-filesystem graphs
echo   full                   Run complete stress test and generate graphs
echo   full-comp              Run compression analysis and generate graphs
echo   full-real              Run real filesystem test and generate graphs
echo   full-crash             Run crash recovery test and generate graphs
echo   full-combined          Run complete test suite and generate all graphs
echo   full-metadata          Run metadata test and generate graphs
echo   full-metadata-comparison Run metadata comparison test and generate graphs
echo   full-throughput-comparison Run throughput comparison test and generate graphs
echo   diagnostics            Run RAZOR operations diagnostics
echo   copy-results           Copy results from container to Windows
echo   clean                  Clean Docker resources
echo.
echo Examples:
echo   stress-test.bat build
echo   stress-test.bat full
echo   stress-test.bat metadata
echo   stress-test.bat diagnostics
echo   stress-test.bat metadata-comparison
echo.
goto end

:build
echo Building RAZOR filesystem stress testing image...
docker-compose -f docker-compose-stress.yml build
if errorlevel 1 (
    echo Build failed!
    goto end
)
echo Build complete!
goto end

:stress
echo Running comprehensive stress tests...
echo This may take 10-20 minutes depending on your system.
echo.
docker-compose -f docker-compose-stress.yml run --rm razorfs-stress stress-test
goto end

:compression
echo Running compression analysis vs ext4 and btrfs...
echo This may take 15-25 minutes.
echo.
docker-compose -f docker-compose-stress.yml run --rm razorfs-stress compression-test
goto end

:comprehensive
echo Running comprehensive 4-filesystem analysis...
echo Testing RAZOR vs EXT4 vs ReiserFS vs BTRFS (25-35 minutes)
echo.
docker-compose -f docker-compose-stress.yml run --rm razorfs-stress full-comprehensive
goto end

:fast
echo Running FAST 6-filesystem analysis...
echo Testing RAZOR vs EXT4 vs BTRFS vs ZFS vs ReiserFS vs XFS
echo Files: 1MB, 10MB, 40MB (Quick test - under 5 minutes!)
echo.
docker-compose -f docker-compose-stress.yml run --rm razorfs-stress full-fast
goto end

:metadata
echo Running Metadata and Memory Stress Test...
echo This will create a large number of small files to test metadata performance.
echo.
docker-compose -f docker-compose-stress.yml run --rm razorfs-stress full-metadata
goto end

:simple_metadata
echo Running Simple Metadata Test...
echo This test works with the current RAZOR implementation.
echo.
docker-compose -f docker-compose-stress.yml run --rm razorfs-stress full-simple-metadata
goto end

:metadata_comparison
echo Running Metadata Performance Comparison Test...
echo Testing RAZOR vs EXT4 vs BTRFS metadata operations.
echo This will take 15-20 minutes.
echo.
docker-compose -f docker-compose-stress.yml run --rm razorfs-stress full-metadata-comparison
goto end

:throughput_comparison
echo Running Throughput Performance Comparison Test...
echo Testing RAZOR vs EXT4 vs BTRFS write/read throughput.
echo This will take 20-25 minutes.
echo.
docker-compose -f docker-compose-stress.yml run --rm razorfs-stress full-throughput-comparison
goto end

:real_filesystem
echo Running Real Filesystem Comparison Test...
echo Testing RAZOR vs EXT4 vs BTRFS vs ReiserFS vs XFS vs SquashFS
echo This will take 20-30 minutes.
echo.
docker-compose -f docker-compose-stress.yml run --rm razorfs-stress real-filesystem-test
goto end

:crash_recovery
echo Running Crash and Recovery Test...
echo Testing filesystem resilience and recovery capabilities.
echo This will take 15-20 minutes.
echo.
docker-compose -f docker-compose-stress.yml run --rm razorfs-stress crash-recovery-test
goto end

:combined
echo Running Combined Filesystem Test Suite...
echo Complete test suite with performance and recovery testing.
echo This will take 30-45 minutes.
echo.
docker-compose -f docker-compose-stress.yml run --rm razorfs-stress combined-test
goto end

:comp_graphs
echo Generating compression analysis graphs...
docker-compose -f docker-compose-stress.yml run --rm razorfs-stress compression-graphs
goto end

:fast_graphs
echo Generating FAST 6-filesystem analysis graphs...
docker-compose -f docker-compose-stress.yml run --rm razorfs-stress fast-graphs
goto end

:comp_full
echo Generating comprehensive 4-filesystem analysis graphs...
docker-compose -f docker-compose-stress.yml run --rm razorfs-stress comprehensive-graphs
goto end

:full_real
echo Running real filesystem comparison with graph generation...
echo Testing RAZOR vs EXT4 vs BTRFS vs ReiserFS vs XFS vs SquashFS
echo This will take 25-35 minutes.
echo.
set /p confirm="Continue with real filesystem comparison? (y/N): "
if /i not "%confirm%"=="y" (
    echo Real filesystem comparison cancelled.
    goto end
)

echo Starting real filesystem comparison...
docker-compose -f docker-compose-stress.yml run --rm razorfs-stress full-real-filesystem

echo.
echo Copying real filesystem results to Windows...
docker cp razorfs-stress-container:/results ./ 2>nul || (
    echo Creating temporary container to copy results...
    docker-compose -f docker-compose-stress.yml run -d --name temp-results razorfs-stress shell
    timeout /t 3 /nobreak >nul
    docker cp temp-results:/results ./
    docker rm -f temp-results
)

echo.
echo === Real Filesystem Comparison Complete! ===
echo Results copied to: %CD%\results\
echo Real filesystem PNG graphs generated:
dir results\*real*.png 2>nul || echo No real filesystem PNG files found
goto end

:full_crash
echo Running crash recovery test with graph generation...
echo Testing filesystem resilience and recovery capabilities.
echo This will take 15-25 minutes.
echo.
set /p confirm="Continue with crash recovery test? (y/N): "
if /i not "%confirm%"=="y" (
    echo Crash recovery test cancelled.
    goto end
)

echo Starting crash recovery test...
docker-compose -f docker-compose-stress.yml run --rm razorfs-stress full-crash-recovery

echo.
echo Copying crash recovery results to Windows...
docker cp razorfs-stress-container:/results ./ 2>nul || (
    echo Creating temporary container to copy results...
    docker-compose -f docker-compose-stress.yml run -d --name temp-results razorfs-stress shell
    timeout /t 3 /nobreak >nul
    docker cp temp-results:/results ./
    docker rm -f temp-results
)

echo.
echo === Crash Recovery Test Complete! ===
echo Results copied to: %CD%\results\
echo Crash recovery PNG graphs generated:
dir results\*crash*.png 2>nul || echo No crash recovery PNG files found
goto end

:full_combined
echo Running complete filesystem test suite with graphs...
echo Complete test suite with performance and recovery testing.
echo This will take 45-60 minutes.
echo.
set /p confirm="Continue with complete test suite? (y/N): "
if /i not "%confirm%"=="y" (
    echo Complete test suite cancelled.
    goto end
)

echo Starting complete test suite...
docker-compose -f docker-compose-stress.yml run --rm razorfs-stress full-combined

echo.
echo Copying complete test results to Windows...
docker cp razorfs-stress-container:/results ./ 2>nul || (
    echo Creating temporary container to copy results...
    docker-compose -f docker-compose-stress.yml run -d --name temp-results razorfs-stress shell
    timeout /t 3 /nobreak >nul
    docker cp temp-results:/results ./
    docker rm -f temp-results
)

echo.
echo === Complete Test Suite Complete! ===
echo Results copied to: %CD%\results\
echo PNG graphs generated:
dir results\*.png 2>nul || echo No PNG files found
goto end

:full_comp
echo Running full compression analysis with graph generation...
echo This will test RAZOR vs ext4 vs btrfs compression (20-30 minutes).
echo.
set /p confirm="Continue with full compression analysis? (y/N): "
if /i not "%confirm%"=="y" (
    echo Compression analysis cancelled.
    goto end
)

echo Starting comprehensive compression analysis...
docker-compose -f docker-compose-stress.yml run --rm razorfs-stress full-compression

echo.
echo Copying compression results to Windows...
docker cp razorfs-stress-container:/results ./ 2>nul || (
    echo Creating temporary container to copy results...
    docker-compose -f docker-compose-stress.yml run -d --name temp-results razorfs-stress shell
    timeout /t 3 /nobreak >nul
    docker cp temp-results:/results ./
    docker rm -f temp-results
)

echo.
echo === Compression Analysis Complete! ===
echo Results copied to: %CD%\results\
echo Compression PNG graphs generated:
dir results\*compression*.png 2>nul || echo No compression PNG files found
goto end

:full_metadata
echo Running full metadata test with graph generation...
echo This will test metadata performance and generate graphs.
echo.
docker-compose -f docker-compose-stress.yml run --rm razorfs-stress full-metadata

echo.
echo Copying metadata results to Windows...
docker run --rm -v razorfs-stress-results:/results -v %CD%\results:/windows-results busybox sh -c "cp /results/*metadata*.csv /windows-results/ 2>/dev/null; cp /results/*metadata*.png /windows-results/ 2>/dev/null; echo 'Metadata files copied successfully'"

echo.
echo === Metadata Test Complete! ===
echo Results copied to: %CD%\results\
echo Metadata PNG graphs generated:
dir results\*metadata*.png 2>nul || echo No metadata PNG files found
goto end

:full_metadata_comparison
echo Running metadata comparison test with graph generation...
echo Testing RAZOR vs EXT4 vs BTRFS metadata performance.
echo This will take 15-20 minutes.
echo.
set /p confirm="Continue with metadata comparison test? (y/N): "
if /i not "%confirm%"=="y" (
    echo Metadata comparison test cancelled.
    goto end
)

echo Starting metadata comparison test...
docker-compose -f docker-compose-stress.yml run --rm razorfs-stress full-metadata-comparison

echo.
echo Copying metadata comparison results to Windows...
docker run --rm -v razorfs-stress-results:/results -v %CD%\results:/windows-results busybox sh -c "cp /results/*metadata*.csv /windows-results/ 2>/dev/null; cp /results/*metadata*.png /windows-results/ 2>/dev/null; echo 'Metadata comparison files copied successfully'"

echo.
echo === Metadata Comparison Test Complete! ===
echo Results copied to: %CD%\results\
echo Metadata comparison PNG graphs generated:
dir results\*metadata*.png 2>nul || echo No metadata PNG files found
goto end

:full_throughput_comparison
echo Running throughput comparison test with graph generation...
echo Testing RAZOR vs EXT4 vs BTRFS write/read throughput.
echo This will take 20-25 minutes.
echo.
set /p confirm="Continue with throughput comparison test? (y/N): "
if /i not "%confirm%"=="y" (
    echo Throughput comparison test cancelled.
    goto end
)

echo Starting throughput comparison test...
docker-compose -f docker-compose-stress.yml run --rm razorfs-stress full-throughput-comparison

echo.
echo Copying throughput comparison results to Windows...
docker run --rm -v razorfs-stress-results:/results -v %CD%\results:/windows-results busybox sh -c "cp /results/*throughput*.csv /windows-results/ 2>/dev/null; cp /results/*throughput*.png /windows-results/ 2>/dev/null; echo 'Throughput comparison files copied successfully'"

echo.
echo === Throughput Comparison Test Complete! ===
echo Results copied to: %CD%\results\
echo Throughput comparison PNG graphs generated:
dir results\*throughput*.png 2>nul || echo No throughput PNG files found
goto end

:graphs
echo Generating performance graphs...
docker-compose -f docker-compose-stress.yml run --rm razorfs-stress generate-graphs
goto end

:simple_graphs
echo Generating simple graphs without pandas conflicts...
docker-compose -f docker-compose-stress.yml run --rm razorfs-stress simple-graphs
goto end

:full
echo Running full stress test with graph generation...
echo This will take 15-30 minutes.
echo.
set /p confirm="Continue with full stress test? (y/N): "
if /i not "%confirm%"=="y" (
    echo Stress test cancelled.
    goto end
)

echo Starting comprehensive stress test...
docker-compose -f docker-compose-stress.yml run --rm razorfs-stress full-test

echo.
echo Copying results to Windows...
docker cp razorfs-stress-results:/results ./ 2>nul || (
    echo Creating temporary container to copy results...
    docker-compose -f docker-compose-stress.yml run -d --name temp-results razorfs-stress shell
    timeout /t 3 /nobreak >nul
    docker cp temp-results:/results ./
    docker rm -f temp-results
)

echo.
echo === Stress Test Complete! ===
echo Results copied to: %CD%\results\
echo PNG graphs generated:
dir results\*.png 2>nul || echo No PNG files found
goto end

:diagnostics
echo Running RAZOR operations diagnostics...
echo This will test which operations work in the current RAZOR implementation.
echo.
docker-compose -f docker-compose-stress.yml run --rm razorfs-stress diagnostics

echo.
echo Copying diagnostics results to Windows...
docker run --rm -v razorfs-stress-results:/results -v %CD%\results:/windows-results busybox sh -c "cp /results/razor_operations_diagnostics.csv /windows-results/ 2>/dev/null; cp /results/diagnostics_system_info.txt /windows-results/ 2>/dev/null; echo 'Files copied successfully'"

echo.
echo === Diagnostics Complete! ===
echo Results copied to: %CD%\results\
echo Diagnostic files generated:
dir results\*diagnostics* 2>nul || echo No diagnostics files found
goto end

:copy_results
echo Copying results from Docker container to Windows...

REM Try to copy from running container first
docker cp razorfs-stress:/results ./ 2>nul || (
    echo No running container found. Starting temporary container...
    docker-compose -f docker-compose-stress.yml run -d --name temp-results razorfs-stress shell
    timeout /t 3 /nobreak >nul
    docker cp temp-results:/results ./
    docker rm -f temp-results
)

echo.
echo Results available in: %CD%\results\
echo.
echo Generated files:
dir results\ 2>nul || echo No results directory found

echo.
echo PNG Performance Graphs:
dir results\*.png 2>nul || echo No PNG files found

echo.
echo CSV Data Files:  
dir results\*.csv 2>nul || echo No CSV files found

goto end

:clean
echo Cleaning Docker resources...
docker-compose -f docker-compose-stress.yml down --rmi all --volumes
docker system prune -f
echo Cleanup complete!
goto end

:end