# Changelog

All notable changes to RAZORFS will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- Documentation reorganization and cleanup
- Comprehensive benchmark suite with Docker integration
- CI/CD pipeline with GitHub Actions

## [0.1.0-alpha] - 2025-10-19

### Added
- N-ary tree core implementation with 16-way branching factor
- O(log n) complexity for all major operations using binary search
- FUSE3 interface for standard filesystem operations
- NUMA-aware memory optimization using mbind() syscall
- Cache-friendly design with 64-byte aligned nodes
- Transparent zlib compression (level 1, files ≥512 bytes)
- BFS memory layout for spatial locality
- Multithreading with ext4-style per-inode locking
- Deadlock prevention through global lock ordering
- Disk-backed persistent storage via mmap (MAP_SHARED)
- Write-Ahead Logging (WAL) with ARIES-style recovery
- Crash recovery with Analysis/Redo/Undo phases
- Extended attributes (xattr) support - 4 namespaces
- Hardlink support with reference counting (up to 65,535 links)
- Comprehensive test suite (98 unit + integration tests)
- Static analysis (cppcheck, clang-tidy)
- Dynamic analysis (Valgrind, ASan, UBSan, TSan)
- Code coverage reporting (65.7% lines, 82.8% functions)
- Docker-based benchmarking infrastructure
- Cross-compilation support (ARM64, PowerPC64LE, RISC-V)
- Security hardening (stack protector, RELRO, PIE)

### Performance
- Metadata Operations: ~1800ms for 1000 files (create/stat/delete)
- I/O Throughput: 16.44 MB/s write, 37.17 MB/s read
- Cache Efficiency: ~70% typical, 92.5% peak
- Binary Search: 4x faster than linear search for path operations

### Documentation
- Comprehensive README with architecture details
- Complexity analysis with mathematical proofs
- Feature design documents (WAL, recovery, xattr, hardlinks)
- Deployment and testing guides
- Security audit and policies
- Cross-compilation instructions

### Development
- AI-assisted development using Claude Sonnet 4.5
- ~2,500 lines of C code
- Developed in ~48 hours across 6 phases
- Human oversight and validation throughout

## [0.0.1-pre-alpha] - 2025-08-01

### Added
- Initial project structure
- Basic n-ary tree implementation
- Prototype FUSE interface

---

## Release Notes

### Version 0.1.0-alpha

This is the first alpha release of RAZORFS, marking the completion of Phase 6 (Persistence & Recovery).

**Status**: Experimental Alpha - NOT production-ready

**What Works**:
- ✅ All core filesystem operations (create, read, write, mkdir, etc.)
- ✅ O(log n) performance validated with benchmarks
- ✅ Disk-backed persistence (survives reboot)
- ✅ Crash recovery with WAL
- ✅ Extended attributes
- ✅ Hardlinks
- ✅ Multithreading
- ✅ Transparent compression

**Known Limitations**:
- ⏳ Performance needs tuning for ext4-level throughput
- ⏳ Large file handling (>10MB) needs optimization
- ⏳ No filesystem check tool (razorfsck) yet
- ⏳ Limited POSIX compliance (partial)

**Recommended Use**:
- ✅ Research and education
- ✅ Filesystem algorithm prototyping
- ✅ Performance benchmarking studies
- ✅ Development/testing workloads with backups
- ❌ NOT for production use
- ❌ NOT for critical data

**Next Steps** (Phase 7 - Production Hardening):
- Filesystem check tool (razorfsck)
- Performance optimization
- Storage compaction
- Enhanced error handling
- Monitoring and observability

---

For more information, see [README.md](README.md) and [docs/development/PRODUCTION_ROADMAP.md](docs/development/PRODUCTION_ROADMAP.md).
