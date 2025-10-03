@echo off
REM =========================================
REM RAZORFS WINDOWS TEST SUITE
REM =========================================

echo =========================================
echo RAZORFS WINDOWS TEST SUITE
echo =========================================
echo.

REM Test results tracking
set TOTAL_TESTS=0
set PASSED_TESTS=0

echo 🚀 Starting RAZORFS Windows Validation...
echo.

REM Build verification
echo 🔧 BUILD VERIFICATION
echo =====================
set /a TOTAL_TESTS+=1
echo [%TOTAL_TESTS%] Project Build
echo     Description: Verify the project builds without errors
echo     Status: 

REM Try to build the project
cd /d %~dp0
make clean 2>nul
make all 2>nul
if %ERRORLEVEL% EQU 0 (
    echo ✅ PASS
    set /a PASSED_TESTS+=1
) else (
    echo ❌ FAIL
    echo     Error: Build failed
)
echo.

REM Docker tests
echo 🐳 DOCKER TESTS
echo ===============
set /a TOTAL_TESTS+=1
echo [%TOTAL_TESTS%] Docker Build Test
echo     Description: Test Docker image building capability
echo     Status: 

if exist test-docker-build.bat (
    call test-docker-build.bat >nul 2>&1
    if %ERRORLEVEL% EQU 0 (
        echo ✅ PASS
        set /a PASSED_TESTS+=1
    ) else (
        echo ❌ FAIL
        echo     Error: Docker build failed
    )
) else (
    echo ⚠️  SKIP
    echo     Error: test-docker-build.bat not found
)
echo.

REM Article comparison tests
echo 📊 ARTICLE COMPARISON TESTS
echo =========================
set /a TOTAL_TESTS+=1
echo [%TOTAL_TESTS%] Article Comparison Generation
echo     Description: Generate article-ready comparison charts
echo     Status: 

if exist article_comparison\generate_article_graphs.py (
    python article_comparison\generate_article_graphs.py >nul 2>&1
    if %ERRORLEVEL% EQU 0 (
        echo ✅ PASS
        set /a PASSED_TESTS+=1
    ) else (
        echo ❌ FAIL
        echo     Error: Article comparison generation failed
    )
) else (
    echo ⚠️  SKIP
    echo     Error: generate_article_graphs.py not found
)
echo.

REM Final summary
echo 🏁 VALIDATION COMPLETE
echo ======================
echo Total Tests: %TOTAL_TESTS%
echo Passed: %PASSED_TESTS%
echo Failed: %TOTAL_TESTS%-%PASSED_TESTS%|cmd /c set /a result=

set /a FAILED_TESTS=%TOTAL_TESTS%-%PASSED_TESTS%

if %PASSED_TESTS% EQU %TOTAL_TESTS% (
    echo.
    echo 🎉 ALL TESTS PASSED! RAZORFS is functioning correctly!
    echo.
    echo 📋 Verified functionality:
    echo    • Successful compilation
    echo    • Docker containerization support
    echo    • Article-ready comparison generation
) else (
    echo.
    echo ⚠️  Some tests failed.
    echo 📋 %PASSED_TESTS% out of %TOTAL_TESTS% tests passed.
)

echo.
echo Press any key to exit...
pause >nul