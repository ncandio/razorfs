# RAZORFS Docker Testing Workflow

**Complete guide for running Docker-based benchmarks and syncing results to Windows**

---

## 🎯 Overview

This workflow enables you to:
1. **Test RazorFS virtually** against ext4, btrfs, and ZFS using Docker containers
2. **Generate performance graphs** with commit-tagged reproducibility
3. **Sync results** between WSL2 and Windows Desktop for easy access
4. **Run benchmarks** in both WSL2 (development) and Windows (Docker Desktop)

---

## 🏗️ Architecture

### Environment Setup

```
┌─────────────────────────────────────────────────────────────────┐
│                    Windows 11 Host                              │
│                                                                 │
│  ┌────────────────────────────────────────────────────────┐   │
│  │  Windows Filesystem                                     │   │
│  │  C:\Users\liber\Desktop\Testing-Razor-FS\              │   │
│  │  ├── benchmarks/      (synced results)                 │   │
│  │  ├── readme_graphs/   (performance visualizations)     │   │
│  │  ├── docker/          (Dockerfile + scripts)           │   │
│  │  └── logs/            (execution logs)                 │   │
│  └────────────────────────────────────────────────────────┘   │
│                            ▲                                    │
│                            │ sync-windows.sh                    │
│  ┌────────────────────────────────────────────────────────┐   │
│  │  WSL2 (Ubuntu 22.04)                                    │   │
│  │  /home/nico/WORK_ROOT/razorfs/                         │   │
│  │                                                         │   │
│  │  ┌──────────────────────────────────────────────┐     │   │
│  │  │  Native RazorFS Testing                      │     │   │
│  │  │  • FUSE3 mounting                             │     │   │
│  │  │  • /dev/shm for fast testing                 │     │   │
│  │  │  • Direct filesystem operations               │     │   │
│  │  └──────────────────────────────────────────────┘     │   │
│  │                                                         │   │
│  │  ┌──────────────────────────────────────────────┐     │   │
│  │  │  Docker Containers (Virtualized FS)          │     │   │
│  │  │                                               │     │   │
│  │  │  ┌────────┐  ┌────────┐  ┌────────┐         │     │   │
│  │  │  │ ext4   │  │ btrfs  │  │  ZFS   │         │     │   │
│  │  │  │ Test   │  │ Test   │  │ Test   │         │     │   │
│  │  │  └────────┘  └────────┘  └────────┘         │     │   │
│  │  │  • Isolated environments                     │     │   │
│  │  │  • Loop devices for disk images              │     │   │
│  │  │  • Privileged mode for FS creation           │     │   │
│  │  └──────────────────────────────────────────────┘     │   │
│  └────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────┘
```

---

## 📋 Prerequisites

### WSL2 Environment
```bash
# Install required packages
sudo apt-get update
sudo apt-get install -y \
    build-essential \
    pkg-config \
    libfuse3-dev \
    zlib1g-dev \
    gnuplot \
    bc \
    wget \
    rsync \
    docker.io

# Add user to docker group
sudo usermod -aG docker $USER
newgrp docker
```

### Windows Environment
- Windows 10/11 (Build 19041+)
- WSL2 enabled
- Docker Desktop for Windows (optional, for testing on Windows side)
- Test directory: `C:\Users\liber\Desktop\Testing-Razor-FS`

---

## 🚀 Quick Start

### 1. Initial Setup (One-Time)

```bash
cd /home/nico/WORK_ROOT/razorfs

# Build RazorFS
make clean && make

# Create Windows sync directory
mkdir -p /mnt/c/Users/liber/Desktop/Testing-Razor-FS

# Verify setup
./sync-windows.sh
```

### 2. Run Benchmarks in WSL2

```bash
# Full benchmark suite (RazorFS + ext4 + btrfs + ZFS in Docker)
./tests/docker/benchmark_filesystems.sh

# Results are saved to:
# - benchmarks/BENCHMARK_REPORT_*.md
# - benchmarks/data/*.dat
# - benchmarks/graphs/*.png
```

### 3. Generate README Graphs

```bash
# Generate commit-tagged graphs for README
./generate_tagged_graphs.sh

# Creates 5 professional graphs in readme_graphs/
# Each graph is tagged with current commit SHA
```

### 4. Sync to Windows

```bash
# Automatic sync to Windows Desktop
./sync-windows.sh

# Results appear in:
# C:\Users\liber\Desktop\Testing-Razor-FS\
```

---

## 📊 Test Scenarios

### 1. Compression Efficiency Test

**What it tests**: How well each filesystem compresses real-world data

**Methodology**:
- Downloads git source archive (~10MB)
- Copies to each filesystem
- Measures actual disk usage
- Calculates compression ratio

