# RAZORFS Filesystem Benchmarking Suite

Comprehensive benchmark comparing RAZORFS against ext4, ReiserFS, and ZFS.

## Features

The benchmark suite tests 4 key areas:

1. **Compression Efficiency** - How well each filesystem compresses real-world data
2. **Backup & Recovery** - 10-second crash recovery simulation with data integrity
3. **NUMA Friendliness** - Memory locality and access latency on NUMA systems
4. **Persistence** - Data persistence verification across mount/unmount cycles

## Requirements

### Linux (WSL)
```bash
sudo apt-get install -y \
    gnuplot \
    bc \
    wget \
    fuse3 \
    docker.io
```

### Docker for Windows
- Docker Desktop must be running
- WSL2 integration enabled

## Running the Benchmarks

```bash
cd /home/nico/WORK_ROOT/RAZOR_repo
./benchmark_filesystems.sh
```

The script will:
1. Download a 1MB+ test file (git source archive)
2. Build RAZORFS if needed
3. Run all 4 benchmark tests
4. Generate gnuplot graphs
5. Create a comprehensive markdown report
6. Save results to `C:\Users\liber\Desktop\Testing-Razor-FS\benchmarks\`

## Output

Results are saved to: `C:\Users\liber\Desktop\Testing-Razor-FS\benchmarks/`

```
benchmarks/
├── BENCHMARK_REPORT_YYYYMMDD_HHMMSS.md  # Full report
├── data/                                 # Raw test data
│   ├── compression_TIMESTAMP.dat
│   ├── recovery_TIMESTAMP.dat
│   ├── numa_TIMESTAMP.dat
│   └── persistence_TIMESTAMP.dat
├── graphs/                               # Generated graphs
│   ├── compression_comparison.png
│   ├── recovery_comparison.png
│   └── numa_comparison.png
└── *.gp                                  # Gnuplot scripts
```

## Benchmark Details

### Test 1: Compression Efficiency

**Method:**
- Downloads git-2.43.0.tar.gz (~1MB compressed archive)
- Stores file on each filesystem
- Measures actual disk usage
- Calculates compression ratio

**Filesystems:**
- **RAZORFS:** Native zlib compression
- **ext4:** No compression (baseline)
- **ZFS:** LZ4 compression
- **ReiserFS:** No compression

**Metrics:**
- Disk usage (MB)
- Compression ratio (original size / disk usage)

### Test 2: Backup & Recovery

**Method:**
- Mount filesystem
- Write test data
- Simulate crash (unmount)
- Measure recovery time
- Verify data integrity

**10-Second Simulation:**
The test simulates a crash that occurs 10 seconds into filesystem operation, then measures:
- Time to complete recovery
- Percentage of data successfully recovered

**RAZORFS Advantage:**
- Shared memory persistence = instant recovery
- No fsck required
- 100% data integrity

### Test 3: NUMA Friendliness

**Method:**
Measures memory locality optimization on NUMA (Non-Uniform Memory Access) systems.

**NUMA Score Criteria (0-100):**
- Memory allocation on local NUMA node
- Cache-friendly access patterns
- Minimized cross-node memory access

**Access Latency:**
- Measured in nanoseconds
- Lower = better performance

**RAZORFS NUMA Features:**
- Uses `numa_bind_memory()` for optimal placement
- Thread-local data structures
- Shared memory on local NUMA node

### Test 4: Persistence Verification

**Method:**
1. Create 1MB file with random data
2. Calculate MD5 checksum
3. Unmount filesystem
4. Remount filesystem
5. Verify MD5 checksum matches

**Test File:**
- Size: 1MB
- Content: Random data (`/dev/urandom`)
- Hash: MD5 checksum

**Pass Criteria:**
- ✅ Checksums match → Data persisted correctly
- ❌ Checksums differ → Data corruption or loss

## Gnuplot Graph Descriptions

### 1. Compression Comparison
- **X-axis:** Filesystem name
- **Y-axis (left):** Disk usage in MB (blue bars)
- **Y-axis (right):** Compression ratio (red bars)
- **Legend:** Lower disk usage = better compression

### 2. Recovery Comparison
- **X-axis:** Filesystem name
- **Y-axis (left):** Recovery time in milliseconds (purple bars)
- **Y-axis (right):** Success rate % (green bars)
- **Legend:** Lower recovery time + higher success rate = better

### 3. NUMA Comparison
- **X-axis:** Filesystem name
- **Y-axis (left):** NUMA score 0-100 (orange bars)
- **Y-axis (right):** Access latency in ns (red bars)
- **Legend:** Higher NUMA score + lower latency = better

## Understanding the Results

### RAZORFS Expected Performance

| Metric | Expected Value | Explanation |
|--------|---------------|-------------|
| Compression Ratio | 1.5-3.0x | zlib compression on compressible data |
| Recovery Time | <500ms | Shared memory = instant recovery |
| NUMA Score | 90-95 | Native NUMA support |
| Persistence | 100% | Zero-copy shared memory persistence |

### Comparison Matrix

| Feature | RAZORFS | ext4 | ZFS | ReiserFS |
|---------|---------|------|-----|----------|
| **Compression** | ✅ zlib | ❌ None | ✅ LZ4/ZSTD | ❌ None |
| **NUMA-aware** | ✅ Yes | ❌ No | ⚠️  Partial | ❌ No |
| **Recovery** | ✅ Instant | ⚠️  Slow | ⚠️  Slow | ⚠️  Slow |
| **Persistence** | ✅ Shared Mem | ✅ Disk | ✅ Disk | ✅ Disk |
| **Use Case** | Memory FS | General | Enterprise | Legacy |

## Troubleshooting

### Docker Issues

If Docker tests fail:
```bash
# Check Docker is running
docker ps

# Ensure WSL2 integration
# Docker Desktop → Settings → Resources → WSL Integration → Enable your distro
```

### Permission Issues

```bash
# Add user to docker group
sudo usermod -aG docker $USER
newgrp docker
```

### Gnuplot Not Found

```bash
sudo apt-get install gnuplot

# Or on Windows, install: http://www.gnuplot.info/
```

### RAZORFS Build Fails

```bash
cd /home/nico/WORK_ROOT/RAZOR_repo
make clean
make release
```

## Custom Test Files

To test with your own file instead of the git archive:

```bash
# Edit benchmark_filesystems.sh
TEST_FILE_URL="https://your-url.com/your-file.tar.gz"
TEST_FILE_NAME="your-file.tar.gz"
```

## Viewing Results on Windows

The script automatically opens the report in your default markdown viewer on Windows.

Manually view:
```
C:\Users\liber\Desktop\Testing-Razor-FS\benchmarks\BENCHMARK_REPORT_*.md
```

View graphs:
```
C:\Users\liber\Desktop\Testing-Razor-FS\benchmarks\graphs\*.png
```

## Re-running Tests

Each test run creates timestamped files, so you can:
- Run multiple times
- Compare results over time
- Track performance improvements

## Data Format

All data files use space-separated format compatible with gnuplot:

```
# Column1 Column2 Column3
RAZORFS 5.2 2.5
ext4 10.0 1.0
ZFS 8.5 1.5
```

You can import this data into Excel, Python, R, or any analysis tool.

## Contributing

To add more tests or filesystems, edit `benchmark_filesystems.sh` and follow the existing test patterns.
