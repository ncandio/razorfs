@echo off
REM Filesystem Comparison Test Runner for Windows
REM Inspired by RAZOR testing framework workflow

setlocal enabledelayedexpansion

set "COMMAND=%~1"
set "DOCKER_COMPOSE_FILE=docker-compose-filesystem-comparison.yml"

if "%COMMAND%"=="" (
    echo Usage: %0 ^<command^>
    echo.
    echo Available commands:
    echo   diagnostics     - Check filesystem availability
    echo   quick          - Run quick comparison test ^(5 min^)
    echo   full           - Run comprehensive comparison ^(30 min^)
    echo   micro          - Run micro-benchmarks
    echo   stress         - Run stress tests
    echo   clean          - Clean up Docker resources
    echo   setup          - Initial setup and build
    echo   copy-results   - Copy results from container to Windows
    echo.
    goto :eof
)

echo === RazorFS vs Traditional Filesystems ===
echo Command: %COMMAND%
echo Time: %DATE% %TIME%
echo.

if "%COMMAND%"=="diagnostics" (
    echo Running filesystem diagnostics...
    docker-compose -f %DOCKER_COMPOSE_FILE% run --rm diagnostics
    goto :eof
)

if "%COMMAND%"=="setup" (
    echo Setting up filesystem comparison environment...
    echo Building Docker image...
    docker-compose -f %DOCKER_COMPOSE_FILE% build
    echo.
    echo Setup complete. Run 'filesystem-test.bat diagnostics' to verify.
    goto :eof
)

if "%COMMAND%"=="quick" (
    echo Running quick filesystem comparison...
    echo This will test basic operations on all filesystems ^(~5 minutes^)
    docker-compose -f %DOCKER_COMPOSE_FILE% run --rm quick-test
    call :copy_results
    goto :eof
)

if "%COMMAND%"=="full" (
    echo Running comprehensive filesystem comparison...
    echo This will run extensive benchmarks ^(~30 minutes^)
    docker-compose -f %DOCKER_COMPOSE_FILE% run --rm filesystem-benchmark
    call :copy_results
    goto :eof
)

if "%COMMAND%"=="micro" (
    echo Running micro-benchmarks...
    docker-compose -f %DOCKER_COMPOSE_FILE% run --rm filesystem-benchmark ./run-filesystem-comparison.sh micro
    call :copy_results
    goto :eof
)

if "%COMMAND%"=="stress" (
    echo Running stress tests...
    echo Warning: This will push all filesystems to their limits
    docker-compose -f %DOCKER_COMPOSE_FILE% run --rm filesystem-benchmark ./run-filesystem-comparison.sh stress
    call :copy_results
    goto :eof
)

if "%COMMAND%"=="clean" (
    echo Cleaning up Docker resources...
    docker-compose -f %DOCKER_COMPOSE_FILE% down --remove-orphans
    docker system prune -f
    echo Cleanup complete.
    goto :eof
)

if "%COMMAND%"=="copy-results" (
    call :copy_results
    goto :eof
)

echo Unknown command: %COMMAND%
echo Run 'filesystem-test.bat' without arguments to see available commands.
goto :eof

:copy_results
echo.
echo === Copying Results to Windows ===

REM Create results directory if it doesn't exist
if not exist "results" mkdir results

REM Get the container ID of the latest run
for /f %%i in ('docker ps -a --filter "name=razorfs-" --format "{{.ID}}" --latest') do set CONTAINER_ID=%%i

if not "%CONTAINER_ID%"=="" (
    echo Copying results from container %CONTAINER_ID%...
    docker cp %CONTAINER_ID%:/app/results/. ./results/
    echo Results copied to ./results/
) else (
    echo No container found. Results may already be available in ./results/
)

REM List available results
echo.
echo Available results:
dir /B results
echo.
echo Results location: %CD%\results
goto :eof