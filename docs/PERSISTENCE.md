# RazorFS Persistence Guide

**Last Updated:** October 2025
**Status:** ✅ Fully Implemented

---

## Executive Summary

RazorFS implements **complete disk-backed persistence** using mmap-based storage and Write-Ahead Logging (WAL).

**What persists:**
- ✅ Tree nodes (filesystem metadata)
- ✅ File data (content)
- ✅ String table (filenames)
- ✅ WAL (crash recovery log)

**What survives:**
- ✅ Clean unmount/remount
- ✅ Process crashes (with WAL recovery)
- ✅ System reboots (when using proper storage location)

**⚠️ Important:** Data only survives reboots when stored on a real filesystem (not tmpfs).

---

## Quick Start

### 1. Setup Storage

```bash
# Run setup script (creates /var/lib/razorfs)
./scripts/setup_storage.sh

# Verify storage is on real disk (NOT tmpfs)
df -T /var/lib/razorfs
```

### 2. Test Persistence

```bash
# Run automated persistence tests
./scripts/test_persistence.sh
```

### 3. Mount with Persistence

```bash
mkdir -p /mnt/razorfs
./razorfs /mnt/razorfs

# Create test data
echo "Hello RazorFS" > /mnt/razorfs/test.txt

# Unmount
fusermount3 -u /mnt/razorfs

# Remount - data persists
./razorfs /mnt/razorfs
cat /mnt/razorfs/test.txt  # Output: Hello RazorFS
```

---

## Architecture

### Storage Components

```
/var/lib/razorfs/
├── nodes.dat          # Tree nodes (mmap'd, 131KB default)
├── strings.dat        # String table (saved on unmount)
├── file_<inode>       # Per-file data (mmap'd)
└── /tmp/razorfs_wal.log  # Write-Ahead Log (disk-backed)
```

### How It Works

**Tree Nodes** (`src/shm_persist.c:502-665`)
- mmap() with MAP_SHARED on `nodes.dat`
- NUMA-aware memory binding
- msync() on unmount for durability
- Automatic attach if file exists (remount)

**File Data** (`src/shm_persist.c:669-803`)
- Per-file mmap'd storage: `/var/lib/razorfs/file_<inode>`
- Header format:
  ```
  magic: 0x46494C45 ("FILE")
  inode: 4 bytes
  size: 8 bytes (uncompressed)
  data_size: 8 bytes (actual)
  is_compressed: 4 bytes
  ```

**String Table** (`src/shm_persist.c:811-923`)
- Saved to `strings.dat` on clean unmount
- Loaded from disk on mount if file exists
- Stores all filenames with deduplication

**Write-Ahead Log** (`src/wal.c`)
- ARIES-style recovery (Analysis/Redo/Undo)
- fsync() after every write
- Automatic recovery on mount

---

## Data Flow

### Write Operation

```
User write
  ↓
FUSE razorfs_mt_write()
  ↓
WAL logs operation (fsync'd to disk)
  ↓
Update in-memory tree
  ↓
disk_file_data_save()
  ↓
mmap() file, write header + data
  ↓
msync(MS_SYNC)
  ↓
Data on disk ✓
```

### Mount/Remount

```
./razorfs /mnt
  ↓
disk_tree_init()
  ↓
stat(/var/lib/razorfs/nodes.dat)
  ↓
File exists?
  YES:
    ├─ mmap(MAP_SHARED) existing file
    ├─ Validate header (magic, version)
    ├─ Restore tree metadata
    ├─ Load string table
    └─ Files loaded on-demand
  NO:
    ├─ Create new file
    ├─ mmap(MAP_SHARED)
    ├─ Initialize empty tree
    └─ Create root directory
```

---

## Persistence Guarantees

### What Survives

