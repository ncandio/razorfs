#!/bin/bash
###############################################################################
# RAZORFS Windows Sync Script
#
# Automatically syncs test results, benchmarks, and graphs to Windows Desktop
# for easy access and sharing.
#
# Target: C:\Users\liber\Desktop\Testing-Razor-FS
###############################################################################

set -e

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

# Configuration
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
WINDOWS_SYNC_DIR="/mnt/c/Users/liber/Desktop/Testing-Razor-FS"
TIMESTAMP=$(date +"%Y%m%d_%H%M%S")
LOG_FILE="$REPO_ROOT/logs/windows_sync_$TIMESTAMP.log"

echo -e "${BLUE}╔══════════════════════════════════════════════════════════════╗${NC}"
echo -e "${BLUE}║   RAZORFS Windows Sync Script                                ║${NC}"
echo -e "${BLUE}║   Syncing to: C:\\Users\\liber\\Desktop\\Testing-Razor-FS      ║${NC}"
echo -e "${BLUE}╚══════════════════════════════════════════════════════════════╝${NC}"
echo ""

# Create log directory
mkdir -p "$REPO_ROOT/logs"

# Log function
log() {
    echo "$1" | tee -a "$LOG_FILE"
}

# Check if Windows mount point exists
if [ ! -d "/mnt/c/Users/liber" ]; then
    echo -e "${RED}ERROR: Windows mount point not found!${NC}"
    echo "Expected: /mnt/c/Users/liber"
    echo ""
    echo "Troubleshooting:"
    echo "1. Check if C: drive is mounted: ls /mnt/c/"
    echo "2. Restart WSL if needed: wsl --shutdown (from PowerShell)"
    echo "3. Verify user directory exists in Windows"
    exit 1
fi

# Create Windows sync directory structure
log "${BLUE}[1/6]${NC} Creating Windows directory structure..."
mkdir -p "$WINDOWS_SYNC_DIR"/{benchmarks,readme_graphs,logs,docker,tests}
mkdir -p "$WINDOWS_SYNC_DIR/benchmarks"/{data,graphs,reports,versioned_results}
log "${GREEN}✓${NC} Directory structure created"
echo ""

# Sync benchmarks
log "${BLUE}[2/6]${NC} Syncing benchmark results..."
if [ -d "$REPO_ROOT/benchmarks" ]; then
    rsync -av --progress "$REPO_ROOT/benchmarks/" "$WINDOWS_SYNC_DIR/benchmarks/" | tee -a "$LOG_FILE"
    BENCHMARK_COUNT=$(find "$WINDOWS_SYNC_DIR/benchmarks" -type f | wc -l)
    log "${GREEN}✓${NC} Synced $BENCHMARK_COUNT benchmark files"
else
    log "${YELLOW}⚠${NC} No benchmarks directory found, skipping"
fi
echo ""

# Sync README graphs
log "${BLUE}[3/6]${NC} Syncing README graphs..."
if [ -d "$REPO_ROOT/readme_graphs" ]; then
    rsync -av --progress "$REPO_ROOT/readme_graphs/" "$WINDOWS_SYNC_DIR/readme_graphs/" | tee -a "$LOG_FILE"
    GRAPH_COUNT=$(find "$WINDOWS_SYNC_DIR/readme_graphs" -name "*.png" | wc -l)
    log "${GREEN}✓${NC} Synced $GRAPH_COUNT PNG graphs"
else
    log "${YELLOW}⚠${NC} No readme_graphs directory found, skipping"
fi
echo ""

# Sync logs
log "${BLUE}[4/6]${NC} Syncing logs..."
if [ -d "$REPO_ROOT/logs" ]; then
    rsync -av --progress "$REPO_ROOT/logs/" "$WINDOWS_SYNC_DIR/logs/" | tee -a "$LOG_FILE"
    log "${GREEN}✓${NC} Logs synced"
else
    mkdir -p "$REPO_ROOT/logs"
    log "${YELLOW}⚠${NC} Created logs directory"
fi
echo ""

