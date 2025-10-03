# RAZORFS Windows Testing Guide

## Quick Start (Windows)

1. Open Command Prompt or PowerShell
2. Navigate to `C:\Users\liber\Desktop\Testing-Razor-FS`
3. Copy RAZORFS repository contents to this folder
4. Run: `run-tests-windows.bat`

## What It Does

The Windows test runner executes the complete RAZORFS benchmark suite in Docker and outputs results directly to your Desktop folders:

```
C:\Users\liber\Desktop\Testing-Razor-FS\
├── results\      # Summary reports
├── charts\       # PNG comparison graphs
└── data\         # Raw benchmark data (.dat files)
```

## Prerequisites

- **Docker Desktop for Windows** with WSL2 backend
- **Privileged container access** (for loop device mounting)
- **RAZORFS source code** in this directory

## Test Categories

### 1. Metadata Performance
- File creation (1000 files)
- Stat operations (1000 stats)
- File deletion (1000 deletes)
- **Output:** `data/metadata_*.dat`

### 2. O(log n) Scalability
- Lookup tests at scales: 10, 50, 100, 500, 1000 files
- Validates logarithmic complexity
- **Output:** `data/ologn_*.dat`

### 3. I/O Throughput
- Sequential write (10MB)
- Sequential read (10MB)
- **Output:** `data/io_*.dat`

### 4. Visual Comparison
- 4-panel comparison graph
- **Output:** `charts/razorfs_comparison.png`

## Filesystems Compared

- **ext4** - General-purpose Linux filesystem
- **reiserfs** - Metadata-optimized filesystem
- **btrfs** - Modern filesystem with built-in features
- **RAZORFS** - N-ary tree with O(log n), compression, NUMA

## Expected Results

RAZORFS should demonstrate:
- ✅ Consistent O(log n) scalability (flat line in panel 2)
- ✅ Competitive metadata performance (panel 1)
- ✅ Transparent compression efficiency
- ✅ Cache-friendly design benefits
- ✅ NUMA-aware memory allocation

## Troubleshooting

**Docker build fails:**
```batch
docker build -t razorfs-test -f Dockerfile .
```

**Privileged mode issues:**
Ensure Docker Desktop has privileged container access enabled.

**Missing output:**
Check that directories exist:
```batch
dir results
dir charts
dir data
```

## Manual Steps

### Build Only
```batch
docker build -t razorfs-test -f Dockerfile .
```

### Run Benchmarks Only
```batch
docker run --rm --privileged -v %cd%:/testing -v %cd%\data:/data razorfs-test bash /testing/benchmark-windows.sh
```

### Generate Graphs Only
```batch
docker run --rm -v %cd%:/testing -v %cd%\data:/data -v %cd%\charts:/charts razorfs-test gnuplot /testing/visualize-windows.gnuplot
```

## Output Files

**Data Files (data/):**
- `metadata_ext4.dat` - ext4 metadata benchmarks
- `metadata_reiserfs.dat` - reiserfs metadata benchmarks
- `metadata_btrfs.dat` - btrfs metadata benchmarks
- `metadata_razorfs.dat` - RAZORFS metadata benchmarks
- `ologn_*.dat` - Scalability test data
- `io_*.dat` - I/O throughput data

**Charts (charts/):**
- `razorfs_comparison.png` - 4-panel comparison visualization

**Results (results/):**
- `summary.txt` - Test execution summary

## Integration with WSL

If you also have WSL access, you can sync results:
```bash
# From WSL
cd /home/nico/WORK_ROOT/RAZOR_repo
./testing/sync-to-windows.sh
```

This creates a bidirectional testing workflow.
