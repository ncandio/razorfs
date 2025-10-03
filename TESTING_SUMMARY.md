# RAZORFS Testing Infrastructure - Complete Setup

## 📋 What Was Created

### Testing Infrastructure (`/testing/`)

1. **Dockerfile** - Docker environment with all filesystems and tools
2. **benchmark.sh** - Comprehensive benchmark suite
3. **visualize.gnuplot** - Graph generation scripts
4. **run-tests.sh** - Master test runner
5. **sync-to-windows.sh** - Windows Desktop sync
6. **README.md** - Quick reference
7. **TESTING_GUIDE.md** - Detailed documentation

## 🚀 Usage

### One-Command Execution
```bash
cd /home/nico/WORK_ROOT/RAZOR_repo
./testing/run-tests.sh
```

### What It Does
1. ✅ Builds Docker image with RAZORFS + ext4/reiserfs/btrfs
2. ✅ Runs 4 benchmark categories
3. ✅ Generates comparison graphs
4. ✅ Syncs to `C:\Users\liber\Desktop\Testing-Razor-FS`

## 📊 Benchmark Categories

| Category | What | Metric | RAZORFS Feature |
|----------|------|--------|-----------------|
| **Metadata** | Create/stat/delete 1000 files | ms | O(log n) tree |
| **Scalability** | Lookup @ 10-1000 files | μs | Logarithmic growth |
| **Compression** | Compressible data | Ratio | Transparent zlib |
| **I/O** | Sequential read/write 10MB | MB/s | Cache-friendly |

## 📁 Output Structure

### WSL
```
/tmp/razorfs-results/
├── metadata_*.dat
├── ologn_*.dat
├── compression_*.dat
├── io_*.dat
└── razorfs_comparison.png
```

### Windows
```
C:\Users\liber\Desktop\Testing-Razor-FS\
├── data/*.dat
├── graphs/*.png
└── results/*.txt
```

## 🎯 Key Metrics Visualized

### Graph Panels (2×2 layout)
1. **Metadata Operations** - Bar chart comparing create/stat/delete times
2. **O(log n) Scalability** - Line plot showing lookup time vs file count
3. **I/O Throughput** - Bar chart for read/write MB/s
4. **Feature Summary** - Normalized scores

## ✅ RAZORFS Features Tested

- ✅ **N-ary tree** (16-way branching)
- ✅ **O(log n) complexity**
- ✅ **Compression** (zlib level 1)
- ✅ **FUSE3** implementation
- ✅ **NUMA-aware** allocation
- ✅ **Cache-friendly** (64B aligned)
- ✅ **Persistent** (shared memory)
- ✅ **Multithreaded** (ext4-style locking)

## 🔧 Quick Commands

```bash
# Full test suite
./testing/run-tests.sh

# Sync results to Windows
./testing/sync-to-windows.sh

# View results
ls -lh /tmp/razorfs-results/
```

## 📝 Files Created

```
testing/
├── Dockerfile              # Test environment
├── benchmark.sh            # Benchmark suite
├── visualize.gnuplot       # Graphs
├── run-tests.sh           # Master runner
├── sync-to-windows.sh     # Windows sync
├── README.md              # Quick start
└── TESTING_GUIDE.md       # Full guide
```

## 🎓 Next Steps

1. Run `./testing/run-tests.sh`
2. Check graphs in Windows Desktop folder
3. Review raw data in `/tmp/razorfs-results`
4. Use graphs for documentation/presentations
