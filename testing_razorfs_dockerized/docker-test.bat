@echo off
REM RAZOR Filesystem Docker Testing Script for Windows
REM Batch file for Windows Docker Desktop

setlocal enabledelayedexpansion

if "%1"=="" goto help
if "%1"=="help" goto help
if "%1"=="--help" goto help
if "%1"=="-h" goto help

REM Check if Docker is available
docker --version >nul 2>&1
if errorlevel 1 (
    echo [ERROR] Docker is not installed or not in PATH
    exit /b 1
)

docker-compose --version >nul 2>&1
if errorlevel 1 (
    echo [ERROR] docker-compose is not installed or not in PATH
    exit /b 1
)

if "%1"=="build" goto build
if "%1"=="test-fuse" goto test_fuse
if "%1"=="test-kernel" goto test_kernel
if "%1"=="test-all" goto test_all
if "%1"=="dev" goto dev
if "%1"=="clean" goto clean
if "%1"=="logs" goto logs
if "%1"=="shell" goto shell

echo [ERROR] Unknown command: %1
echo.
goto help

:help
echo RAZOR Filesystem Docker Testing Script for Windows
echo.
echo Usage: docker-test.bat [COMMAND]
echo.
echo Commands:
echo   build          Build Docker images
echo   test-fuse      Test FUSE implementation (safe)
echo   test-kernel    Test kernel module (requires Hyper-V)
echo   test-all       Run all tests
echo   dev            Start development container
echo   clean          Clean Docker resources
echo   logs           Show container logs
echo   shell          Start interactive shell in container
echo   help           Show this help
echo.
echo Examples:
echo   docker-test.bat build
echo   docker-test.bat test-fuse
echo   docker-test.bat dev
echo.
echo Requirements:
echo   - Docker Desktop for Windows
echo   - WSL2 backend (recommended)
goto end

:build
echo === Building RAZOR Filesystem Docker Images ===
echo.
docker-compose build
if errorlevel 1 (
    echo [ERROR] Failed to build Docker images
    exit /b 1
)
echo [SUCCESS] Docker images built successfully
goto end

:test_fuse
echo === Testing FUSE Implementation ===
echo.
echo [INFO] Testing FUSE implementation (this is safe)

REM Create test directory
set TESTDIR=%TEMP%\razorfs-docker-test
if not exist "%TESTDIR%" mkdir "%TESTDIR%"

docker-compose run --rm razorfs-fuse
if errorlevel 1 (
    echo [ERROR] FUSE test failed
    rmdir /s /q "%TESTDIR%" 2>nul
    exit /b 1
)

echo [SUCCESS] FUSE test completed
rmdir /s /q "%TESTDIR%" 2>nul
goto end

:test_kernel
echo === Testing Kernel Module ===
echo.
echo [WARNING] This requires privileged Docker containers!
echo [WARNING] On Windows, kernel modules run in WSL2 VM, not Windows kernel.
echo.
set /p response="Are you sure you want to continue? (y/N): "
if /i not "%response%"=="y" (
    echo [WARNING] Kernel module test cancelled
    goto end
)

docker-compose run --rm razorfs-kernel
if errorlevel 1 (
    echo [ERROR] Kernel module test failed
    exit /b 1
)
echo [SUCCESS] Kernel module test completed
goto end

:test_all
echo === Running All Tests ===
echo.

call :test_fuse
if errorlevel 1 set FAILED=1

echo.
set /p response="Run kernel module test? This requires privileged mode (y/N): "
if /i "%response%"=="y" (
    call :test_kernel
    if errorlevel 1 set FAILED=1
) else (
    echo [WARNING] Skipping kernel module test
)

if defined FAILED (
    echo [ERROR] Some tests failed
    exit /b 1
) else (
    echo [SUCCESS] All tests passed!
)
goto end

:dev
echo === Starting Development Container ===
echo.
echo [INFO] Starting interactive development environment
docker-compose run --rm razorfs-dev
goto end

:clean
echo === Cleaning Docker Resources ===
echo.
echo Stopping containers...
docker-compose down --remove-orphans

echo Removing images...
docker-compose down --rmi all --volumes --remove-orphans

echo [SUCCESS] Cleanup completed
goto end

:logs
echo === Container Logs ===
docker-compose logs
goto end

:shell
echo === Starting Interactive Shell ===
docker-compose run --rm razorfs-dev shell
goto end

:end
endlocal