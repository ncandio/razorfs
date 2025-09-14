# razorfs Filesystem

[![Build Status](https://img.shields.io/badge/build-passing-brightgreen)](fuse/Makefile) [![License](https://img.shields.io/badge/license-GPL--2.0-blue)](LICENSE) [![Focus](https://img.shields.io/badge/focus-FUSE-orange)]()

A modern filesystem implementation with a focus on a clean, tree-based architecture.

---

## Current Status & Development Focus

The `razorfs` project has recently undergone a significant architectural refactoring. The primary focus of development and testing is now on the **FUSE (Filesystem in Userspace) implementation**, which provides a stable and reliable way to use the filesystem.

Key characteristics of the current `main` branch:

- **Unified Tree Architecture**: The FUSE implementation now uses the N-ary tree as the **single source of truth** for all filesystem structure and metadata. This improves maintainability and simplifies the implementation.
- **Functional Core**: All basic filesystem operations are fully functional:
  - ✅ Create files and directories (`create`, `mkdir`)
  - ✅ Read and write data with proper offset handling (`read`, `write`)
  - ✅ Delete files and directories (`unlink`, `rmdir`)
- **Persistence**: The filesystem state is saved to a binary file (`/tmp/razorfs.dat`) on shutdown and reloaded on startup.

### Features Under Development
- **Compression**: The architecture is designed to support compression, but this feature is currently under active development and not yet enabled. We are unable to maintain the compression ratios promised in earlier documentation as we focus on core stability.
- **Performance Tuning**: The current implementation is focused on correctness. Performance optimizations are a future goal.

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