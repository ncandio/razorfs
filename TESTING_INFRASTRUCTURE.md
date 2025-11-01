# RazorFS Testing Infrastructure Overview

**Comprehensive Docker-based filesystem benchmarking and Windows synchronization**

---

## 🎯 Purpose

This infrastructure enables:

1. **Virtual Filesystem Testing** - Test RazorFS against ext4, btrfs, and ZFS in isolated Docker containers
2. **Reproducible Benchmarks** - Git commit-tagged graphs ensure traceability
3. **Cross-Platform Workflow** - Develop in WSL2, test on Docker, share via Windows
4. **Continuous Validation** - Automated testing on every commit

---

## 📐 Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    Development Cycle                        │
└─────────────────────────────────────────────────────────────┘
                          │
                          ▼
┌─────────────────────────────────────────────────────────────┐
│  WSL2: /home/nico/WORK_ROOT/razorfs                         │
│  • Native RazorFS development                               │
│  • FUSE3 mounting and testing                               │
│  • Docker benchmark orchestration                           │
└─────────────────────────────────────────────────────────────┘
                          │
                          ▼
┌─────────────────────────────────────────────────────────────┐
│  Docker Containers (Virtual Filesystem Testing)             │
│  ┌────────────┐  ┌────────────┐  ┌────────────┐           │
│  │   ext4     │  │   btrfs    │  │    ZFS     │           │
│  │ (baseline) │  │ (modern)   │  │(enterprise)│           │
│  └────────────┘  └────────────┘  └────────────┘           │
│  • Isolated environments                                    │
│  • Loop device filesystems                                  │
│  • Controlled benchmarking                                  │
└─────────────────────────────────────────────────────────────┘
                          │
                          ▼
