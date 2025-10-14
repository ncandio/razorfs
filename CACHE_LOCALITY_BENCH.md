# RazorFS Cache Locality Docker Workflow

This Docker workflow allows you to run comprehensive cache locality benchmarks comparing RazorFS (FUSE3-based) and ext4 filesystems.

## Architecture

The workflow includes:
- Docker container with all necessary dependencies
- Mount points for both RazorFS and ext4
- Benchmarking scripts to measure cache locality
- Plotting tools (gnuplot and Python with numpy/matplotlib)
- Results generation and analysis

## Prerequisites

- Docker installed
- Docker Compose installed
- Privileged access for FUSE mounting

## Usage

### Build and Run with Docker Compose (Recommended)

```bash
# Build and run the benchmark in one command
docker-compose up --build

# Or build first, then run
docker-compose build
docker-compose up
```

### Manual Docker Commands

```bash
# Build the image
docker build -t razorfs-cache-bench .

# Run the benchmark container
docker run --privileged \
  -v $(pwd)/benchmarks/results:/app/benchmarks/results \
  -v /tmp:/tmp \
  razorfs-cache-bench
```

## What the Workflow Does

1. Builds RazorFS from source inside the container
2. Mounts RazorFS using FUSE3 at `/tmp/razorfs_cache_test`
3. Uses ext4 at `/tmp/ext4_cache_test` as comparison baseline
4. Runs multiple cache locality benchmarks:
   - Directory traversal (sequential access)
   - File reads (sequential access)
   - Random access patterns
   - Detailed cache performance analysis using `perf`

5. Generates plots using:
   - gnuplot for command-line visualization
   - Python with numpy/matplotlib for advanced visualization
   - CSV data files for further analysis

## Output Files

The benchmark generates several output files in `./benchmarks/results/`:

### Data Files
- `cache_locality_benchmark.csv` - Main benchmark results
- `cache_miss_analysis.csv` - Cache miss rate data
- `cache_locality_summary.txt` - Summary analysis

### Plots (gnuplot)
- `cache_locality_comparison.png` - Performance comparison
- `cache_miss_analysis.png` - Cache miss rate comparison

### Plots (Python/numpy/matplotlib)
- `cache_locality_comparison_matplotlib.png` - Matplotlib bar chart
- `cache_miss_comparison_matplotlib.png` - Matplotlib cache miss plot
- `cache_locality_heatmap.png` - Performance heatmap
- `cache_locality_python_report.txt` - Detailed Python analysis

### Additional Analysis
- `memory_numa_analysis.png` - Memory and NUMA performance characteristics visualization in the `readme_graphs/` directory

## Key Features

### Cache Locality Metrics
- Sequential access performance
- Random access performance
- Cache miss rates
- Time-based comparisons

### Architectural Considerations
- **RazorFS (FUSE3)**: 
  - 64-byte aligned nodes
  - 16-way branching (O(log₁₆ n) complexity)
  - NUMA-aware memory allocation
  - Spatial and temporal locality optimizations

- **ext4 (Baseline)**:
  - Standard Linux filesystem
  - Well-optimized for general use cases
  - Serves as performance reference

## Legend and Annotations

All plots include proper legends identifying:
- "razorfs (FUSE3)" - The RazorFS filesystem mounted via FUSE3
- "ext4" - The standard Linux ext4 filesystem as baseline
- Operation types: directory traversal, file reads, random access
- Performance metrics: time, cache miss rates

## Results Interpretation

- **Lower times** indicate better cache locality
- **Lower cache miss rates** indicate better cache utilization
- Performance ratios show relative advantages between filesystems
- Sequential access patterns benefit from spatial locality
- Random access patterns test temporal locality

## Notes

- The workflow runs in a container environment which may have slightly different performance characteristics than a native setup
- FUSE3 adds some overhead but provides the flexibility to implement advanced filesystem features
- Results may vary based on system hardware and configuration
- For production evaluation, consider running benchmarks on the target system