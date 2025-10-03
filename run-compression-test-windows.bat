@echo off
echo === RAZOR Filesystem Compression Test for Windows ===
echo.

REM Navigate to the testing directory
cd /d "C:\Users\liber\Desktop\Testing-Razor-FS"

echo 🐳 Building Docker image...
docker build -f Dockerfile.compression-test -t razor-compression-test .

if %ERRORLEVEL% neq 0 (
    echo ❌ Docker build failed
    pause
    exit /b 1
)

echo.
echo ✅ Docker image built successfully
echo.
echo 🚀 Running compression test...

REM Run the container with privileged mode for FUSE
docker run --rm --privileged --cap-add SYS_ADMIN --device /dev/fuse razor-compression-test

echo.
echo ✅ Test completed! Check the output above for compression statistics.
pause