**Filesystems tested**:
- RazorFS (zlib compression)
- ext4 (no compression)
- btrfs (ZSTD compression)
- ZFS (LZ4 compression)

### 2. Recovery Performance Test

**What it tests**: Crash recovery speed and data integrity

**Methodology**:
- Write test data
- Unmount filesystem
- Remount filesystem
- Verify data integrity
- Measure recovery time

### 3. NUMA Friendliness Test

**What it tests**: Memory locality on NUMA architectures

**Methodology**:
- Evaluates memory allocation patterns
- Measures access latency
- Scores NUMA optimization (0-100)

**RazorFS advantage**: Built-in `numa_bind_memory()` support

### 4. Persistence Verification Test

**What it tests**: Data durability across mount/unmount cycles

**Methodology**:
- Create 1MB random data file
- Calculate MD5 checksum
- Unmount filesystem
- Remount filesystem
- Verify MD5 checksum matches

---

## 🔄 Common Workflows

### Workflow A: Daily Development

**Use Case**: Testing code changes during development

```bash
# 1. Make changes to RazorFS
vim src/nary_tree_mt.c

# 2. Rebuild
make clean && make

# 3. Quick benchmark (RazorFS only, no Docker)
./tests/shell/razorfs-only-benchmark.sh

# 4. If satisfied, full comparison with Docker
./tests/docker/benchmark_filesystems.sh

# 5. Generate graphs and sync
./generate_tagged_graphs.sh
./sync-windows.sh

# 6. View results on Windows
# Open: C:\Users\liber\Desktop\Testing-Razor-FS\
```

**Time**: 5-10 minutes

### Workflow B: Pre-Commit Validation

**Use Case**: Validate performance before committing

```bash
# 1. Run full benchmark suite
./tests/docker/benchmark_filesystems.sh

# 2. Review results
cat benchmarks/BENCHMARK_REPORT_*.md | less

# 3. If good, commit changes
git add .
git commit -m "perf: optimize lookup algorithm"

# 4. Regenerate graphs with new commit tag
./generate_tagged_graphs.sh
git add readme_graphs/
git commit -m "docs: update performance graphs"

# 5. Sync to Windows
./sync-windows.sh
```

**Time**: 15-20 minutes

### Workflow C: Testing on Windows (Docker Desktop)

**Use Case**: Run benchmarks using Windows Docker Desktop

```powershell
# From PowerShell
cd C:\Users\liber\Desktop\Testing-Razor-FS

# Build Docker image (first time only)
docker build -t razorfs-test -f docker\Dockerfile .

# Run benchmarks
docker run --privileged `
  -v ${PWD}\benchmarks:/app/benchmarks `
  razorfs-test `
  bash -c "./tests/docker/benchmark_filesystems.sh"

# Generate graphs
docker run `
  -v ${PWD}\readme_graphs:/app/readme_graphs `
  razorfs-test `
  ./generate_tagged_graphs.sh

# View results
explorer.exe benchmarks\graphs
explorer.exe readme_graphs
```

**Time**: 15-20 minutes

### Workflow D: Continuous Integration

**Use Case**: Automated testing on every commit

```yaml
# .github/workflows/benchmark.yml
name: Performance Benchmarks

on: [push, pull_request]

jobs:
  benchmark:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3
      
      - name: Install dependencies
        run: |
          sudo apt-get update
          sudo apt-get install -y libfuse3-dev zlib1g-dev gnuplot
      
      - name: Build RazorFS
        run: make clean && make
      
      - name: Run benchmarks
        run: ./tests/docker/benchmark_filesystems.sh
      
      - name: Generate graphs
        run: ./generate_tagged_graphs.sh
      
      - name: Upload artifacts
        uses: actions/upload-artifact@v3
        with:
          name: performance-results-${{ github.sha }}
          path: |
            benchmarks/
            readme_graphs/
```

---

## 📁 Directory Structure

### WSL2 Structure
```
/home/nico/WORK_ROOT/razorfs/
├── src/                          # RazorFS source code
├── tests/
│   ├── docker/
│   │   └── benchmark_filesystems.sh    # Main benchmark script
│   └── shell/
│       └── razorfs-only-benchmark.sh   # Quick RazorFS test
├── benchmarks/                   # Generated results
│   ├── BENCHMARK_REPORT_*.md
│   ├── data/*.dat
│   └── graphs/*.png
├── readme_graphs/                # Documentation graphs
├── Dockerfile                    # Docker test environment
├── generate_tagged_graphs.sh     # Graph generation script
└── sync-windows.sh              # Windows sync script
```

### Windows Structure
```
C:\Users\liber\Desktop\Testing-Razor-FS\
├── docker/
│   ├── Dockerfile
│   ├── benchmark_filesystems.sh
│   └── generate_tagged_graphs.sh
├── benchmarks/
│   ├── BENCHMARK_REPORT_*.md
│   ├── data/
│   └── graphs/
├── readme_graphs/
├── logs/
├── README.md                     # Auto-generated index
└── WORKFLOW.md                   # This file
```

