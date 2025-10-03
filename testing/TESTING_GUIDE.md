# RAZORFS Testing Guide

## Overview

Complete testing infrastructure for RAZORFS, comparing performance against ext4, reiserfs, and btrfs filesystems.

## Prerequisites

- **WSL2** with Docker Desktop
- **Windows path:** `C:\Users\liber\Desktop\Testing-Razor-FS`
- **Privileged Docker access** for loop device mounting

## Quick Start

```bash
cd /home/nico/WORK_ROOT/RAZOR_repo
./testing/run-tests.sh
```

This will:
1. Build Docker test image
2. Run comprehensive benchmarks
3. Generate comparison graphs
4. Sync results to Windows Desktop

## Test Suite Components

### 1. Metadata Performance
**What:** File creation, stat operations, deletion
**Metric:** Milliseconds for 1000 operations
**Focus:** RAZORFS O(log n) tree efficiency

### 2. Scalability (O(log n) Validation)
**What:** Lookup operations at different scales (10→1000 files)
**Metric:** Microseconds per operation
**Focus:** Logarithmic vs linear growth

### 3. Compression Efficiency
**What:** Compressible data storage
**Metric:** Compression ratio, storage savings
**Focus:** RAZORFS transparent zlib compression

### 4. I/O Throughput
**What:** Sequential read/write
**Metric:** MB/s
**Focus:** Real-world data transfer speed

## Output Locations

**WSL/Linux:**
```
/tmp/razorfs-results/
├── metadata_*.dat       # Raw metadata benchmarks
├── ologn_*.dat         # Scalability data
├── compression_*.dat   # Compression stats
├── io_*.dat           # I/O throughput
└── razorfs_comparison.png  # Comparison graphs
```

**Windows Desktop:**
```
C:\Users\liber\Desktop\Testing-Razor-FS\
├── data/         # Raw .dat files
├── graphs/       # PNG visualizations
└── results/      # Summary reports
```

## Manual Execution

### Build Only
```bash
docker build -t razorfs-test -f testing/Dockerfile .
```

### Run Benchmarks Only
```bash
docker run --rm --privileged \
    -v $(pwd)/testing:/testing \
    -v /tmp/razorfs-results:/results \
    razorfs-test bash /testing/benchmark.sh
```

### Generate Graphs Only
```bash
docker run --rm \
    -v /tmp/razorfs-results:/results \
    razorfs-test gnuplot /testing/visualize.gnuplot
```

### Sync to Windows Only
```bash
./testing/sync-to-windows.sh
```

## Expected Results

### RAZORFS Strengths
- ✅ **O(log n) scalability** - Constant performance with file count
- ✅ **Fast metadata ops** - Efficient n-ary tree lookups
- ✅ **Automatic compression** - Transparent zlib (level 1)
- ✅ **Cache-friendly** - 64-byte aligned nodes
- ✅ **NUMA-aware** - Memory locality optimization

### Comparison Baseline
- **ext4:** General-purpose, mature
- **reiserfs:** Metadata optimized
- **btrfs:** Modern with compression

## Troubleshooting

**Docker won't start filesystems:**
```bash
# Run with --privileged flag
docker run --rm --privileged ...
```

**Windows sync fails:**
```bash
# Check Windows path exists
ls /mnt/c/Users/liber/Desktop/Testing-Razor-FS
```

**Missing results:**
```bash
# Check Docker volumes
docker run --rm razorfs-test ls -la /results
```

## Graph Interpretation

The generated `razorfs_comparison.png` contains 4 panels:

1. **Top-Left:** Metadata operations (lower is better)
2. **Top-Right:** O(log n) scalability (flatter is better)
3. **Bottom-Left:** I/O throughput (higher is better)
4. **Bottom-Right:** Feature score summary

## Next Steps

After running tests:
1. Review `/tmp/razorfs-results/razorfs_comparison.png`
2. Check raw data in `*.dat` files
3. Results automatically synced to Windows Desktop
4. Use graphs for documentation/presentations
