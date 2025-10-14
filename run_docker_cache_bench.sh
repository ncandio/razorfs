#!/bin/bash
# run_docker_cache_bench.sh - Script to run the complete cache locality benchmark workflow

echo "Starting RazorFS Cache Locality Benchmark Workflow..."
echo "This will:"
echo "1. Build the Docker image with RazorFS"
echo "2. Run cache locality benchmarks comparing RazorFS (FUSE3) with ext4"
echo "3. Generate plots using gnuplot and Python (numpy/matplotlib)"
echo "4. Save results to ./benchmarks/results/"
echo

# Build and run with docker-compose
echo "Building and running Docker container..."
docker-compose up --build

echo
echo "Benchmark complete!"
echo "Results are available in ./benchmarks/results/"
echo
echo "Key files generated:"
echo "- cache_locality_benchmark.csv - Main benchmark results"
echo "- cache_locality_comparison.png - Performance comparison plot (gnuplot)"
echo "- cache_locality_comparison_matplotlib.png - Performance comparison plot (Python)"
echo "- cache_miss_analysis.png - Cache miss rate comparison (gnuplot)"
echo "- cache_miss_comparison_matplotlib.png - Cache miss rate comparison (Python)"
echo "- cache_locality_heatmap.png - Performance heatmap (Python)"
echo "- cache_locality_summary.txt - Summary analysis"
echo "- cache_locality_python_report.txt - Detailed Python report"