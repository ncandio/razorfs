@echo off
REM Simple RAZOR Filesystem Test - Windows

echo === RAZOR Filesystem Simple Test ===
echo.

if "%1"=="clean" goto clean
if "%1"=="build" goto build  
if "%1"=="test" goto test
if "%1"=="dev" goto dev
if "%1"=="" goto help

:help
echo Usage: test-simple.bat [command]
echo.
echo Commands:
echo   build    Build Docker image (FUSE only)
echo   test     Test FUSE filesystem  
echo   dev      Start development shell
echo   clean    Clean Docker resources
echo.
goto end

:build
echo Building FUSE-only Docker image...
docker-compose -f docker-compose.simple.yml build
echo Build complete!
goto end

:test
echo Testing FUSE filesystem...
docker-compose -f docker-compose.simple.yml run --rm razorfs-fuse
goto end

:dev  
echo Starting development environment...
docker-compose -f docker-compose.simple.yml run --rm razorfs-dev
goto end

:clean
echo Cleaning Docker resources...
docker-compose -f docker-compose.simple.yml down --rmi all --volumes
goto end

:end