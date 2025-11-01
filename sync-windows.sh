#!/bin/bash
###############################################################################
# RAZORFS Windows Sync Script
#
# Automatically syncs Docker artifacts, benchmarks, and graphs from WSL2 to
# Windows Desktop for easy access and testing.
#
# WSL2 Source: /home/nico/WORK_ROOT/razorfs
# Windows Target: C:\Users\liber\Desktop\Testing-Razor-FS
###############################################################################

set -e

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

# Configuration
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WINDOWS_SYNC_DIR="/mnt/c/Users/liber/Desktop/Testing-Razor-FS"
TIMESTAMP=$(date +"%Y%m%d_%H%M%S")
LOG_FILE="$REPO_ROOT/logs/windows_sync_$TIMESTAMP.log"

echo -e "${BLUE}╔══════════════════════════════════════════════════════════════╗${NC}"
echo -e "${BLUE}║   RAZORFS Windows Sync Script                                ║${NC}"
echo -e "${BLUE}║   WSL2 → Windows Desktop Synchronization                     ║${NC}"
echo -e "${BLUE}╚══════════════════════════════════════════════════════════════╝${NC}"
echo ""

# Create log directory
mkdir -p "$REPO_ROOT/logs"

# Log function
log() {
    echo -e "$1" | tee -a "$LOG_FILE"
}

# Check if Windows mount point exists
if [ ! -d "/mnt/c/Users/liber" ]; then
    log "${RED}ERROR: Windows mount point not found!${NC}"
    echo "Expected: /mnt/c/Users/liber"
    echo ""
    echo "Troubleshooting:"
    echo "1. Check if C: drive is mounted: ls /mnt/c/"
    echo "2. Restart WSL if needed: wsl --shutdown (from PowerShell)"
    echo "3. Verify user directory exists in Windows"
    exit 1
fi

log "${GREEN}✓${NC} Windows mount point accessible"
echo ""

# Create Windows sync directory structure
log "${BLUE}[1/7]${NC} Creating Windows directory structure..."
mkdir -p "$WINDOWS_SYNC_DIR"/{benchmarks,readme_graphs,logs,docker,tests,data}
mkdir -p "$WINDOWS_SYNC_DIR/benchmarks"/{data,graphs,reports,versioned_results}
log "${GREEN}✓${NC} Directory structure created"
echo ""

# Sync Docker files
log "${BLUE}[2/7]${NC} Syncing Docker configuration..."
cp -v "$REPO_ROOT/Dockerfile" "$WINDOWS_SYNC_DIR/docker/" 2>/dev/null || true
cp -v "$REPO_ROOT/docker-compose.yml" "$WINDOWS_SYNC_DIR/docker/" 2>/dev/null || true
if [ -f "$REPO_ROOT/docs/development/DOCKER_WORKFLOW.md" ]; then
    cp -v "$REPO_ROOT/docs/development/DOCKER_WORKFLOW.md" "$WINDOWS_SYNC_DIR/WORKFLOW.md"
fi
log "${GREEN}✓${NC} Docker files synced"
echo ""

# Sync benchmark scripts
log "${BLUE}[3/7]${NC} Syncing benchmark scripts..."
cp -v "$REPO_ROOT/tests/docker/benchmark_filesystems.sh" "$WINDOWS_SYNC_DIR/docker/" 2>/dev/null || true
cp -v "$REPO_ROOT/generate_tagged_graphs.sh" "$WINDOWS_SYNC_DIR/docker/" 2>/dev/null || true
cp -v "$REPO_ROOT/run_benchmarks_wsl.sh" "$WINDOWS_SYNC_DIR/docker/" 2>/dev/null || true
log "${GREEN}✓${NC} Scripts synced"
echo ""

# Sync benchmarks
log "${BLUE}[4/7]${NC} Syncing benchmark results..."
if [ -d "$REPO_ROOT/benchmarks" ]; then
    cp -rv "$REPO_ROOT/benchmarks/"* "$WINDOWS_SYNC_DIR/benchmarks/" 2>&1 | tee -a "$LOG_FILE" | grep -E "(directory|->)" | head -20 || true
    BENCHMARK_COUNT=$(find "$WINDOWS_SYNC_DIR/benchmarks" -type f 2>/dev/null | wc -l)
    log "${GREEN}✓${NC} Synced $BENCHMARK_COUNT benchmark files"
