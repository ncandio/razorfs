#!/bin/bash
# Master test runner for RAZORFS

set -e

echo "========================================="
echo "  RAZORFS Testing Infrastructure"
echo "========================================="
echo ""

# Build Docker image
echo "🐳 Building Docker image..."
docker build -t razorfs-test -f testing/Dockerfile .

# Run benchmarks
echo ""
echo "🚀 Running comprehensive benchmarks..."
docker run --rm --privileged \
    -v $(pwd)/testing:/testing \
    -v /tmp/razorfs-results:/results \
    razorfs-test \
    bash /testing/benchmark.sh

# Generate graphs
echo ""
echo "📊 Generating visualizations..."
docker run --rm \
    -v /tmp/razorfs-results:/results \
    razorfs-test \
    gnuplot /testing/visualize.gnuplot

# Sync to Windows
echo ""
echo "📤 Syncing results to Windows..."
bash testing/sync-to-windows.sh

echo ""
echo "✅ All tests complete!"
echo "📁 Results: /tmp/razorfs-results"
echo "📁 Windows: /mnt/c/Users/liber/Desktop/Testing-Razor-FS"
