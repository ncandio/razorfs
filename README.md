<div align="center">

![RAZORFS Logo](docs/images/razorfs-logo.jpg)

# RAZORFS - Experimental N-ary Tree Filesystem

**⚠️ EXPERIMENTAL PROJECT - AI-ASSISTED ENGINEERING**

[![Build Status](https://img.shields.io/badge/build-passing-brightgreen.svg)](https://github.com/ncandio/razorfs/actions)
[![Tests](https://img.shields.io/badge/tests-199%2F199%20passing-brightgreen.svg)](https://github.com/ncandio/razorfs/actions)
[![Coverage](https://img.shields.io/badge/coverage-65.7%25%20lines-blue.svg)](https://github.com/ncandio/razorfs/actions)
[![License](https://img.shields.io/badge/license-BSD--3--Clause-blue.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/platform-Linux-lightgrey.svg)](https://github.com/ncandio/razorfs)

</div>

---

## What is RazorFS?

RazorFS is an experimental FUSE3-based filesystem implementing an **n-ary tree structure** with advanced optimizations for modern hardware. Built using AI-assisted development methodology, it demonstrates the potential of AI copilots in systems programming and filesystem research.

**Status:** Alpha - Active Development (Phase 7)

### Key Features

- **O(log n) Operations** - Logarithmic complexity for all filesystem operations
- **Cache-Optimized** - 64-byte aligned nodes, 70%+ cache hit ratios
- **NUMA-Aware** - Automatic memory binding on NUMA systems
- **Multithreaded** - ext4-style per-inode locking, deadlock-free
- **Transparent Compression** - zlib level 1, automatic and lightweight
- **Disk-Backed Persistence** - Full mmap-based storage, survives reboots
- **Crash Recovery** - ARIES-style WAL with automatic recovery

---

## Quick Start

### Prerequisites
- Linux with FUSE3
- GCC/Clang compiler
- zlib development libraries

```bash
# Install dependencies (Ubuntu/Debian)
sudo apt-get install fuse3 libfuse3-dev zlib1g-dev build-essential
```

### Build and Run

```bash
# Clone repository
git clone https://github.com/ncandio/razorfs.git
cd razorfs

# Build
make clean && make

# Setup persistent storage
./scripts/setup_storage.sh

# Mount filesystem
mkdir -p /tmp/razorfs_mount
./razorfs /tmp/razorfs_mount

# Test it
echo "Hello RazorFS!" > /tmp/razorfs_mount/test.txt
cat /tmp/razorfs_mount/test.txt

# Unmount
fusermount3 -u /tmp/razorfs_mount
```

---

## Performance

### O(log n) Scalability

![O(log n) Validation](readme_graphs/ologn_scaling_validation.png)

Consistent performance from 10 to 1,000 files demonstrates true O(log n) complexity.

### Feature Comparison

![Performance Radar](readme_graphs/comprehensive_performance_radar.png)

RazorFS compared against ext4, ZFS, and ReiserFS across 8 dimensions.

### Measured Metrics

| Operation | Performance |
|-----------|-------------|
| **Metadata** (create/stat/delete) | ~1.7ms per operation |
| **Write Throughput** | 16.44 MB/s |
| **Read Throughput** | 37.17 MB/s |
| **Compression** | 50-70% space savings (text) |
| **Cache Hit Ratio** | 70%+ typical, 92.5% peak |

**Note:** These are baseline measurements. Performance tuning to achieve ext4-level throughput is ongoing (Phase 7).

---

## Architecture Highlights

### N-ary Tree Design
- **16-way branching** - Balances depth with cache efficiency
- **Binary search on sorted children** - O(log k) = O(4) per level
- **Cache-aligned nodes** - 64 bytes (single cache line)
- **Tree depth for 1M files** - log₁₆(1M) ≈ 5 levels

### Persistence
- **Tree nodes:** mmap'd to `/var/lib/razorfs/nodes.dat`
- **File data:** Per-file mmap'd storage
- **String table:** Disk-backed with deduplication
- **WAL:** fsync'd transaction log, automatic crash recovery

### Concurrency
- **Per-inode locking** - Fine-grained `pthread_rwlock_t`
- **Global lock ordering** - Deadlock prevention (tree → parent → child)
- **No retry logic** - Eliminates livelock

**📖 Full Details:** See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md)

---

## Documentation

### User Guides
- **[Getting Started](docs/ARCHITECTURE.md#quick-start)** - Installation and basic usage
- **[Testing Guide](docs/TESTING.md)** - Running tests and benchmarks
- **[Persistence Guide](docs/PERSISTENCE.md)** - Data durability and recovery

### Architecture
- **[Architecture Overview](docs/ARCHITECTURE.md)** - Design principles and implementation
- **[Complexity Analysis](docs/architecture/COMPLEXITY_ANALYSIS.md)** - Mathematical proofs of O(log n)
- **[Cache Locality](docs/architecture/CACHE_LOCALITY.md)** - Cache optimization details
- **[WAL Design](docs/architecture/WAL_DESIGN.md)** - Write-Ahead Logging
- **[Recovery Design](docs/architecture/RECOVERY_DESIGN.md)** - Crash recovery system

### Development
- **[Contributing](CONTRIBUTING.md)** - Contribution guidelines
- **[Roadmap](docs/development/PRODUCTION_ROADMAP.md)** - Future plans
- **[Status](docs/development/STATUS.md)** - Current development status

### Operations
- **[Deployment Guide](docs/operations/DEPLOYMENT_GUIDE.md)** - Production deployment
- **[Security](docs/security/SECURITY_AUDIT.md)** - Security audit and hardening
- **[CI/CD](docs/ci-cd/GITHUB_ACTIONS.md)** - Continuous integration setup

---

## Testing

### Run Tests

```bash
# Complete test suite
make test

# Individual test categories
make test-unit           # Unit tests (199 tests)
make test-static         # Static analysis
make test-valgrind       # Memory leak detection
make test-coverage       # Generate coverage report

# Docker benchmarks
cd tests/docker && ./benchmark_filesystems.sh
```

### Test Coverage

- **Total Tests:** 199 passing
- **Line Coverage:** 65.7%
- **Function Coverage:** 82.8%
- **Quality Gates:** All passing (zero memory leaks, zero sanitizer violations)

**📖 Full Details:** See [docs/TESTING.md](docs/TESTING.md)

---

## Status & Limitations

### ✅ What's Implemented

- ✅ N-ary tree with O(log n) operations
- ✅ FUSE3 interface (create, read, write, mkdir, rmdir, etc.)
- ✅ Multithreading with per-inode locking
- ✅ Transparent compression (zlib level 1)
- ✅ Disk-backed persistence (mmap-based)
- ✅ WAL journaling with crash recovery
- ✅ Extended attributes (xattr)
- ✅ Hardlink support
- ✅ NUMA-aware memory allocation
- ✅ Comprehensive test suite (199 tests)

### ⏳ In Progress (Phase 7)

- ⏳ razorfsck consistency checker
- ⏳ Performance tuning for ext4-level throughput
- ⏳ Background flusher thread
- ⏳ Storage compaction
- ⏳ Large file optimization (>10MB)

### ⚠️ Important Warnings

> **EXPERIMENTAL ALPHA SOFTWARE**
>
> - ⚠️ NOT production-ready - use for research and testing only
> - ⚠️ Data only persists on real filesystems (not tmpfs)
> - ⚠️ Performance optimization ongoing (Phase 7)
> - ⚠️ Always maintain backups of important data
> - ⚠️ No warranty - see [LICENSE](LICENSE) (BSD-3-Clause)

### Recommended Use Cases

- ✅ Filesystem research and education
- ✅ AI-assisted development experimentation
- ✅ Algorithm prototyping and benchmarking
- ✅ Non-critical testing environments
- ❌ NOT for production use
- ❌ NOT for critical data

---

## AI-Assisted Development

This project embraces **AI-assisted engineering** as a deliberate approach:

- **Primary AI Model:** Claude Sonnet 4.5 via Claude Code
- **Development Period:** August 2025 - Present
- **AI Role:** Code generation, optimization, documentation, testing
- **Human Role:** Architecture decisions, validation, strategic direction

**Development Timeline:**
- **Phases 1-6 (48 hours):** Core implementation using AI-assisted development
- **Phase 7 (Current):** Production hardening and optimization

We believe AI-assisted development represents the future of systems programming, combining human expertise with AI capabilities for accelerated innovation.

**Learn More:** [Deep Dive into LLMs](https://www.youtube.com/watch?v=7xTGNNLPyMI)

---

## Contributing

Contributions welcome! This is an AI-assisted research project exploring:
- AI copilots in systems programming
- Filesystem algorithm optimization
- Performance benchmarking methodologies

### How to Contribute

1. **Testing** - Run benchmarks, report issues
2. **Code Review** - Analyze AI-generated code quality
3. **Documentation** - Improve explanations and guides
4. **Features** - Propose AI-assisted enhancements

See [CONTRIBUTING.md](CONTRIBUTING.md) for detailed guidelines.

---

## Project Structure

```
razorfs/
├── src/                    # Core implementation (C)
├── fuse/                   # FUSE3 interface
├── docs/                   # Documentation
├── tests/                  # Test suite
├── scripts/                # Build and test scripts
├── benchmarks/             # Performance benchmarks
└── tools/                  # Utilities (razorfsck)
```

---

## License

BSD 3-Clause License - See [LICENSE](LICENSE)

Copyright (c) 2025, Nico Liberato

---

## Acknowledgments

- **ncandio** - n-ary tree design inspiration
- **Claude Code** (Anthropic) - AI-assisted development
- **FUSE Project** - Userspace filesystem framework
- **Linux Community** - Filesystem research and best practices

---

## Contact

**Project Maintainer:** Nico Liberato
- **Email:** nicoliberatoc@gmail.com
- **GitHub:** https://github.com/ncandio
- **Repository:** https://github.com/ncandio/razorfs

---

**Built with AI-Assisted Engineering 🤖 + Human Expertise 👨‍💻**

*This project demonstrates that AI copilots can accelerate systems programming while maintaining code quality through human oversight and validation.*
