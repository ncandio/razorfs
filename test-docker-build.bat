@echo off
echo Testing RazorFS Docker builds...
echo.

echo Step 1: Testing simple build...
docker build --no-cache -f razorfs_windows_testing/Dockerfile.simple -t razorfs-simple .

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo ERROR: Simple build failed!
    echo This is likely due to missing source files or compilation errors.
    pause
    exit /b 1
)

echo.
echo Step 2: Running simple container to verify build...
docker run --rm razorfs-simple

echo.
echo Simple build successful!
echo.
echo Step 3: Now trying optimized build...
docker build --no-cache -f razorfs_windows_testing/Dockerfile.optimized.fixed -t razorfs-optimized .

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo ERROR: Optimized build failed!
    echo The simple build worked, so this might be a dependency issue.
    pause
    exit /b 1
)

echo.
echo All builds successful!
echo.
echo You can now run tests with:
echo   docker run --rm -v "%cd%\results:/results" razorfs-optimized
echo.
pause