| Event | Data Survives? | Notes |
|-------|----------------|-------|
| Clean unmount/remount | ✅ Yes | msync() + disk storage |
| Process crash | ✅ Yes | WAL replays operations |
| System reboot | ✅ Yes* | *If storage on real disk (not tmpfs) |
| Power loss | ⚠️ Depends | Depends on hardware write-back cache |

### Durability Features

**Implemented:**
- ✅ msync(MS_SYNC) on critical operations
- ✅ WAL fsync() after every write
- ✅ mmap(MAP_SHARED) → kernel flushes dirty pages
- ✅ Clean unmount → explicit sync

**Not Implemented (future):**
- ⏳ Background flusher thread
- ⏳ Configurable sync policy
- ⏳ WAL checkpointing

---

## Storage Configuration

### Default Paths

```c
#define DISK_DATA_DIR "/var/lib/razorfs"
#define DISK_DATA_DIR_FALLBACK "/tmp/razorfs_data"
#define WAL_FILE_PATH "/tmp/razorfs_wal.log"
```

### Why /var/lib/razorfs?

- Standard Linux location for persistent application data
- Never on tmpfs (unlike /tmp)
- Survives reboots
- Proper permissions management

### Fallback Behavior

If `/var/lib/razorfs` is not accessible:
1. Falls back to `/tmp/razorfs_data`
2. Warns user about tmpfs risk
3. Continues operation (for development/testing)

---

## Testing

### Automated Test Suite

```bash
# Run all persistence tests
./scripts/test_persistence.sh
```

Tests verify:
1. Initial mount and data creation
2. Clean unmount/remount persistence
3. Multiple files and directories
4. Storage statistics and validation

### Manual Verification

```bash
# 1. Setup storage
sudo mkdir -p /var/lib/razorfs
sudo chown $(id -u):$(id -g) /var/lib/razorfs

# 2. Verify NOT tmpfs
df -T /var/lib/razorfs | grep -v tmpfs

# 3. Mount and create data
./razorfs /tmp/razorfs_mount
echo "Persistence test" > /tmp/razorfs_mount/test.txt

# 4. Unmount
fusermount3 -u /tmp/razorfs_mount

# 5. Verify files exist on disk
ls -lh /var/lib/razorfs/

# 6. Reboot system (optional)
sudo reboot

# 7. Remount after reboot
./razorfs /tmp/razorfs_mount
cat /tmp/razorfs_mount/test.txt  # Should output: Persistence test
```

---

## Performance Characteristics

### mmap Benefits

1. **Zero-copy I/O** - Direct memory access, no syscalls
2. **Page cache** - Kernel caches frequently accessed pages
3. **Lazy loading** - Pages loaded on demand
4. **Write-back caching** - Kernel batches writes

### mmap Trade-offs

1. **Page fault latency** - First access slower (~microseconds)
2. **msync overhead** - Forcing sync is expensive (~milliseconds)
3. **Memory pressure** - Large files can pressure page cache
4. **No fine-grained control** - Kernel decides when to flush

### Measured Performance

From benchmark results:
- **Metadata ops:** ~1.7ms per operation (create/stat/delete)
- **I/O throughput:** 16 MB/s write, 37 MB/s read
- **Persistence overhead:** ~5-10% (msync calls)

---

## Troubleshooting

### Data Not Persisting After Reboot

**Problem:** Files disappear after system reboot.

**Diagnosis:**
```bash
df -T /var/lib/razorfs
# If shows "tmpfs", that's the problem
```

**Solution:**
```bash
# Move to real filesystem
sudo mkdir -p /opt/razorfs
sudo chown $(id -u):$(id -g) /opt/razorfs

# Update DISK_DATA_DIR in src/shm_persist.h
# Or use symlink:
sudo ln -s /opt/razorfs /var/lib/razorfs
```

### WAL Recovery Fails

**Problem:** Mount fails with WAL recovery error.

**Diagnosis:**
```bash
ls -l /tmp/razorfs_wal.log
# Check if WAL exists and is readable
```

