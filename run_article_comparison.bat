@echo off
REM Article-Ready Filesystem Comparison for RAZORFS
REM Comprehensive benchmarking of RAZORFS vs ext4, reiserfs, ext2

echo ==========================================
echo     RAZORFS Article-Ready Comparison
echo ==========================================
echo.
echo This will build and test RAZORFS against traditional filesystems
echo and generate publication-quality comparison charts.
echo.

REM Create output directory
mkdir graphs 2>nul

echo 1. Building RAZORFS Docker image...
docker build -t razorfs-article -f article_comparison/Dockerfile.article .

echo.
echo 2. Running comprehensive filesystem comparison...
docker run --rm -v %cd%\graphs:/article_output razorfs-article python3 /razor_repo/article_comparison/generate_article_graphs.py

echo.
echo 3. Copying results to local directory...
docker run --rm -v %cd%\graphs:/article_output razorfs-article cp /article_output/* /article_output/

echo.
echo 4. Results saved to graphs/ directory:
dir graphs

echo.
echo 🎉 Article-ready comparison complete!
echo Check the graphs/ directory for:
echo  - comprehensive_filesystem_comparison.png
echo  - ologn_complexity_analysis.png
echo.
echo These charts show RAZORFS vs ext4, reiserfs, and ext2 performance.
pause