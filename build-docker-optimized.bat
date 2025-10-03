@echo off
echo Building optimized RazorFS Docker image...
echo.

REM Build the Docker image from the repository root
echo Building from repository root...
docker build --no-cache -f razorfs_windows_testing/Dockerfile.optimized.fixed -t razorfs-optimized .

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo ERROR: Docker build failed!
    echo.
    echo Troubleshooting tips:
    echo 1. Make sure Docker Desktop is running
    echo 2. Ensure you're running this from the RAZOR_repo directory
    echo 3. Check that all source files exist in src/ and tools/ directories
    exit /b 1
)

echo.
echo Build successful! You can now run the container with:
echo docker run --rm -v "%cd%\results:/results" razorfs-optimized
echo.
pause