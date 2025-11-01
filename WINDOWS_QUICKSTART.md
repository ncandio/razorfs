# RazorFS Windows Testing Quick Start

**Run Docker-based benchmarks from Windows using Docker Desktop**

---

## 📋 Prerequisites

1. **Docker Desktop for Windows** (latest version)
   - Download from: https://www.docker.com/products/docker-desktop
   - Ensure WSL2 backend is enabled

2. **Windows Terminal** (optional but recommended)
   - Install from Microsoft Store

3. **Test Directory**
   - Location: `C:\Users\liber\Desktop\Testing-Razor-FS`

---

## 🚀 Quick Start

### Option 1: Using Synced Files from WSL2

**Step 1**: Sync files from WSL2
```bash
# From WSL2 Ubuntu terminal
cd /home/nico/WORK_ROOT/razorfs
./sync-windows.sh
```

**Step 2**: Run benchmarks on Windows
```powershell
# From PowerShell (Run as Administrator)
cd C:\Users\liber\Desktop\Testing-Razor-FS

# Build Docker image
docker build -t razorfs-test -f docker\Dockerfile .

# Run benchmarks
docker run --privileged `
  -v ${PWD}\benchmarks:/app/benchmarks `
  -v ${PWD}\readme_graphs:/app/readme_graphs `
  razorfs-test `
  bash -c "cd /app && ./tests/docker/benchmark_filesystems.sh"
```

**Step 3**: View results
```powershell
explorer.exe benchmarks\graphs
explorer.exe readme_graphs
notepad.exe benchmarks\BENCHMARK_REPORT_*.md
```

---

### Option 2: Clone and Build on Windows

**Step 1**: Clone repository
```powershell
# From PowerShell
cd C:\Users\liber\Desktop
git clone https://github.com/ncandio/razorfs.git
cd razorfs
```

**Step 2**: Build and run
```powershell
# Build Docker image
docker build -t razorfs-test .

# Run benchmarks
docker run --privileged `
  -v ${PWD}\benchmarks:/app/benchmarks `
  razorfs-test `
  ./tests/docker/benchmark_filesystems.sh

# Generate graphs
docker run `
  -v ${PWD}\readme_graphs:/app/readme_graphs `
  razorfs-test `
  ./generate_tagged_graphs.sh
```

---

## 📊 Understanding Results

### Benchmark Report
Location: `benchmarks\BENCHMARK_REPORT_<timestamp>.md`

Contains:
- Compression efficiency comparison
- Recovery performance metrics
- NUMA friendliness scores
- Persistence verification results

### Performance Graphs
Location: `benchmarks\graphs\`

Generated graphs:
- `compression_comparison.png` - Disk usage and compression ratios
- `recovery_comparison.png` - Recovery time and success rates
- `numa_comparison.png` - NUMA scores and access latency
- `persistence_verification.png` - Data integrity verification

### README Graphs
Location: `readme_graphs\`

Professional graphs for documentation:
- `comprehensive_performance_radar.png` - 8-metric radar chart
- `ologn_scaling_validation.png` - O(log n) complexity proof
- `scalability_heatmap.png` - Cross-filesystem heatmap
- `compression_effectiveness.png` - Compression comparison
- `memory_numa_analysis.png` - NUMA performance analysis

Each graph is tagged with Git commit SHA for traceability.

---

## 🔄 Filesystem Comparison

Benchmarks compare RazorFS against:

| Filesystem | Description | Key Features |
|------------|-------------|--------------|
| **RazorFS** | Experimental N-ary tree FS | NUMA-aware, zlib compression, O(log n) ops |
| **ext4** | Linux standard | Mature, fast, no compression |
| **btrfs** | Modern Linux FS | Copy-on-write, ZSTD compression, snapshots |
| **ZFS** | Enterprise-grade FS | LZ4 compression, checksumming, data integrity |

---

## 🛠️ Common Tasks

### Rebuild Docker Image
```powershell
docker build --no-cache -t razorfs-test -f docker\Dockerfile .
```

### Run Specific Tests
```powershell
# Compression test only
docker run --privileged razorfs-test bash -c "./tests/docker/test_compression.sh"

# Recovery test only  
docker run --privileged razorfs-test bash -c "./tests/docker/test_recovery.sh"
```

### Clean Up Docker
```powershell
# Remove containers and images
docker system prune -a

# Check disk usage
docker system df
```

### View Logs
```powershell
# List all logs
dir logs

# View latest sync log
Get-Content logs\windows_sync_*.log | Select-Object -Last 50
```

---

## 🐛 Troubleshooting

### Docker Desktop not starting
```powershell
# Restart Docker Desktop
Stop-Process -Name "Docker Desktop" -Force
Start-Sleep -Seconds 5
Start-Process "C:\Program Files\Docker\Docker\Docker Desktop.exe"

# Wait for Docker to be ready
docker ps
```

### Permission errors
```powershell
# Run PowerShell as Administrator
# Right-click PowerShell → "Run as Administrator"
```

### Build fails
```powershell
# Clear Docker cache
docker system prune -a

# Rebuild from scratch
docker build --no-cache -t razorfs-test .
```

### Slow performance
```
# Increase Docker resources:
# Docker Desktop → Settings → Resources
# - Memory: 8GB (minimum 4GB)
# - CPUs: 4 (minimum 2)
# - Disk: 100GB
```

---

## 📚 Additional Resources

### Documentation
- [WORKFLOW.md](WORKFLOW.md) - Complete workflow guide
- [README.md](README.md) - Automatically generated index
- [docker/](docker/) - Docker configuration files

### WSL2 Testing
For native Linux testing, use WSL2:
```bash
# From WSL2
cd /home/nico/WORK_ROOT/razorfs
./tests/docker/benchmark_filesystems.sh
./sync-windows.sh
```

### GitHub Repository
- Project: https://github.com/ncandio/razorfs
- Issues: https://github.com/ncandio/razorfs/issues
- Docs: https://github.com/ncandio/razorfs/tree/main/docs

---

## 🎯 Best Practices

1. ✅ **Run benchmarks after code changes** - Catch performance regressions early
2. ✅ **Keep Docker Desktop updated** - Latest version has best WSL2 integration
3. ✅ **Allocate enough resources** - 8GB RAM, 4 CPUs for best performance
4. ✅ **Clean up regularly** - `docker system prune -a` to free disk space
5. ✅ **Review graphs** - Visual comparison is easier than raw numbers
6. ✅ **Keep results** - Historical data helps track performance trends

---

**Last Updated**: 2025-11-01  
**Maintained by**: RazorFS Development Team  
**Questions?**: Create an issue on GitHub
