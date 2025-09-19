#!/bin/bash
# Sync RazorFS filesystem comparison tests to Windows
# Inspired by RAZOR testing framework sync workflow

set -e

# Configuration
WSL_SOURCE_DIR="/home/nico/WORK_ROOT/RAZOR_repo/TEST/razorfs"
WINDOWS_TARGET_DIR="/mnt/c/Users/liber/Desktop/Testing-Razor-FS"

echo "=== Syncing RazorFS Filesystem Comparison to Windows ==="
echo "Source: $WSL_SOURCE_DIR"
echo "Target: $WINDOWS_TARGET_DIR"

# Create target directory if it doesn't exist
if [ ! -d "$WINDOWS_TARGET_DIR" ]; then
    echo "Creating Windows target directory..."
    mkdir -p "$WINDOWS_TARGET_DIR"
fi

# Copy essential files
echo "Copying core files..."

# Docker and workflow files
cp -v workflow.md "$WINDOWS_TARGET_DIR/"
cp -v docker-compose-filesystem-comparison.yml "$WINDOWS_TARGET_DIR/"
cp -v Dockerfile.filesystem-comparison "$WINDOWS_TARGET_DIR/"
cp -v filesystem-test.bat "$WINDOWS_TARGET_DIR/"

# Source code
echo "Copying RazorFS source code..."
mkdir -p "$WINDOWS_TARGET_DIR/src"
mkdir -p "$WINDOWS_TARGET_DIR/fuse"
cp -rv src/* "$WINDOWS_TARGET_DIR/src/"
cp -rv fuse/* "$WINDOWS_TARGET_DIR/fuse/"

# Benchmark scripts
echo "Copying benchmark scripts..."
mkdir -p "$WINDOWS_TARGET_DIR/benchmarks"
mkdir -p "$WINDOWS_TARGET_DIR/scripts"
cp -rv benchmarks/* "$WINDOWS_TARGET_DIR/benchmarks/" 2>/dev/null || echo "No benchmark scripts to copy yet"
cp -rv scripts/* "$WINDOWS_TARGET_DIR/scripts/" 2>/dev/null || echo "No analysis scripts to copy yet"

# Create results directory
mkdir -p "$WINDOWS_TARGET_DIR/results"
mkdir -p "$WINDOWS_TARGET_DIR/test-data"

# Create README for Windows
cat > "$WINDOWS_TARGET_DIR/README.md" << 'EOF'
# RazorFS Filesystem Comparison Testing

This directory contains the Docker-based testing framework for comparing RazorFS against traditional filesystems (ext4, btrfs, reiserfs).

## Quick Start

1. **Setup Environment**:
   ```powershell
   .\filesystem-test.bat setup
   ```

2. **Run Diagnostics**:
   ```powershell
   .\filesystem-test.bat diagnostics
   ```

3. **Quick Test** (~5 minutes):
   ```powershell
   .\filesystem-test.bat quick
   ```

4. **Full Comparison** (~30 minutes):
   ```powershell
   .\filesystem-test.bat full
   ```

## Available Commands

- `diagnostics` - Check filesystem availability
- `quick` - Basic operations test (5 min)
- `full` - Comprehensive benchmarks (30 min)
- `micro` - Micro-benchmarks (path resolution, etc.)
- `stress` - Stress testing
- `clean` - Clean up Docker resources
- `copy-results` - Copy results from container

## Results

Test results are saved in the `results/` directory with timestamps.

## Philosophy: "Spezziamo le remi a Linus Torvalds"

We aim for brutally honest filesystem comparisons:
- No cherry-picked scenarios
- Real-world workloads
- Document both strengths and weaknesses
- Include failure modes and limitations

## Requirements

- Docker Desktop for Windows
- WSL2 backend enabled
- At least 8GB RAM (16GB recommended)
- 10GB free disk space

## Troubleshooting

See `workflow.md` for detailed troubleshooting steps.
EOF

# Create version info
echo "Creating version info..."
cat > "$WINDOWS_TARGET_DIR/VERSION.txt" << EOF
RazorFS Filesystem Comparison Testing Framework
Generated: $(date)
Source: $WSL_SOURCE_DIR
Target: $WINDOWS_TARGET_DIR

Git Information:
$(cd "$WSL_SOURCE_DIR" && git rev-parse --short HEAD 2>/dev/null || echo "Not a git repository")
$(cd "$WSL_SOURCE_DIR" && git log -1 --oneline 2>/dev/null || echo "No git history")

Files synced:
$(find "$WINDOWS_TARGET_DIR" -type f | wc -l) files
$(du -sh "$WINDOWS_TARGET_DIR" | cut -f1) total size
EOF

# Set permissions for Windows compatibility
echo "Setting file permissions..."
find "$WINDOWS_TARGET_DIR" -name "*.sh" -exec chmod +x {} \;
find "$WINDOWS_TARGET_DIR" -name "*.bat" -exec chmod +x {} \;

# Summary
echo ""
echo "=== Sync Complete ==="
echo "Files copied to: $WINDOWS_TARGET_DIR"
echo "Total files: $(find "$WINDOWS_TARGET_DIR" -type f | wc -l)"
echo "Total size: $(du -sh "$WINDOWS_TARGET_DIR" | cut -f1)"
echo ""
echo "Next steps:"
echo "1. Open PowerShell in Windows"
echo "2. Navigate to: C:\\Users\\liber\\Desktop\\Testing-Razor-FS"
echo "3. Run: .\\filesystem-test.bat setup"
echo "4. Run: .\\filesystem-test.bat diagnostics"
echo ""
echo "Ready to challenge traditional filesystems! 🚀"