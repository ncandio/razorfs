#!/bin/bash
# Sync test results to Windows

WINDOWS_PATH="/mnt/c/Users/liber/Desktop/Testing-Razor-FS"

echo "📤 Syncing RAZORFS test results to Windows..."

# Create directories if they don't exist
mkdir -p "$WINDOWS_PATH/results"
mkdir -p "$WINDOWS_PATH/graphs"
mkdir -p "$WINDOWS_PATH/data"

# Copy results
cp -v /results/*.dat "$WINDOWS_PATH/data/" 2>/dev/null || true
cp -v /results/*.png "$WINDOWS_PATH/graphs/" 2>/dev/null || true
cp -v /results/*.txt "$WINDOWS_PATH/results/" 2>/dev/null || true

# Copy test infrastructure
cp -v /home/nico/WORK_ROOT/RAZOR_repo/testing/*.sh "$WINDOWS_PATH/" 2>/dev/null || true
cp -v /home/nico/WORK_ROOT/RAZOR_repo/testing/*.gnuplot "$WINDOWS_PATH/" 2>/dev/null || true
cp -v /home/nico/WORK_ROOT/RAZOR_repo/testing/Dockerfile "$WINDOWS_PATH/" 2>/dev/null || true

echo "✅ Sync complete!"
echo "📁 Results available at: $WINDOWS_PATH"
