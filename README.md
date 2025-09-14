# razorfs Filesystem

[![Build Status](https://img.shields.io/badge/build-passing-brightgreen)](fuse/Makefile) [![License](https://img.shields.io/badge/license-GPL--2.0-blue)](LICENSE) [![Focus](https://img.shields.io/badge/focus-FUSE-orange)]()

A modern filesystem implementation with enterprise-grade persistence and transaction logging.

---

## Current Status & Development Focus

The `razorfs` project is currently in heavy testing and development phase, with primary focus on the **FUSE (Filesystem in Userspace) implementation** and **SRC core library**. This provides a stable and reliable foundation for the filesystem.

### Current Implementation Status

- **Production-Ready FUSE Layer**: The FUSE implementation now uses the complete RAZOR core API with full persistence support
- **Enterprise Persistence**: Filesystem state is automatically saved and restored using the core RAZOR transaction logging system
- **Full Filesystem Operations**: All standard operations are implemented and tested:
  - ✅ Create/delete files and directories with proper permissions
  - ✅ Read/write operations with offset handling and metadata updates
  - ✅ Directory listing with proper stat information
  - ✅ Automatic persistence on mount/unmount operations

### Active Development Areas

We are conducting extensive testing on:
- **Core Library (`src/` folder)**: Transaction logging, crash recovery, and data integrity systems
- **FUSE Implementation (`fuse/` folder)**: Performance optimization and edge case handling
- **Integration Testing**: Real-world usage patterns and stress testing

### Features Under Development
- **Compression System**: Compression capabilities are under active development. We are working on implementing efficient compression algorithms, but cannot currently maintain the high compression ratios mentioned in earlier project documentation. The focus is on achieving reliable compression with reasonable performance trade-offs.
- **Performance Optimization**: Cache-aware data structures and SIMD optimizations are being evaluated
- **Advanced Features**: Extended attributes, symbolic links, and advanced permissions are planned

## Quick Start (FUSE)

This will build and run the FUSE filesystem. All data will be persisted in `/tmp/razorfs.dat`.

**1. Build the executable:**
```bash
cd fuse
make
```

**2. Create a mount point:**
```bash
mkdir -p /tmp/my_razorfs
```

**3. Run the filesystem:**

This command will start the filesystem and occupy the current terminal.
```bash
./razorfs_fuse /tmp/my_razorfs
```

**4. Use the filesystem:**

Open a **new terminal** and interact with your mount point.
```bash
ls -l /tmp/my_razorfs

echo "Hello from razorfs!" > /tmp/my_razorfs/welcome.txt

cat /tmp/my_razorfs/welcome.txt
```

To stop the filesystem, go to the first terminal and press `Ctrl+C`.

## Architecture Overview

- **Core Data Structure**: The filesystem hierarchy is managed exclusively by the `LinuxFilesystemNaryTree` class located in `src/`.
- **Metadata Storage**: File and directory metadata (permissions, size, timestamps) are stored directly on the nodes of the tree.
- **Content Storage**: For simplicity, the contents of all files are currently stored in an in-memory map and are not yet optimized for large files.
- **Persistence Model**: On clean shutdown, the entire tree and all file contents are serialized to a single binary file.

## Attribution

This implementation is inspired by the succinct data structure approach in the [ncandio/n-ary_python_package](https://github.com/ncandio/n-ary_python_package) repository.