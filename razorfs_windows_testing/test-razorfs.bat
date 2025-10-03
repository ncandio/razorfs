@echo off
REM RAZOR Filesystem Test - Windows (Simple Version)

echo === RAZOR Filesystem Test for Windows ===
echo.

if "%1"=="clean" goto clean
if "%1"=="build" goto build  
if "%1"=="test" goto test
if "%1"=="dev" goto dev
if "%1"=="" goto help

:help
echo Usage: test-razorfs.bat [command]
echo.
echo Commands:
echo   build    Build Docker image (FUSE only)
echo   test     Test FUSE filesystem  
echo   dev      Start development shell
echo   clean    Clean Docker resources
echo.
echo Examples:
echo   test-razorfs.bat build
echo   test-razorfs.bat test
echo   test-razorfs.bat dev
echo.
goto end

:build
echo Building FUSE-only Docker image...
docker-compose build
if errorlevel 1 (
    echo Build failed!
    goto end
)
echo Build complete!
goto end

:test
echo Testing FUSE filesystem...
docker-compose run --rm razorfs-fuse
goto end

:dev  
echo Starting development environment...
echo Type 'exit' to return to Windows
docker-compose run --rm razorfs-dev
goto end

:clean
echo Cleaning Docker resources...
docker-compose down --rmi all --volumes
echo Cleanup complete!
goto end

:end