┌─────────────────────────────────────────────────────────────┐
│  Benchmark Results & Graphs                                 │
│  • benchmarks/BENCHMARK_REPORT_*.md                         │
│  • benchmarks/data/*.dat (raw data)                         │
│  • benchmarks/graphs/*.png (comparisons)                    │
│  • readme_graphs/*.png (documentation, commit-tagged)       │
└─────────────────────────────────────────────────────────────┘
                          │
                          ▼
┌─────────────────────────────────────────────────────────────┐
│  Windows Desktop: C:\Users\liber\Desktop\Testing-Razor-FS   │
│  • Automatic synchronization via sync-windows.sh            │
│  • Easy access for sharing and presentation                 │
│  • Docker Desktop support for Windows-side testing          │
└─────────────────────────────────────────────────────────────┘
```

---

## 🛠️ Key Components

### 1. Benchmark Script
**File**: `tests/docker/benchmark_filesystems.sh`

**Tests**:
- ✅ Compression efficiency (zlib vs ZSTD vs LZ4)
- ✅ Recovery performance (crash recovery simulation)
- ✅ NUMA friendliness (memory locality)
- ✅ Persistence verification (data integrity)

**Filesystems Compared**:
- RazorFS (N-ary tree, NUMA-aware, zlib compression)
- ext4 (Linux standard, no compression)
- btrfs (Copy-on-write, ZSTD compression)
- ZFS (Enterprise, LZ4 compression)

### 2. Graph Generation
**File**: `generate_tagged_graphs.sh`

**Generates**:
1. `comprehensive_performance_radar.png` - 8-metric radar chart
2. `ologn_scaling_validation.png` - O(log n) complexity proof
3. `scalability_heatmap.png` - Performance heatmap
4. `compression_effectiveness.png` - Compression comparison
5. `memory_numa_analysis.png` - NUMA analysis

**Feature**: Each graph tagged with Git commit SHA (e.g., "Generated: 2025-11-01 [a1b2c3d]")

### 3. Windows Sync
**File**: `sync-windows.sh`

**Syncs**:
- Docker configuration files
- Benchmark results and data
- Generated graphs
- Execution logs
- Auto-generated README.md index

**Target**: `/mnt/c/Users/liber/Desktop/Testing-Razor-FS`

### 4. Docker Configuration
**File**: `Dockerfile`

**Includes**:
- Ubuntu 22.04 base
- FUSE3 + zlib dependencies
- Testing tools (gnuplot, python3, numpy, matplotlib)
- Pre-built RazorFS binary
- All benchmark scripts

---

## 📊 Testing Workflow

### Standard Development Workflow

```bash
# 1. Make code changes
vim src/nary_tree_mt.c

# 2. Build
make clean && make

# 3. Run quick test (RazorFS only)
./tests/shell/razorfs-only-benchmark.sh

# 4. Full Docker benchmark (all filesystems)
./tests/docker/benchmark_filesystems.sh

# 5. Generate commit-tagged graphs
./generate_tagged_graphs.sh

# 6. Sync to Windows
./sync-windows.sh

# 7. View results on Windows
# C:\Users\liber\Desktop\Testing-Razor-FS\
```

### Windows-Side Testing

```powershell
# Run from Windows PowerShell
cd C:\Users\liber\Desktop\Testing-Razor-FS

# Build Docker image
docker build -t razorfs-test -f docker\Dockerfile .

# Run benchmarks
docker run --privileged `
  -v ${PWD}\benchmarks:/app/benchmarks `
  razorfs-test ./tests/docker/benchmark_filesystems.sh

# View results
explorer.exe benchmarks\graphs
```

---

## 📁 File Structure

### WSL2 (Development)
```
/home/nico/WORK_ROOT/razorfs/
├── src/                              # RazorFS source
├── tests/
│   ├── docker/
│   │   └── benchmark_filesystems.sh  # Main benchmark script
│   └── shell/
│       └── razorfs-only-benchmark.sh # Quick test
├── benchmarks/                       # Generated results
│   ├── BENCHMARK_REPORT_*.md
│   ├── data/*.dat
│   └── graphs/*.png
├── readme_graphs/                    # Documentation graphs
├── Dockerfile                        # Docker test environment
├── generate_tagged_graphs.sh         # Graph generator
├── sync-windows.sh                   # Windows sync
├── WORKFLOW.md                       # Complete workflow guide
├── WINDOWS_QUICKSTART.md            # Windows testing guide
└── TESTING_INFRASTRUCTURE.md        # This file
```

### Windows (Shared Results)
```
C:\Users\liber\Desktop\Testing-Razor-FS\
├── docker/
│   ├── Dockerfile
│   └── benchmark_filesystems.sh
├── benchmarks/
│   ├── BENCHMARK_REPORT_*.md
│   ├── data/
│   └── graphs/
├── readme_graphs/
├── logs/
├── README.md                         # Auto-generated index
└── WORKFLOW.md                       # Synced workflow guide
```

---

## 🎯 Use Cases

### Use Case 1: Daily Development
**Goal**: Quick performance validation during coding

**Steps**:
1. Make changes
2. Build: `make clean && make`
3. Quick test: `./tests/shell/razorfs-only-benchmark.sh`
4. Review output

**Time**: 2-5 minutes

### Use Case 2: Pre-Commit Validation
**Goal**: Ensure no regressions before committing

**Steps**:
1. Full benchmark: `./tests/docker/benchmark_filesystems.sh`
2. Generate graphs: `./generate_tagged_graphs.sh`
3. Review comparison vs ext4/btrfs/ZFS
4. Commit if performance acceptable

**Time**: 15-20 minutes

### Use Case 3: Release Preparation
**Goal**: Comprehensive testing before release

**Steps**:
1. Full benchmark suite
2. Generate all graphs with commit tags
3. Sync to Windows for documentation
4. Review all metrics
5. Update README with latest graphs

**Time**: 30-45 minutes

### Use Case 4: Performance Regression Testing
**Goal**: Compare performance between commits

**Steps**:
1. Checkout baseline commit
2. Run benchmarks: `./tests/docker/benchmark_filesystems.sh`
3. Save results: `cp benchmarks/BENCHMARK_REPORT_*.md benchmarks/versioned_results/baseline.md`
4. Checkout new commit
5. Run benchmarks again
6. Compare reports side-by-side

**Time**: 30-40 minutes

---

## 🔧 Configuration

### Benchmark Configuration
Edit `tests/docker/benchmark_filesystems.sh`:

```bash
# Test file (change to test different data)
TEST_FILE_URL="https://github.com/git/git/archive/refs/tags/v2.43.0.tar.gz"

# Output location
RESULTS_DIR="$REPO_ROOT/benchmarks"

# Filesystems to test (comment out to skip)
test_compression "RAZORFS" ...
test_compression "ext4" ...
test_compression "btrfs" ...
test_compression "ZFS" ...
```

### Graph Styling
Edit `generate_tagged_graphs.sh`:

```bash
# Graph dimensions
set terminal pngcairo enhanced font 'Arial Bold,14' size 1200,900

# Colors
set style line 1 lc rgb '#3498db' lt 1 lw 3

# Commit tag
TAG_TEXT="Generated: $COMMIT_DATE [$COMMIT_SHA]"
```

### Windows Sync Path
Edit `sync-windows.sh`:

```bash
WINDOWS_SYNC_DIR="/mnt/c/Users/liber/Desktop/Testing-Razor-FS"
```

---

## 📚 Documentation Files

| File | Purpose |
|------|---------|
| `WORKFLOW.md` | Complete Docker workflow guide |
| `WINDOWS_QUICKSTART.md` | Windows testing quick start |
| `TESTING_INFRASTRUCTURE.md` | This file - infrastructure overview |
| `README.md` | Main project documentation |
| `docs/TESTING.md` | Comprehensive testing guide |
| `docs/development/DOCKER_WORKFLOW.md` | Detailed Docker documentation |

---

## 🚀 Getting Started

### First Time Setup

```bash
# 1. Clone repository
cd /home/nico/WORK_ROOT
git clone https://github.com/ncandio/razorfs.git
cd razorfs

# 2. Install dependencies
sudo apt-get update
sudo apt-get install -y \
    build-essential libfuse3-dev zlib1g-dev \
    gnuplot bc wget docker.io

# 3. Build RazorFS
make clean && make

# 4. Create Windows directory
mkdir -p /mnt/c/Users/liber/Desktop/Testing-Razor-FS

# 5. Run first benchmark
./tests/docker/benchmark_filesystems.sh

# 6. Generate graphs
./generate_tagged_graphs.sh

# 7. Sync to Windows
./sync-windows.sh

# 8. View results
# Open: C:\Users\liber\Desktop\Testing-Razor-FS\
```

---

## 🎓 Best Practices

1. **✅ Tag everything** - Commit SHA in graphs ensures traceability
2. **✅ Version results** - Keep old benchmarks in `versioned_results/`
3. **✅ Document changes** - Update CHANGELOG with performance impacts
4. **✅ Test before commits** - Catch regressions early
5. **✅ Clean Docker cache** - Run `docker system prune -a` periodically
6. **✅ Sync regularly** - Keep Windows directory updated for easy sharing

---

## 🤝 Contributing

To contribute benchmark improvements:

1. Fork the repository
2. Create feature branch
3. Modify benchmarks or add new tests
4. Run full benchmark suite
5. Generate graphs with commit tag
6. Submit PR with benchmark results

---

**Last Updated**: 2025-11-01  
**Maintained by**: RazorFS Development Team  
**Questions?**: https://github.com/ncandio/razorfs/issues