# Sync Docker files
log "${BLUE}[5/6]${NC} Syncing Docker configuration..."
cp "$REPO_ROOT/Dockerfile" "$WINDOWS_SYNC_DIR/docker/" 2>/dev/null || true
cp "$REPO_ROOT/docker-compose.yml" "$WINDOWS_SYNC_DIR/docker/" 2>/dev/null || true
cp "$REPO_ROOT/DOCKER_WORKFLOW.md" "$WINDOWS_SYNC_DIR/" 2>/dev/null || true
log "${GREEN}✓${NC} Docker files synced"
echo ""

# Create index file
log "${BLUE}[6/6]${NC} Creating index file..."
cat > "$WINDOWS_SYNC_DIR/INDEX.md" << EOF
# RAZORFS Test Results - Synced from WSL2

**Last Sync**: $(date)
**Commit**: $(cd "$REPO_ROOT" && git log -1 --format='%h - %s' 2>/dev/null || echo "Unknown")

## Directory Structure

\`\`\`
Testing-Razor-FS/
├── benchmarks/                  # Benchmark results
│   ├── BENCHMARK_REPORT_*.md    # Latest reports
│   ├── data/                    # Raw data files
│   ├── graphs/                  # Performance graphs
│   └── reports/                 # Historical reports
│
├── readme_graphs/               # README documentation graphs
│   ├── comprehensive_performance_radar.png
│   ├── ologn_scaling_validation.png
│   ├── scalability_heatmap.png
│   ├── compression_effectiveness.png
│   └── memory_numa_analysis.png
│
├── logs/                        # Execution logs
│   ├── windows_sync_*.log
│   ├── benchmark_run_*.log
│   └── docker_build_*.log
│
└── docker/                      # Docker configuration
    ├── Dockerfile
    └── docker-compose.yml
\`\`\`

## Latest Results

### Benchmarks
$(ls -1t "$WINDOWS_SYNC_DIR/benchmarks"/BENCHMARK_REPORT_*.md 2>/dev/null | head -5 | while read f; do
    echo "- $(basename "$f")"
done)

### Graphs Generated
$(ls -1 "$WINDOWS_SYNC_DIR/readme_graphs"/*.png 2>/dev/null | while read f; do
    echo "- $(basename "$f")"
done)

## Quick Access

- **Latest Benchmark Report**: Open \`benchmarks/BENCHMARK_REPORT_*.md\`
- **Performance Graphs**: Browse \`readme_graphs/\`
- **Execution Logs**: Check \`logs/\` for details

## WSL2 Source

- **Path**: \`/home/nico/WORK_ROOT/razorfs\`
- **Sync Script**: \`scripts/sync_to_windows.sh\`

## Regenerating Results

From WSL2 terminal:
\`\`\`bash
cd /home/nico/WORK_ROOT/razorfs

# Full benchmark suite
./tests/docker/benchmark_filesystems.sh

# README graphs only
./generate_tagged_graphs.sh

# Sync to Windows
./scripts/sync_to_windows.sh
\`\`\`

---
*Automatically generated by sync_to_windows.sh*
EOF

log "${GREEN}✓${NC} Index file created"
echo ""

# Generate summary
echo -e "${GREEN}╔══════════════════════════════════════════════════════════════╗${NC}"
echo -e "${GREEN}║   Sync Complete!                                            ║${NC}"
echo -e "${GREEN}╚══════════════════════════════════════════════════════════════╝${NC}"
echo ""
echo "Results synced to Windows:"
echo "  ${BLUE}C:\\Users\\liber\\Desktop\\Testing-Razor-FS\\${NC}"
echo ""
echo "Summary:"
echo "  - Benchmarks: $BENCHMARK_COUNT files"
echo "  - Graphs: $GRAPH_COUNT PNG files"
echo "  - Log: $LOG_FILE"
echo ""
echo "Open in Windows Explorer:"
echo "  ${YELLOW}explorer.exe \"C:\\Users\\liber\\Desktop\\Testing-Razor-FS\"${NC}"
echo ""
echo "View latest benchmark report:"
LATEST_REPORT=$(ls -1t "$WINDOWS_SYNC_DIR/benchmarks"/BENCHMARK_REPORT_*.md 2>/dev/null | head -1)
if [ -n "$LATEST_REPORT" ]; then
    echo "  ${YELLOW}notepad.exe \"$LATEST_REPORT\"${NC}"
fi
echo ""

# Success
exit 0
