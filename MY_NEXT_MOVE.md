# RAZORFS - Next Steps

**Date**: October 3, 2025
**Current Status**: Phase 3 Complete - Simple MT FUSE Built and Tested

## What We Just Completed

✅ **Phase 3 - Multithreaded Tree + Simple FUSE**
- Implemented `src/nary_tree_mt.c/h` with ext4-style per-inode locking
- Created simple C-based FUSE: `fuse/razorfs_mt.c` (no optimizations)
- All stress tests passing (1000 threads, 9.8M ops/sec on tree)
- FUSE tested with 10 concurrent threads successfully
- Zero data races verified with ThreadSanitizer
- All code committed to git

## Current Working State

**Built Binary**: `/home/nico/WORK_ROOT/RAZOR_repo/fuse/razorfs_mt`

**Working FUSE Operations**:
- ✅ mkdir - Create directories
- ✅ create - Create files (fixed to set fi->fh)
- ✅ read - Read file content
- ✅ write - Write file content
- ✅ unlink - Delete files (basic)
- ✅ rmdir - Remove directories (basic)
- ✅ getattr - Get file attributes
- ✅ readdir - List directory contents

**Test Scripts Created**:
- `fuse/test_mt_fuse.sh` - Basic operations test
- `fuse/test_mt_stress.sh` - Multithreaded stress test

**Build System**:
- `fuse/Makefile` - Simple build, no optimizations
- Dependencies: nary_tree_mt.c, string_table.c

## Next Steps - Phase 4: Minimal POSIX Compliance

### 4.1 Add rename() Operation
```c
static int razorfs_mt_rename(const char *from, const char *to, unsigned int flags) {
    // 1. Lookup source node
    // 2. Check if destination exists
    // 3. Update name in string table
    // 4. Update parent if moving to different directory
    // 5. Handle RENAME_NOREPLACE flag
}
```

### 4.2 Enhance unlink/rmdir
```c
static int razorfs_mt_unlink(const char *path) {
    // Add proper error codes:
    // - EISDIR if path is directory
    // - ENOENT if not found
    // - Verify file has no open handles
}

static int razorfs_mt_rmdir(const char *path) {
    // Add proper checks:
    // - ENOTDIR if not directory
    // - ENOTEMPTY if has children
    // - Update parent directory mtime
}
```

### 4.3 Add truncate() Support
```c
static int razorfs_mt_truncate(const char *path, off_t size,
                                 struct fuse_file_info *fi) {
    // 1. Find file data
    // 2. Resize buffer (grow or shrink)
    // 3. Update node size
    // 4. Zero-fill if growing
}
```

### 4.4 Test POSIX Features
- Test rename across directories
- Test rename with RENAME_NOREPLACE
- Test rmdir on non-empty directories (should fail)
- Test truncate to grow/shrink files
- Verify all error codes are correct

## Phase 5: Comprehensive Stability Testing

### 5.1 Long-Running Stability Tests
```bash
# 24+ hour stress test
while true; do
    # Create 1000 files
    # Read all files
    # Delete all files
    # Verify no memory leaks
done
```

### 5.2 Memory Analysis
- Run with valgrind for leak detection
- ThreadSanitizer for race conditions
- Track memory usage over 24 hours
- Verify no file descriptor leaks

### 5.3 Extreme Concurrency Tests
- 1000+ threads creating files simultaneously
- Random read/write/delete operations
- Verify zero deadlocks
- Measure lock contention under load

### 5.4 Performance Validation
- Measure operations/sec under various loads
- Verify O(log n) complexity holds
- Compare with baseline ext4 performance
- Document realistic performance numbers

## Commands to Resume

```bash
# Mount the filesystem
mkdir -p /tmp/razorfs_mount
fusermount3 -u /tmp/razorfs_mount 2>/dev/null
/home/nico/WORK_ROOT/RAZOR_repo/fuse/razorfs_mt /tmp/razorfs_mount -f &

# Run basic tests
/home/nico/WORK_ROOT/RAZOR_repo/fuse/test_mt_fuse.sh

# Run stress tests
/home/nico/WORK_ROOT/RAZOR_repo/fuse/test_mt_stress.sh

# Rebuild if needed
cd /home/nico/WORK_ROOT/RAZOR_repo/fuse
make clean && make
```

## Key Files to Work On

**Phase 4 Implementation**:
- `fuse/razorfs_mt.c` - Add rename, enhance delete ops, add truncate
- `fuse/test_posix_mt.sh` - New test script for POSIX features

**Phase 5 Testing**:
- `tests/stability_24h.sh` - 24-hour stress test
- `tests/extreme_concurrency.sh` - 1000+ thread test
- `tests/memory_analysis.sh` - valgrind + ThreadSanitizer

## Known Limitations (By Design)

1. **16-child limit per directory** - NARY_BRANCHING_FACTOR = 16
   - This is intentional for O(log₁₆ n) performance
   - Directories with >16 children will return ENOSPC or EEXIST

2. **No persistence** - In-memory filesystem only
   - All data lost on unmount
   - This is intentional for simplicity

3. **Simple locking** - Per-node rwlocks, no RCU yet
   - Good enough for correctness
   - Can optimize later if needed

## Success Criteria

**Phase 4 (POSIX)**:
- [ ] rename() working correctly
- [ ] Proper error codes for all operations
- [ ] truncate() working for grow/shrink
- [ ] All POSIX tests passing

**Phase 5 (Stability)**:
- [ ] 24+ hour test with zero crashes
- [ ] Zero memory leaks (valgrind clean)
- [ ] Zero data races (ThreadSanitizer clean)
- [ ] Performance matches O(log n) guarantees
- [ ] Documentation matches reality

---
**Status**: Ready for Phase 4 implementation
**Next Action**: Add rename() to razorfs_mt.c
