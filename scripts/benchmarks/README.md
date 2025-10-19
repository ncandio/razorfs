# Benchmark Scripts

This directory contains performance benchmarking scripts for RAZORFS.

## Available Benchmarks

### Core Benchmarks

- **`run_benchmark_suite.sh`** - Complete performance test suite
  - Metadata operations (create/stat/delete)
  - I/O throughput (read/write)
  - Scalability testing (O(log n) validation)
  - Compression efficiency

- **`run_cache_bench.sh`** - Cache locality benchmarks
  - Cache hit ratio measurements
  - Memory access patterns
  - BFS layout validation

- **`cache_locality_plots.py`** - Generate performance graphs
  - Requires: matplotlib, numpy
  - Output: PNG graphs in benchmarks/graphs/

### Docker Benchmarks

See [scripts/docker/](../docker/) for Docker-based benchmarks comparing RAZORFS against ext4, btrfs, and reiserfs.

## Usage

### Run Full Benchmark Suite

```bash
cd scripts/benchmarks
./run_benchmark_suite.sh
```

Results are stored in `../../benchmarks/results/`

### Run Cache Benchmark

```bash
./run_cache_bench.sh
```

### Generate Performance Graphs

```bash
python3 cache_locality_plots.py
```

Graphs saved to `../../benchmarks/graphs/`

## Benchmark Results

All benchmark results are stored in:
- **CSV Data**: `../../benchmarks/results/`
- **Graphs**: `../../benchmarks/graphs/`
- **Reports**: `../../benchmarks/reports/`

## Requirements

- Linux with FUSE3
- Python 3.8+ (for plotting)
- gnuplot (optional, for enhanced graphs)
- bc (for calculations)

## Interpreting Results

### Metadata Performance
- **Create**: Time to create 1000 files
- **Stat**: Time to stat 1000 files
- **Delete**: Time to delete 1000 files

### I/O Throughput
- **Write**: Sequential write speed (MB/s)
- **Read**: Sequential read speed (MB/s)

### Scalability (O(log n))
- Should show logarithmic growth as file count increases
- Compare against theoretical O(log₁₆ n) curve

## Troubleshooting

**Permission Denied**: Run with `sudo` or ensure FUSE permissions
**Mount Failed**: Check if mountpoint exists and is empty
**Benchmark Slow**: Disable compression for pure I/O tests

## Contributing

When adding new benchmarks:
1. Follow existing script structure
2. Store results in `../../benchmarks/results/`
3. Update this README with usage instructions
4. Add to CI/CD pipeline if appropriate