else
    log "${YELLOW}⚠${NC} No benchmarks directory found, skipping"
    BENCHMARK_COUNT=0
fi
echo ""

# Sync README graphs
log "${BLUE}[5/7]${NC} Syncing README graphs..."
if [ -d "$REPO_ROOT/readme_graphs" ]; then
    cp -rv "$REPO_ROOT/readme_graphs/"* "$WINDOWS_SYNC_DIR/readme_graphs/" 2>&1 | tee -a "$LOG_FILE" | grep -E "(directory|->)" | head -20 || true
    GRAPH_COUNT=$(find "$WINDOWS_SYNC_DIR/readme_graphs" -name "*.png" -o -name "*.svg" 2>/dev/null | wc -l)
    log "${GREEN}✓${NC} Synced $GRAPH_COUNT graph files"
else
    log "${YELLOW}⚠${NC} No readme_graphs directory found, skipping"
    GRAPH_COUNT=0
fi
echo ""

# Sync logs
log "${BLUE}[6/7]${NC} Syncing logs..."
if [ -d "$REPO_ROOT/logs" ]; then
    cp -rv "$REPO_ROOT/logs/"* "$WINDOWS_SYNC_DIR/logs/" 2>&1 | tee -a "$LOG_FILE" | grep -E "(directory|->)" | head -10 || true
    log "${GREEN}✓${NC} Logs synced"
else
    log "${YELLOW}⚠${NC} Created logs directory"
fi
echo ""

# Create index file
log "${BLUE}[7/7]${NC} Creating index file..."
COMMIT_SHA=$(cd "$REPO_ROOT" && git log -1 --format='%h' 2>/dev/null || echo "Unknown")
COMMIT_MSG=$(cd "$REPO_ROOT" && git log -1 --format='%s' 2>/dev/null || echo "No commit info")

cat > "$WINDOWS_SYNC_DIR/README.md" << EOF
# RAZORFS Testing Environment - Windows Sync

