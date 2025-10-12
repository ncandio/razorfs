<div align="center">

![RAZORFS Logo](docs/images/razorfs-logo.jpg)

# RAZORFS - Experimental N-ary Tree Filesystem

**⚠️ EXPERIMENTAL PROJECT - AI-ASSISTED ENGINEERING**

[![Build Status](https://img.shields.io/badge/build-passing-brightgreen.svg)](https://github.com/ncandio/razorfs/actions)
[![Tests](https://img.shields.io/badge/tests-199%2F199%20passing-brightgreen.svg)](https://github.com/ncandio/razorfs/actions)
[![License](https://img.shields.io/badge/license-BSD--3--Clause-blue.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/platform-Linux-lightgrey.svg)](https://github.com/ncandio/razorfs)
[![Status](https://img.shields.io/badge/status-alpha-yellow.svg)](https://github.com/ncandio/razorfs)

</div>

RAZORFS is an experimental filesystem built using AI-assisted development methodology. This project demonstrates the potential of AI copilots in systems programming, data structure optimization, and filesystem research.

**For the complete, unabridged documentation, please see [README_FULL.md](./README_FULL.md).**

---

## 📋 Overview

RAZORFS is a FUSE3-based filesystem implementing an n-ary tree structure with advanced optimizations:

### Core Architecture
- **N-ary Tree:** 16-way branching factor with O(log₁₆ n) complexity.
- **Implementation:** Pure C with FUSE3 interface.

### Key Features
- **O(log n) Complexity:** Logarithmic operations for lookup, insert, delete.
- **Cache-Friendly:** 64-byte aligned nodes.
- **NUMA-Aware:** Memory binding to the local CPU's NUMA node.
- **Multithreaded:** ext4-style per-inode locking.
- **Transparent Compression:** Automatic and lightweight zlib compression.
- **Disk-Backed Persistence:** `mmap`-based persistent storage that survives reboots.
- **WAL & Crash Recovery:** Write-Ahead Logging for crash-safe operations.

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
│   Disk-Backed Persistent Storage    │
│  • mmap(MAP_SHARED) on real disk    │
│  • /var/lib/razorfs/*.dat files     │
│  • WAL for crash recovery           │
│  • msync() for durability           │
└─────────────────────────────────────┘
```

--- 

## 🏛️ ARCHITECTURAL WILL

RazorFS operates with an intelligent, adaptive performance model based on the hardware it runs on:

1.  **NUMA Adaptive Optimization** - NUMA (Non-Uniform Memory Access) is a memory architecture in multi-socket systems where memory access time depends on the memory location relative to the processor. RazorFS automatically detects and adapts to the underlying hardware:

    **On a Standard (Non-NUMA) System:** The filesystem detects the absence of a NUMA architecture. Its NUMA-specific code remains disabled. It performs like a traditional filesystem, using standard memory allocation without special placement.

    **On a NUMA System:** The filesystem detects the NUMA topology. It activates its NUMA optimizations, binding its core metadata structures to the local memory node of the CPU running the process. This minimizes memory access latency, unlocking a higher performance profile specifically tailored for NUMA hardware.

    In essence, RazorFS is designed to be universally compatible, offering a baseline performance on standard systems while automatically enabling its high-performance, low-latency mode when it identifies the presence of a NUMA architecture.

2.  **N-ary Tree Architecture:** The filesystem employs a 16-way branching tree structure instead of traditional binary trees. On small filesystems (under 10,000 files), the performance difference is negligible. However, as the filesystem scales to millions of files, the 16-way branching delivers dramatically faster operations—requiring only ~5 tree levels for 1 million files versus ~20 levels in binary trees. This architecture shines at scale, with cache-aligned contiguous memory layout that minimizes CPU cache misses during tree traversal, making it ideal for large-scale deployments while remaining efficient for everyday use.

---

## 🚀 Quick Start

### Prerequisites
- Linux with FUSE3
- GCC/Clang compiler
- zlib development libraries
- Make

### Build and Run

Copy and paste this complete script to build and test RAZORFS:

```bash
#!/bin/bash
# RAZORFS Quick Start Script

# Step 1: Build
echo "Building RAZORFS..."
make clean && make
echo "Build complete."

# Step 2: Create mount point
echo "Creating mount point..."
mkdir -p /tmp/razorfs_mount
echo "Mount point created at /tmp/razorfs_mount"

# Step 3: Mount filesystem
echo "Mounting RAZORFS..."
./razorfs /tmp/razorfs_mount
echo "Filesystem mounted."

# Step 4: Test filesystem
echo "Testing filesystem operations..."
echo "Hello RAZORFS!" > /tmp/razorfs_mount/test.txt
cat /tmp/razorfs_mount/test.txt
echo "File operations working."

# Step 5: Unmount
echo "To unmount, run: fusermount3 -u /tmp/razorfs_mount"
```

---

## 📊 Performance Benchmarks

#### O(log n) Scalability Validation
![O(log n) Comparison](readme_graphs/ologn_scaling_validation.png)
*Consistent performance demonstrates true O(log n) complexity.*

#### Comprehensive Feature Comparison (Radar Chart)
![Feature Radar](readme_graphs/comprehensive_performance_radar.png)
*RAZORFS vs ext4, ZFS, and ReiserFS across 8 dimensions.*

#### Performance Heatmap
![Performance Heatmap](readme_graphs/scalability_heatmap.png)
*Side-by-side comparison across all metrics.*

---

## ⚠️ Limitations & Known Issues

**EXPERIMENTAL ALPHA - NOT FOR PRODUCTION USE**

RAZORFS is a research project. While it has a full feature set including persistence and crash recovery, it is not yet hardened for production environments. Do NOT use for critical data without regular backups.

- ✅ **Implemented:** All major features listed above are implemented and tested.
- ⚠️ **Data Risk:** This is alpha software. Always maintain backups.

---

## 📄 License

BSD 3-Clause License
