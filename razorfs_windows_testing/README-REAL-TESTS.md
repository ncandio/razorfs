# RAZOR Filesystem Comprehensive Testing Suite

This directory contains enhanced testing scripts for comparing RAZOR filesystem against traditional filesystems.

## New Test Scripts

### 1. Real Filesystem Comparison (`real_filesystem_comparison.sh`)
- Tests RAZOR against real implementations of EXT4, BTRFS, ReiserFS, XFS, and SquashFS
- Measures actual read/write speeds with real filesystems
- Evaluates compression efficiency with real data
- Provides accurate performance metrics

### 2. Crash Recovery Test (`crash_recovery_test.sh`)
- Simulates unclean unmount scenarios for each filesystem
- Tests data integrity after forced shutdowns
- Measures recovery success rates
- Evaluates filesystem resilience

### 3. Combined Test Suite (`combined_filesystem_test.sh`)
- Runs all tests in sequence
- Generates comprehensive graphs and reports
- Provides final summary with recommendations

## Running the Tests

### Quick Start
```bash
# Build the testing environment
docker-compose -f docker-compose-stress.yml build

# Run the complete test suite
docker-compose -f docker-compose-stress.yml run --rm razorfs-stress combined-test

# Or run with full graph generation
docker-compose -f docker-compose-stress.yml run --rm razorfs-stress full-combined
```

### Individual Tests
```bash
# Run real filesystem comparison only
docker-compose -f docker-compose-stress.yml run --rm razorfs-stress real-filesystem-test

# Run crash recovery test only
docker-compose -f docker-compose-stress.yml run --rm razorfs-stress crash-recovery-test

# Generate graphs from existing data
docker-compose -f docker-compose-stress.yml run --rm razorfs-stress real-filesystem-graphs
```

## What Gets Tested

### Performance Metrics
- **Read/Write Speed**: MB/s for different file sizes (1MB, 10MB, 100MB)
- **Compression Efficiency**: Space savings percentage
- **Memory Usage**: During filesystem operations

### Crash Recovery
- **Recovery Status**: FULL, PARTIAL, or FAILED
- **Data Integrity**: Files and directories preserved
- **Mount Success**: Ability to remount after crash

### Filesystem Comparison
- **RAZOR**: Our novel filesystem implementation
- **EXT4**: Traditional Linux filesystem (baseline)
- **BTRFS**: Modern filesystem with built-in compression
- **ReiserFS**: Legacy filesystem with tail packing
- **XFS**: High-performance journaling filesystem
- **SquashFS**: Read-only compressed filesystem

## Generated Results

### CSV Data Files
- `real_compression_results.csv`: Compression metrics
- `real_performance_results.csv`: Read/write speed data
- `crash_recovery_results.csv`: Recovery test results

### PNG Charts
- `real_compression_analysis.png`: Compression efficiency comparison
- `real_performance_analysis.png`: Read/write speed comparison
- `crash_recovery_analysis.png`: Recovery success visualization
- `executive_filesystem_dashboard.png`: Comprehensive dashboard

### Summary Reports
- `real_filesystem_summary.txt`: Performance test summary
- `crash_recovery_summary.txt`: Recovery test summary
- `final_test_report.txt`: Overall test findings and recommendations

## Interpreting Results

### Key Performance Indicators
1. **Higher compression %** = Better space efficiency
2. **Higher MB/s** = Better performance
3. **FULL recovery status** = Better data protection
4. **Lower variance** = More consistent performance

### Trade-offs to Consider
- **Compression vs Speed**: More compression often means slower writes
- **Reliability vs Performance**: Journaling adds overhead but improves recovery
- **Memory Usage**: Some filesystems require more RAM for operations

## System Requirements

- Docker withCompose
- 2GB+ free disk space
- 1GB+ available RAM
- Linux host (for FUSE support)

Note: Some tests require privileged container access for mounting filesystems.