# RAZORFS Testing Infrastructure

Comprehensive benchmark suite comparing RAZORFS against ext4, reiserfs, and btrfs.

## Quick Start

```bash
# Run all tests (builds Docker, runs benchmarks, generates graphs, syncs to Windows)
chmod +x testing/run-tests.sh
./testing/run-tests.sh
```

## Test Categories

### 1. Metadata Operations
- File creation (1000 files)
- Stat operations (1000 stats)
- File deletion (1000 deletes)

**Metric:** Time in milliseconds

### 2. O(log n) Scalability
- Tests at scales: 10, 50, 100, 500, 1000 files
- Measures lookup time per operation

**Metric:** Microseconds per lookup

### 3. Compression
- Compressible data (repetitive text)
- Measures compression ratio and overhead

**Metric:** Compression ratio, storage efficiency

### 4. I/O Throughput
- Sequential write (10MB)
- Sequential read (10MB)

**Metric:** MB/s

## Architecture

```
testing/
├── Dockerfile           # Test environment setup
├── benchmark.sh         # Main benchmark suite
├── visualize.gnuplot    # Graph generation
├── run-tests.sh         # Master runner
└── sync-to-windows.sh   # Windows sync script
```

## Output

Results are saved to:
- **WSL:** `/tmp/razorfs-results/`
- **Windows:** `C:\Users\liber\Desktop\Testing-Razor-FS\`

### Generated Files
- `*.dat` - Raw benchmark data
- `razorfs_comparison.png` - Comparison graphs
- `*.txt` - Summary reports

## Key Features Tested

✅ **O(log n) Complexity** - Logarithmic scalability
✅ **Metadata Performance** - Fast file operations
✅ **Compression** - Transparent zlib compression
✅ **Cache-Friendly** - Aligned data structures
✅ **NUMA-Aware** - Memory locality
✅ **Persistence** - Shared memory storage

## Manual Testing

```bash
# Build test environment
docker build -t razorfs-test -f testing/Dockerfile .

# Run specific benchmark
docker run --rm --privileged razorfs-test bash /testing/benchmark.sh

# Generate graphs only
docker run --rm -v /tmp/razorfs-results:/results razorfs-test \
    gnuplot /testing/visualize.gnuplot
```

## Requirements

- Docker with WSL2 backend
- Privileged container access (for loop devices)
- gnuplot (included in container)