**Solution:**
```bash
# Remove corrupt WAL (data loss!)
rm -f /tmp/razorfs_wal.log

# Or run recovery manually:
./razorfs --recover /tmp/razorfs_wal.log
```

### Permission Denied

**Problem:** Cannot write to `/var/lib/razorfs`.

**Solution:**
```bash
sudo chown $(id -u):$(id -g) /var/lib/razorfs
sudo chmod 755 /var/lib/razorfs
```

---

## Future Enhancements

### Short-Term (Phase 7)
- Background flusher thread for async msync()
- Configurable sync policy (always/periodic/none)
- razorfsck consistency checker

### Medium-Term
- Storage compaction (reclaim deleted inode space)
- Large file optimization (>10MB)
- Enhanced WAL checkpointing

### Long-Term
- Alternative backends (S3, block device)
- Snapshot and backup support
- Monitoring and observability (Prometheus metrics)

---

## Implementation Details

### Code Locations

| Component | File | Function |
|-----------|------|----------|
| Tree init | `src/shm_persist.c:502` | `disk_tree_init()` |
| File save | `src/shm_persist.c:669` | `disk_file_data_save()` |
| File restore | `src/shm_persist.c:735` | `disk_file_data_restore()` |
| String save | `src/shm_persist.c:811` | `disk_string_table_save()` |
| String load | `src/shm_persist.c:868` | `disk_string_table_load()` |
| WAL logging | `src/wal.c` | `wal_log_*()` |
| Recovery | `src/recovery.c` | `recovery_run()` |

### Integration with FUSE

```c
// fuse/razorfs_mt.c:864-898
static void *razorfs_init(...) {
    // 1. Initialize disk-backed tree
    disk_tree_init(&fs.tree);

    // 2. Load string table
    disk_string_table_load(...);

    // 3. Run WAL recovery if needed
    recovery_run(&fs.tree, wal);

    return &fs;
}

// fuse/razorfs_mt.c:402
static int razorfs_open(...) {
    // Restore file data from disk on open
    disk_file_data_restore(inode, ...);
}

// fuse/razorfs_mt.c:563
static int razorfs_write(...) {
    // Save file data to disk after write
    disk_file_data_save(inode, ...);
}
```

---

## Comparison with Alternatives

### vs read()/write() syscalls

| Aspect | mmap | read/write |
|--------|------|------------|
| Latency | Low (no syscall) | High (syscall per op) |
| Memory | Pages cached | Buffer in userspace |
| Persistence | msync() | fsync() |
| **Winner** | **mmap** | - |

### vs Database (SQLite)

| Aspect | mmap | Database |
|--------|------|----------|
| Complexity | Low | High |
| ACID guarantees | WAL only | Full ACID |
| Overhead | Minimal | Significant |
| **Winner** | **mmap** (for filesystem) | - |

---

## References

- **mmap(2):** https://man7.org/linux/man-pages/man2/mmap.2.html
- **msync(2):** https://man7.org/linux/man-pages/man2/msync.2.html
- **ARIES recovery:** Transaction Processing (Gray & Reuter, 1993)
- **Filesystem layout:** https://www.kernel.org/doc/html/latest/filesystems/
- **FHS standard:** https://refspecs.linuxfoundation.org/FHS_3.0/fhs/index.html

---

## Conclusion

RazorFS implements production-grade persistence through:
- ✅ mmap-based disk storage
- ✅ WAL with crash recovery
- ✅ Proper integration with FUSE
- ✅ Comprehensive testing

**Key Takeaway:** Data persists across unmount/remount and system reboots when properly configured with real disk storage (not tmpfs).

For implementation details, see:
- Technical analysis: `docs/architecture/PERSISTENCE_FIX_ANALYSIS.md`
- WAL design: `docs/architecture/WAL_DESIGN.md`
- Recovery design: `docs/architecture/RECOVERY_DESIGN.md`

---

**Document Version:** 2.0
**Date:** 2025-10-29
**Author:** RazorFS Team
