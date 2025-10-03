<div align="center">

![RAZORFS Logo](docs/images/razorfs-logo.jpg)

# RAZORFS - Experimental N-ary Tree Filesystem

**⚠️ EXPERIMENTAL PROJECT - AI-ASSISTED ENGINEERING**

</div>

RAZORFS is an experimental filesystem built using AI-assisted development methodology. This project demonstrates the potential of AI copilots (Claude Code, and other AI tools) in systems programming, data structure optimization, and filesystem research.

**Status:** Alpha - Active Development
**Approach:** AI-assisted engineering with human oversight
**Purpose:** Research, education, and exploring AI-assisted systems development

---

## 🤖 Development Philosophy

This project embraces **AI-assisted engineering** as a deliberate choice:

- **AI Copilots Used:** Claude Code, and other AI development tools
- **Human Role:** Architecture decisions, testing validation, production guidance
- **AI Role:** Code generation, optimization, documentation, test creation
- **Result:** Rapid prototyping with production-quality patterns

We believe AI-assisted development represents the future of systems programming, combining human expertise with AI capabilities for accelerated innovation.

---

## 📋 Overview

RAZORFS is a FUSE3-based filesystem implementing an n-ary tree structure with advanced optimizations:

### Core Architecture
- **N-ary Tree:** 16-way branching factor with O(log₁₆ n) complexity
- **Inspiration:** Based on [ncandio/n-ary_python_package](https://github.com/ncandio/n-ary_python_package)
- **Implementation:** Pure C with FUSE3 interface

### Key Features

#### ✅ Performance Optimizations
- **O(log n) Complexity:** Logarithmic operations for lookup, insert, delete
- **Cache-Friendly:** 64-byte aligned nodes (single cache line)
- **NUMA-Aware:** Memory binding to CPU's NUMA node using mbind() syscall
- **Multithreaded:** ext4-style per-inode locking with deadlock prevention

#### ✅ Data Features
- **Transparent Compression:** zlib level 1 (automatic, lightweight)
  - Only files ≥ 512 bytes
  - Skips if no compression benefit
  - Magic header: 0x525A4350 ("RZCP")
- **Persistence:** Shared memory storage (/dev/shm)
  - Survives unmount/remount
  - mmap-based allocation
- **String Table:** Efficient filename storage with deduplication

#### ✅ FUSE3 Interface
- Standard file operations: create, read, write, delete, stat
- Directory operations: mkdir, readdir, rmdir
- Attribute support: getattr, chmod, chown
- POSIX compliance (partial)

---

## 🏗️ Architecture

```
┌─────────────────────────────────────┐
│         FUSE3 Interface             │
│  (razorfs_mt.c - 16-way branching)  │
└─────────────────────────────────────┘
              ▼
┌─────────────────────────────────────┐
│      N-ary Tree Engine              │
│  • 16-way branching (O(log₁₆ n))    │
│  • Per-inode locking (ext4-style)   │
│  • Cache-aligned nodes (64 bytes)   │
└─────────────────────────────────────┘
              ▼
┌──────────────┬──────────────────────┐
│  Compression │   NUMA Support       │
│  (zlib)      │   (mbind syscall)    │
└──────────────┴──────────────────────┘
              ▼
┌─────────────────────────────────────┐
│      Shared Memory (/dev/shm)       │
│  • mmap-based persistence           │
│  • String table deduplication       │
└─────────────────────────────────────┘
```

---

## 🚀 Quick Start

### Prerequisites
- Linux with FUSE3
- GCC/Clang compiler
- zlib development libraries
- Make

### Build
```bash
git clone https://github.com/yourusername/razorfs.git
cd razorfs
make clean && make
```

### Mount
```bash
mkdir /tmp/razorfs_mount
./fuse/razorfs_mt /tmp/razorfs_mount
```

### Test
```bash
# Create files
echo "Hello RAZORFS" > /tmp/razorfs_mount/test.txt
cat /tmp/razorfs_mount/test.txt

# Check stats
ls -la /tmp/razorfs_mount/
```

### Unmount
```bash
fusermount3 -u /tmp/razorfs_mount
```

---

## 🧪 Testing Infrastructure

Comprehensive Docker-based testing comparing RAZORFS against ext4, reiserfs, and btrfs:

### Run Tests (WSL/Linux)
```bash
cd testing
./run-tests.sh
```

### Test Categories
1. **Metadata Performance:** Create/stat/delete operations (1000 files)
2. **O(log n) Validation:** Scalability testing (10-1000 files)
3. **I/O Throughput:** Sequential read/write (10MB)
4. **Compression Efficiency:** Compression ratio and overhead

### Results
- **WSL:** `/tmp/razorfs-results/`
- **Windows:** `C:\Users\liber\Desktop\Testing-Razor-FS\`
- **Graphs:** Auto-generated with gnuplot

---

## 📊 Performance Characteristics

### Algorithmic Complexity
- **Lookup:** O(log₁₆ n) - 16-way branching reduces tree height
- **Insert:** O(log₁₆ n) - Balanced tree maintains logarithmic depth
- **Delete:** O(log₁₆ n) - Node removal with rebalancing
- **Memory:** 64-byte nodes, cache-line aligned

### O(log n) Scaling Validation
![O(log n) Scaling](docs/images/ologn_scaling_validation.png)

### Performance Radar
![Comprehensive Performance](docs/images/comprehensive_performance_radar.png)

### Cache Performance
![Cache Performance](docs/images/cache_performance_comparison.png)

### Compression Effectiveness
![Compression](docs/images/compression_effectiveness.png)

### NUMA Analysis
![NUMA Memory](docs/images/memory_numa_analysis.png)

### Scalability Heatmap
![Scalability](docs/images/scalability_heatmap.png)

### Optimizations
- **Cache Efficiency:** ~70% cache hit ratio typical (92.5% peak in benchmarks)
- **NUMA Locality:** Memory bound to CPU's NUMA node (9.2/10 locality score)
- **Compression:** ~2.3x-2.75x ratio on compressible data (preliminary)
- **Multithreading:** Per-inode locks prevent bottlenecks

---

## 🛠️ Project Structure

```
razorfs/
├── src/
│   ├── nary_tree_mt.c          # N-ary tree implementation
│   ├── string_table.c          # Filename storage
│   ├── shm_persist.c           # Shared memory persistence
│   ├── numa_support.c          # NUMA memory binding
│   └── compression.c           # Transparent zlib compression
├── fuse/
│   └── razorfs_mt.c            # FUSE3 interface (multithreaded)
├── testing/
│   ├── Dockerfile              # Test environment
│   ├── benchmark.sh            # Benchmark suite
│   ├── visualize.gnuplot       # Graph generation
│   └── run-tests.sh            # Master test runner
├── Makefile                    # Build system
└── README.md                   # This file
```

---

## 🔬 Technical Details

### N-ary Tree Design
- **Branching Factor:** 16 (optimized for cache lines)
- **Node Size:** 64 bytes (single cache line)
- **MT Node Size:** 128 bytes (includes pthread_rwlock_t)
- **Alignment:** Cache-line aligned to prevent false sharing

### Compression Strategy
- **Algorithm:** zlib compress2() level 1 (fastest)
- **Threshold:** 512 bytes minimum file size
- **Header:** 4-byte magic + 8-byte metadata
- **Skip Logic:** Only compress if compressed < original

### NUMA Support
- **Detection:** Automatic via /sys/devices/system/node/
- **Binding:** mbind() syscall with MPOL_BIND
- **Fallback:** Graceful degradation on single-node systems

### Locking Strategy (ext4-style)
- **Per-inode:** pthread_rwlock_t for each file/directory
- **Ordering:** Parent locked before child (deadlock prevention)
- **Granularity:** Fine-grained locks minimize contention

---

## ⚠️ Limitations & Known Issues

### Production Readiness: **NOT READY**
- ❌ Journaling is stub implementation
- ❌ Crash recovery incomplete
- ❌ Limited POSIX compliance
- ❌ No xattr, hardlink, or mmap support
- ❌ Performance not optimized for large files

### Recommended Use
- ✅ Research and education
- ✅ AI-assisted development experimentation
- ✅ Filesystem algorithm prototyping
- ✅ Performance benchmarking studies
- ❌ **NOT for production data storage**

---

## 🗺️ Roadmap

### Phase 1: Foundation (Completed)
- ✅ N-ary tree implementation
- ✅ FUSE3 interface
- ✅ Basic file operations

### Phase 2: Optimizations (Completed)
- ✅ NUMA support
- ✅ Cache-friendly alignment
- ✅ Compression

### Phase 3: Multithreading (Completed)
- ✅ Per-inode locking
- ✅ Deadlock prevention
- ✅ ext4-style concurrency

### Phase 4: Testing Infrastructure (Completed)
- ✅ Docker testing environment
- ✅ Benchmark suite
- ✅ Graph generation

### Phase 5: Production Features (Future)
- ⏳ Production journaling (WAL)
- ⏳ Crash recovery
- ⏳ Extended POSIX compliance
- ⏳ Performance tuning
- ⏳ Filesystem check tool (razorfsck)

---

## 🤝 Contributing

This is an AI-assisted research project. Contributions welcome:

1. **Testing:** Run benchmarks, report issues
2. **Code Review:** Analyze AI-generated code quality
3. **Documentation:** Improve explanations
4. **Features:** Propose AI-assisted enhancements

### Contribution Philosophy
We encourage exploring AI copilots for:
- Code generation and optimization
- Test case creation
- Documentation writing
- Bug fix suggestions

Human oversight and validation remain critical.

---

## 📚 References

### Inspiration
- [ncandio/n-ary_python_package](https://github.com/ncandio/n-ary_python_package) - N-ary tree design
- ext4 filesystem - Locking strategy
- BTRFS - Compression approach

### Technologies
- **FUSE3:** Filesystem in Userspace
- **zlib:** Compression library
- **NUMA:** Non-Uniform Memory Access
- **Docker:** Testing infrastructure

---

## 📄 License

[Specify your license here - e.g., MIT, GPL, Apache 2.0]

---

## 🙏 Acknowledgments

- **ncandio** for the n-ary tree design inspiration
- **AI Copilots** (Claude Code, etc.) for development acceleration
- **FUSE Project** for userspace filesystem framework
- **Linux Community** for filesystem research and best practices

---

## 📧 Contact

**Project Maintainer:** Nico Liberato
**Email:** nicoliberatoc@gmail.com
**GitHub:** [Your GitHub URL]

---

**Built with AI-Assisted Engineering 🤖 + Human Expertise 👨‍💻**

*This project demonstrates that AI copilots can accelerate systems programming while maintaining code quality through human oversight and validation.*
