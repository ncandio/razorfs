# RAZOR Filesystem - Comprehensive Stress Testing

Complete stress testing suite for RAZOR filesystem with performance graph generation.

## Quick Start

### 1. Copy to Windows (if not done already)
```bash
# From WSL/Linux:
cp -r /path/to/RAZOR_repo /mnt/c/Users/liber/Desktop/razorfs-testing
```

### 2. Navigate to Testing Directory
```powershell
cd C:\Users\liber\Desktop\razorfs-testing\razorfs_windows_testing
```

### 3. Run Complete Stress Test
```powershell
# Build stress testing image and run full test
.\stress-test.bat build
.\stress-test.bat full
```

## Available Commands

### Build Stress Testing Environment
```powershell
.\stress-test.bat build
```

### Run Individual Tests
```powershell
# Run stress tests only (no graphs)
.\stress-test.bat stress

# Generate graphs from existing results
.\stress-test.bat graphs

# Copy results to Windows
.\stress-test.bat results
```

### Complete Workflow
```powershell
# Run everything: stress test + graph generation + copy results
.\stress-test.bat full
```

## What Gets Tested

### 1. **File Operations Performance**
- Create/Read/Delete operations
- 100, 500, 1000, 2000 files
- Throughput measurements (files/second)

### 2. **I/O Performance Benchmarks (FIO)**
- Different block sizes: 4K, 64K, 1MB
- Operations: read, write, randread, randwrite
- IOPS, Bandwidth, Latency measurements

### 3. **Directory Operations**
- Directory creation, listing, deletion
- 50, 100, 200, 500 directories
- Operations per second metrics

### 4. **Large File Performance**
- File sizes: 10MB, 50MB, 100MB, 200MB
- Read/Write throughput (MB/second)
- Time-based performance analysis

### 5. **Concurrent Access Testing**
- 2, 4, 8 concurrent processes
- Scalability analysis
- Total throughput measurements

### 6. **Memory Usage Monitoring**
- Memory consumption during operations
- Memory utilization percentages
- Resource usage tracking

## Generated PNG Charts

After running `.\stress-test.bat full`, you'll get these performance graphs:

### Individual Performance Charts
- `file_operations_performance.png` - File create/read/delete performance
- `fio_performance_benchmarks.png` - I/O performance with FIO
- `directory_operations_performance.png` - Directory operation metrics  
- `large_file_performance.png` - Large file throughput analysis
- `concurrent_access_performance.png` - Concurrent access scalability
- `memory_usage_monitoring.png` - Memory consumption tracking

### Summary Dashboard
- `performance_summary_dashboard.png` - Comprehensive overview with metrics table

## Results Location

After running tests, results are copied to:
```
C:\Users\liber\Desktop\razorfs-testing\razorfs_windows_testing\results\
```

Contains:
- **PNG files** - Performance graphs
- **CSV files** - Raw performance data  
- **system_info.txt** - System configuration details

## Test Duration

- **Stress Test Only**: ~10-15 minutes
- **Full Test (with graphs)**: ~15-25 minutes
- **Graph Generation**: ~2-3 minutes

## System Requirements

### For Basic Testing
- Docker Desktop with WSL2
- 4GB RAM minimum
- 2GB free disk space

### For Comprehensive Testing
- 8GB RAM recommended
- 4GB free disk space
- Multi-core CPU (better concurrent testing)

## Understanding the Results

### Performance Metrics
- **IOPS**: Input/Output Operations Per Second
- **Throughput**: MB/second or Files/second
- **Latency**: Response time in milliseconds
- **Scalability**: Performance per concurrent process

### Typical Expectations
- **File Operations**: 100-1000 files/second (varies by operation)
- **I/O Performance**: Depends on storage backend
- **Large Files**: Limited by Docker I/O overhead
- **Concurrent Access**: Should scale reasonably with processes

### Red Flags to Watch For
- Drastically decreasing performance with file count
- Poor concurrent access scalability
- High memory usage growth
- Excessive latency spikes

## Troubleshooting

### Build Issues
```powershell
.\stress-test.bat clean
.\stress-test.bat build
```

### Results Not Generated
```powershell
# Check container logs
docker logs razorfs-stress-container

# Manual results copy
.\stress-test.bat results
```

### Performance Issues
- Close unnecessary applications
- Ensure Docker has sufficient resources
- Check available disk space

## Comparing with Other Filesystems

The stress test framework can be extended to test other filesystems:

1. Mount comparison filesystem (ext4, NTFS, etc.)
2. Run same test suite
3. Compare generated graphs
4. Analyze performance differences

## Advanced Usage

### Manual Testing
```powershell
# Start stress testing container
docker-compose -f docker-compose-stress.yml run --rm razorfs-stress

# Inside container:
/razorfs/razorfs_windows_testing/stress_test_suite.sh
python3 /razorfs/razorfs_windows_testing/generate_graphs.py
```

### Custom Test Parameters
Edit `stress_test_suite.sh` to modify:
- Number of files tested
- File sizes
- Concurrent process counts
- Test duration

## Results Analysis

The generated graphs provide insights into:

1. **Filesystem Bottlenecks** - Where performance drops
2. **Scalability Characteristics** - How it handles load
3. **Resource Usage** - Memory and I/O efficiency
4. **Operational Characteristics** - Best use cases

Use these results to:
- Optimize RAZOR filesystem parameters
- Compare against other filesystems
- Identify performance regression
- Plan deployment strategies


## Real Filesystem Testing

For enhanced testing against real filesystem implementations, see [README-REAL-TESTS.md](README-REAL-TESTS.md)
