# RAZORFS Docker - 5-Minute Quick Start

**Get RAZORFS testing infrastructure running in 5 minutes on Windows + WSL2**

---

## Prerequisites Check ✓

Before starting, ensure you have:

- [ ] Windows 10/11 (Build 19041 or higher)
- [ ] WSL2 installed (`wsl --version` from PowerShell)
- [ ] Docker Desktop for Windows running
- [ ] Ubuntu WSL2 distro installed

**Don't have these?** See [DOCKER_WORKFLOW.md](DOCKER_WORKFLOW.md#windows-environment-setup) for detailed setup.

---

## Step 1: Clone Repository (1 minute)

```bash
# From Ubuntu WSL2 terminal
cd ~
mkdir -p WORK_ROOT
cd WORK_ROOT
git clone https://github.com/ncandio/razorfs.git
cd razorfs
```

---

## Step 2: Create Windows Test Directory (30 seconds)

```bash
# Create Windows sync directory
mkdir -p /mnt/c/Users/liber/Desktop/Testing-Razor-FS/{benchmarks,readme_graphs,logs}

# Verify it worked
ls -la /mnt/c/Users/liber/Desktop/Testing-Razor-FS/
```

**Expected output**: You should see three empty directories.

---

## Step 3: Build Docker Image (2 minutes)

```bash
# Build the RAZORFS test image
docker build -t razorfs-test .

# Verify build succeeded
docker images razorfs-test
```

**Expected output**:
```
REPOSITORY     TAG       IMAGE ID       CREATED         SIZE
razorfs-test   latest    abc123def456   2 minutes ago   1.2GB
```

---

## Step 4: Run First Test (1 minute)

### Option A: Quick Test (README Graphs Only)

```bash
docker run -v $(pwd)/readme_graphs:/app/readme_graphs razorfs-test \
  ./generate_tagged_graphs.sh
```

**Results**: Graphs appear in `readme_graphs/` (5 PNG files)

### Option B: Full Benchmark Suite (5 minutes)

```bash
docker run --privileged \
  -v $(pwd)/benchmarks:/app/benchmarks \
  -v /mnt/c/Users/liber/Desktop/Testing-Razor-FS:/windows-sync \
  razorfs-test \
  bash -c "./tests/docker/benchmark_filesystems.sh && cp -r benchmarks/* /windows-sync/benchmarks/"
```

**Results**: Full benchmark report synced to Windows Desktop

---

## Step 5: View Results (10 seconds)

### From Windows

Open Windows Explorer and navigate to:
```
C:\Users\liber\Desktop\Testing-Razor-FS\
```

**You'll see**:
- `benchmarks/` - Performance reports and data
- `readme_graphs/` - Documentation graphs
- `logs/` - Execution logs

### From WSL2

```bash
# View latest benchmark report
cat benchmarks/BENCHMARK_REPORT_*.md | head -100

# View graphs (requires X server or copy to Windows)
cp readme_graphs/*.png /mnt/c/Users/liber/Desktop/Testing-Razor-FS/readme_graphs/
```

---

## Alternative: Docker Compose (One Command)

Instead of manual docker run commands, use docker-compose:

```bash
# Start all services
docker-compose up

# Services included:
# - razorfs-test: Full benchmark suite
# - graph-generator: README graph generation
# - benchmark-scheduler: Hourly automated tests

# Stop services
docker-compose down
```

---

## What You Just Did

1. ✅ Built a complete RAZORFS testing environment in Docker
2. ✅ Ran filesystem benchmarks comparing RAZORFS vs ext4/ZFS
3. ✅ Generated professional graphs tagged with commit SHA
4. ✅ Synced results to Windows Desktop for easy access

---

## Next Steps

### Daily Development Workflow

```bash
# 1. Make code changes
vim src/nary_tree_mt.c

# 2. Rebuild
make clean && make

# 3. Quick test
docker run -v $(pwd)/readme_graphs:/app/readme_graphs razorfs-test \
  ./generate_tagged_graphs.sh

# 4. Sync to Windows
./scripts/sync_to_windows.sh

# 5. View results in Windows
explorer.exe "C:\\Users\\liber\\Desktop\\Testing-Razor-FS"
```

### Continuous Testing

```bash
# Run benchmarks every hour automatically
docker-compose --profile scheduler up

# Monitor in another terminal
docker logs -f razorfs-benchmark-scheduler
```

### Interactive Debugging

```bash
# Start interactive shell
docker run -it --privileged razorfs-test bash

# Inside container:
./razorfs --help
./tests/docker/benchmark_filesystems.sh
cd tests/build && ctest
```

---

## Troubleshooting

### "Docker command not found"

**Solution**: Docker Desktop not running. Start it from Windows Start Menu.

### "Permission denied on /mnt/c/"

**Solution**: WSL2 permissions issue. See [DOCKER_WORKFLOW.md#troubleshooting](DOCKER_WORKFLOW.md#troubleshooting).

### "Build fails at cmake"

**Solution**: Old Docker cache. Run:
```bash
docker build --no-cache -t razorfs-test .
```

### "No graphs generated"

**Solution**: Missing gnuplot. Should be in Docker image. Rebuild:
```bash
docker build -t razorfs-test .
```

---

## Verify Everything Works

Run this test to verify your setup:

```bash
#!/bin/bash
echo "=== RAZORFS Docker Setup Verification ==="
echo ""

echo "1. Checking Docker..."
docker --version && echo "✅ Docker OK" || echo "❌ Docker not found"

echo "2. Checking WSL2 mount..."
[ -d /mnt/c/Users/liber ] && echo "✅ Windows mount OK" || echo "❌ Mount not found"

echo "3. Checking RAZORFS image..."
docker images razorfs-test | grep razorfs-test && echo "✅ Image built" || echo "❌ Build image first"

echo "4. Checking Windows sync directory..."
[ -d /mnt/c/Users/liber/Desktop/Testing-Razor-FS ] && echo "✅ Sync dir exists" || echo "❌ Create sync directory"

echo ""
echo "All checks passed? You're ready to run tests!"
```

---

## Resources

- **Full Documentation**: [DOCKER_WORKFLOW.md](DOCKER_WORKFLOW.md)
- **Benchmark Details**: [tests/docker/README.md](tests/docker/README.md)
- **Project README**: [README.md](README.md)
- **GitHub Issues**: [https://github.com/ncandio/razorfs/issues](https://github.com/ncandio/razorfs/issues)

---

**Time Invested**: ~5 minutes
**Result**: Fully functional Docker test infrastructure with Windows sync
**Next**: Explore continuous testing workflows in [DOCKER_WORKFLOW.md](DOCKER_WORKFLOW.md)