**Last Sync**: $(date '+%Y-%m-%d %H:%M:%S')  
**Commit**: \`$COMMIT_SHA\` - $COMMIT_MSG  
**WSL2 Source**: \`/home/nico/WORK_ROOT/razorfs\`

---

## 📁 Directory Structure

\`\`\`
Testing-Razor-FS/
├── docker/                      # Docker configuration
│   ├── Dockerfile               # RAZORFS test container
│   ├── docker-compose.yml       # Multi-container setup
│   ├── benchmark_filesystems.sh # Benchmark script
│   └── generate_tagged_graphs.sh
│
├── benchmarks/                  # Benchmark results
│   ├── BENCHMARK_REPORT_*.md    # Latest reports
│   ├── data/                    # Raw CSV/DAT files
│   ├── graphs/                  # PNG graphs
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
│   └── *.log
│
├── tests/                       # Test scripts
└── WORKFLOW.md                  # Complete workflow guide
\`\`\`

---

## 🚀 Quick Start

### Running Benchmarks in Docker (Windows)

**Prerequisites**: Docker Desktop for Windows installed and running

\`\`\`powershell
# Open PowerShell in this directory
cd C:\\Users\\liber\\Desktop\\Testing-Razor-FS

# Build Docker image
docker build -t razorfs-test -f docker/Dockerfile ..

# Run full benchmark suite
docker run --privileged -v \${PWD}/benchmarks:/app/benchmarks razorfs-test ./tests/docker/benchmark_filesystems.sh

# Generate README graphs
docker run -v \${PWD}/readme_graphs:/app/readme_graphs razorfs-test ./generate_tagged_graphs.sh
\`\`\`

### Running Benchmarks in WSL2

\`\`\`bash
# From Ubuntu WSL2 terminal
cd /home/nico/WORK_ROOT/razorfs

# Run full benchmark (Docker + native)
./tests/docker/benchmark_filesystems.sh

# Generate README graphs
./generate_tagged_graphs.sh

# Sync results to Windows
./sync-windows.sh
\`\`\`

---

## 📊 Latest Results

### Benchmark Reports
$(if [ -d "$WINDOWS_SYNC_DIR/benchmarks" ]; then
    ls -1t "$WINDOWS_SYNC_DIR/benchmarks"/BENCHMARK_REPORT_*.md 2>/dev/null | head -5 | while read f; do
        echo "- [$(basename "$f")](benchmarks/$(basename "$f"))"
    done
else
    echo "- No reports generated yet"
fi)

### Available Graphs
$(if [ -d "$WINDOWS_SYNC_DIR/readme_graphs" ] && [ "$(ls -A "$WINDOWS_SYNC_DIR/readme_graphs" 2>/dev/null)" ]; then
    ls -1 "$WINDOWS_SYNC_DIR/readme_graphs"/*.png "$WINDOWS_SYNC_DIR/readme_graphs"/*.svg 2>/dev/null | while read f; do
        echo "- ![$(basename "$f")](readme_graphs/$(basename "$f"))"
    done
else
    echo "- No graphs generated yet - run \`generate_tagged_graphs.sh\`"
fi)

---

## 🔄 Filesystem Comparison

This testing infrastructure compares **RazorFS** against:

| Filesystem | Type | Features Tested |
|------------|------|----------------|
| **ext4** | Traditional | Baseline performance, POSIX compliance |
| **ZFS** | Modern | Compression, snapshots, integrity |
| **btrfs** | Modern | Copy-on-write, compression, subvolumes |
| **RazorFS** | Experimental | N-ary tree, NUMA-aware, compression |

### Test Scenarios

1. **Compression Efficiency** - How well each filesystem compresses real data
2. **Recovery Performance** - Crash recovery and data integrity
3. **NUMA Friendliness** - Memory locality on multi-socket systems
4. **Persistence** - Data durability across mount/unmount cycles
5. **O(log n) Scaling** - Lookup performance with increasing file counts

---

## 📖 Documentation

- **[WORKFLOW.md](WORKFLOW.md)** - Complete Docker testing workflow
- **[Main README](../razorfs/README.md)** - Project overview (in WSL2)
- **Docker Files** - See \`docker/\` directory

---

## 🔧 Useful Commands

### View Latest Benchmark Report
\`\`\`powershell
notepad.exe "$(Get-ChildItem -Path benchmarks\\BENCHMARK_REPORT_*.md | Sort-Object LastWriteTime -Descending | Select-Object -First 1).FullName"
\`\`\`

### Open Results in Browser
\`\`\`powershell
explorer.exe benchmarks\\graphs
explorer.exe readme_graphs
\`\`\`

### Check Sync Log
\`\`\`powershell
Get-Content logs\\windows_sync_*.log | Select-Object -Last 50
\`\`\`

---

## 🆘 Troubleshooting

### Docker Not Starting
\`\`\`powershell
# Restart Docker Desktop
Stop-Process -Name "Docker Desktop" -Force
Start-Process "C:\\Program Files\\Docker\\Docker\\Docker Desktop.exe"
\`\`\`

### WSL2 Connection Issues
\`\`\`powershell
# Restart WSL2
wsl --shutdown
wsl
\`\`\`

### Sync Not Working
\`\`\`bash
# From WSL2, check Windows path
ls /mnt/c/Users/liber/Desktop/Testing-Razor-FS

# Re-run sync manually
cd /home/nico/WORK_ROOT/razorfs
./sync-windows.sh
\`\`\`

---

**Generated by**: \`sync-windows.sh\`  
**Sync Log**: [logs/windows_sync_$TIMESTAMP.log](logs/windows_sync_$TIMESTAMP.log)

EOF

log "${GREEN}✓${NC} Index file created"
echo ""

# Copy sync log to Windows
cp "$LOG_FILE" "$WINDOWS_SYNC_DIR/logs/"

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
echo "  - Graphs: $GRAPH_COUNT files"
echo "  - Commit: $COMMIT_SHA"
echo "  - Log: $LOG_FILE"
echo ""
echo "Quick Access:"
echo "  ${YELLOW}explorer.exe C:\\Users\\liber\\Desktop\\Testing-Razor-FS${NC}"
echo ""
if [ -f "$WINDOWS_SYNC_DIR/WORKFLOW.md" ]; then
    echo "View workflow guide:"
    echo "  ${YELLOW}notepad.exe \"$WINDOWS_SYNC_DIR\\WORKFLOW.md\"${NC}"
    echo ""
fi

# Success
log "${GREEN}✓${NC} Sync completed successfully at $(date)"
exit 0
