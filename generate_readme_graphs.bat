@echo off
REM Generate README graphs for RAZORFS
REM Run this from Windows to create professional performance graphs

echo ================================================
echo   RAZORFS README Graph Generator
echo ================================================
echo.

REM Check for Python
python --version >nul 2>&1
if errorlevel 1 (
    echo [ERROR] Python not found. Please install Python 3.x
    pause
    exit /b 1
)

echo [1/3] Installing required packages...
pip install matplotlib numpy pandas seaborn --quiet

echo [2/3] Generating professional graphs...
python generate_readme_graphs.py

if errorlevel 1 (
    echo [ERROR] Graph generation failed
    pause
    exit /b 1
)

echo [3/3] Graphs generated successfully!
echo.
echo Output directory: readme_graphs\
echo.
echo Generated graphs:
echo   - ologn_scaling_validation.png
echo   - cache_performance_comparison.png
echo   - memory_numa_analysis.png
echo   - comprehensive_performance_radar.png
echo   - scalability_heatmap.png
echo   - compression_effectiveness.png
echo.
echo ================================================
echo   Graph generation complete!
echo ================================================
pause