---

## 🔧 Advanced Configuration

### Customizing Benchmarks

Edit `tests/docker/benchmark_filesystems.sh`:

```bash
# Test file configuration
TEST_FILE_URL="https://github.com/git/git/archive/refs/tags/v2.43.0.tar.gz"
TEST_FILE_NAME="git-2.43.0.tar.gz"

# Output configuration
RESULTS_DIR="$REPO_ROOT/benchmarks"
TIMESTAMP=$(date +%Y%m%d_%H%M%S)

# Docker image selection
DOCKER_IMAGE="ubuntu:22.04"
```

### Adding New Filesystem Tests

```bash
# Add new test in benchmark_filesystems.sh
test_compression "btrfs" "/mnt/btrfs" "
    truncate -s 100M /tmp/btrfs.img
    mkfs.btrfs -f /tmp/btrfs.img
    mkdir -p /mnt/btrfs
    mount -o loop,compress=zstd /tmp/btrfs.img /mnt/btrfs
"
```

### Custom Graph Generation

Edit `generate_tagged_graphs.sh` to customize graph appearance:

```bash
# Change graph size
set terminal pngcairo enhanced font 'Arial Bold,14' size 1200,900

# Change colors
set style line 1 lc rgb '#3498db' lt 1 lw 3

# Add custom labels
set label "Custom Text" at screen 0.5, screen 0.5 center
```

---

## 🐛 Troubleshooting

### Problem: Docker permission denied

```bash
# Solution: Add user to docker group
sudo usermod -aG docker $USER
newgrp docker

# Verify
docker ps
```

### Problem: WSL2 can't access Windows directory

```bash
# Check mount
ls /mnt/c/Users/liber/

# If not mounted, restart WSL (from PowerShell)
wsl --shutdown
wsl
```

### Problem: Benchmark script fails

```bash
# Check dependencies
which gnuplot bc wget docker

# Install missing packages
sudo apt-get install -y gnuplot bc wget

# Check Docker
docker run hello-world
```

### Problem: Sync fails

```bash
# Check Windows path exists
mkdir -p /mnt/c/Users/liber/Desktop/Testing-Razor-FS

# Run sync with verbose output
bash -x ./sync-windows.sh

# Check permissions
ls -la /mnt/c/Users/liber/Desktop/
```

### Problem: Graphs not generating

```bash
# Install gnuplot
sudo apt-get install -y gnuplot gnuplot-qt

# Test gnuplot
gnuplot --version

# Check data files exist
ls -la benchmarks/data/
```

---

## 📚 Reference

### Key Scripts

| Script | Purpose | Location |
|--------|---------|----------|
| `benchmark_filesystems.sh` | Full benchmark suite | `tests/docker/` |
| `generate_tagged_graphs.sh` | Create README graphs | Root |
| `sync-windows.sh` | Sync to Windows | Root |
| `razorfs-only-benchmark.sh` | Quick RazorFS test | `tests/shell/` |

### Generated Files

| File | Description |
|------|-------------|
| `BENCHMARK_REPORT_*.md` | Detailed benchmark results |
| `data/*.dat` | Raw benchmark data |
| `graphs/*.png` | Performance comparison graphs |
| `readme_graphs/*.png` | Documentation graphs (commit-tagged) |

### Environment Variables

```bash
# Customize behavior
export RAZORFS_VERSION="Phase7"
export TEST_MODE="full"
export WINDOWS_SYNC_DIR="/mnt/c/Users/liber/Desktop/Testing-Razor-FS"
```

---

## 🎯 Best Practices

1. **✅ Always tag commits** - Graphs include commit SHA for traceability
2. **✅ Run full benchmarks before releases** - Catch performance regressions
3. **✅ Keep Windows directory synced** - Easy sharing and presentation
4. **✅ Version results** - Keep historical data in `versioned_results/`
5. **✅ Document changes** - Update CHANGELOG with performance impacts
6. **✅ Clean Docker cache** - Periodically run `docker system prune -a`

---

## 📖 Additional Documentation

- **[README.md](README.md)** - Project overview
- **[docs/TESTING.md](docs/TESTING.md)** - Comprehensive testing guide
- **[docs/development/DOCKER_WORKFLOW.md](docs/development/DOCKER_WORKFLOW.md)** - Detailed Docker documentation
- **[CONTRIBUTING.md](CONTRIBUTING.md)** - Contribution guidelines

---

**Last Updated**: 2025-11-01  
**Maintained by**: RazorFS Development Team  
**Questions?**: https://github.com/ncandio/razorfs/issues
