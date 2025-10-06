# RAZORFS Persistence: Reality Check

**Last Updated**: 2025-10-06
**Status**: ⚠️ **CRITICAL CLARIFICATION NEEDED**

---

## Executive Summary

**Current State**: RAZORFS data **DOES NOT SURVIVE** power loss or system reboot.

The filesystem implements "shared memory persistence" which provides:
- ✅ **Survives**: Unmount/remount cycles
- ❌ **DOES NOT SURVIVE**: Power loss, crashes, or reboots

This document clarifies the confusion between "persistence" and "crash recovery."

---

## The Problem: /dev/shm is Volatile

### What is /dev/shm?

`/dev/shm` is a **tmpfs** (temporary filesystem) that:
- Lives in RAM
- Is mounted as a ramdisk
- **Cleared on every reboot**
- Faster than disk but volatile

### RAZORFS Current Architecture

```
┌─────────────────────────────────────────────────────────┐
│  RAZORFS (FUSE3)                                        │
│                                                         │
│  Tree Nodes    → /dev/shm/razorfs_nodes     (131KB)    │
│  String Table  → /dev/shm/razorfs_strings   (1MB)      │
│  File Data     → /dev/shm/razorfs_file_*    (varies)   │
└─────────────────────────────────────────────────────────┘
                            ↓
                    RAM (tmpfs - VOLATILE)
                            ↓
                ⚡ POWER LOSS / REBOOT
                            ↓
                    ❌ ALL DATA LOST
```

---

## Test Comparison: Misleading vs Reality

### ❌ Current Test (test_persistence.sh) - MISLEADING

```bash
1. Mount RAZORFS
2. Create files
3. kill + fusermount3 -u    # ← Graceful unmount
4. Remount RAZORFS
5. ✓ Files still there      # ← /dev/shm NOT cleared
```

**Why it's misleading**: `/dev/shm` persists across unmount/remount because the **system didn't reboot**.

### ✅ New Test (test_crash_simulation.sh) - REALISTIC

```bash
1. Mount RAZORFS
2. Create files
3. kill -9 (SIGKILL)        # ← Simulates crash
4. rm -f /dev/shm/razorfs*  # ← Simulates reboot clearing tmpfs
5. Remount RAZORFS
6. ✗ Files GONE             # ← Real power loss behavior
```

**Why it's accurate**: Simulates what actually happens during power loss.

---

## WAL/Recovery Status: Implemented But Not Integrated

### What Exists ✅

RAZORFS has **fully implemented** WAL and crash recovery:

| Component | Status | Location | Tests |
|-----------|--------|----------|-------|
| Write-Ahead Log | ✅ Complete | `src/wal.c` | 22/22 passing |
| Crash Recovery | ✅ Complete | `src/recovery.c` | 13/13 passing |
| ARIES Algorithm | ✅ Complete | Analysis/Redo/Undo | Verified |

### What's Missing ❌

```diff
- ❌ WAL not called from FUSE operations
- ❌ Recovery not called on mount
- ❌ WAL stored in /dev/shm (should be on disk)
- ❌ No persistent backend configured
```

### Integration Gaps

**File: `fuse/razorfs_mt.c`**
```c
// Current code:
static int razorfs_write(...) {
    // Write data to memory
    // ❌ NO WAL LOGGING
    return bytes_written;
}

// Should be:
static int razorfs_write(...) {
    // ✅ Log operation to WAL
    wal_log_write(wal, inode, offset, data, size);

    // Write data to memory
    // ...

    // ✅ Commit transaction
    wal_commit(wal, txn_id);
    return bytes_written;
}
```

**File: `fuse/razorfs_mt.c` (mount)**
```c
// Current code:
static void *razorfs_init(...) {
    shm_tree_init(&fs.tree);  // Attach to /dev/shm
    // ❌ NO RECOVERY ATTEMPT
    return &fs;
}

// Should be:
static void *razorfs_init(...) {
    shm_tree_init(&fs.tree);

    // ✅ Attempt recovery from WAL
    if (wal_exists()) {
        recovery_run(&fs.tree, wal);
    }

    return &fs;
}
```

---

## Persistence Levels Comparison

### Level 0: No Persistence (Current Default Without /dev/shm)
- ❌ Data lost on unmount
- Use case: None (not useful)

### Level 1: Shared Memory "Persistence" (Current RAZORFS)
- ✅ Survives unmount/remount
- ❌ Lost on reboot/crash
- Use case: Testing only

### Level 2: WAL with Disk Storage (Partially Implemented)
- ✅ Survives crash (with recovery)
- ✅ Survives reboot (with recovery)
- ⚠️ Requires integration work
- Use case: Development/staging

### Level 3: Full Disk-Backed Persistence (Not Implemented)
- ✅ True persistence
- ✅ Production ready
- ❌ Requires new backend
- Use case: Production

---

## How to Achieve True Persistence

### Option A: Quick Fix (1-2 weeks)

**Integrate existing WAL/Recovery**

1. **Move WAL to disk**
   ```c
   #define WAL_FILE_PATH "/var/lib/razorfs/wal.log"
   // Instead of /dev/shm/razorfs_wal
   ```

2. **Call WAL from FUSE operations**
   - Wrap all writes in WAL transactions
   - Log metadata changes (create, delete, rename)

3. **Run recovery on mount**
   - Check for dirty WAL on startup
   - Replay uncommitted operations

**Limitations**:
- Still uses /dev/shm for data (volatile)
- Only metadata survives crashes
- Not production-grade

### Option B: Proper Solution (1-2 months)

**Implement disk-backed storage**

1. **Block device backend**
   - Replace /dev/shm with file-backed mmap
   - Store data in `/var/lib/razorfs/data.img`

2. **WAL for all operations**
   - Log every write before applying
   - Fsync WAL before acknowledging

3. **Recovery on mount**
   - Scan block device
   - Replay WAL if dirty
   - Verify checksums

**Result**: True production-ready persistence

### Option C: Hybrid Approach (2-3 weeks)

**RAM cache + Disk backing**

1. **Hot data in RAM** (/dev/shm for active files)
2. **Cold data on disk** (background flush)
3. **WAL on disk** (all writes logged)
4. **Recovery on mount** (rebuild RAM cache)

**Benefits**:
- Best performance (RAM cache)
- True persistence (disk backing)
- Crash safe (WAL)

---

## Testing Methodology

### Test Types Needed

| Test | Current | Should Be |
|------|---------|-----------|
| **Unmount/Remount** | ✅ `test_persistence.sh` | Keep (but rename) |
| **Crash Recovery** | ❌ None | ✅ `test_crash_simulation.sh` |
| **Power Loss** | ❌ None | ✅ VM snapshot + kill |
| **Reboot Survival** | ❌ None | ✅ Container restart test |

### Running the Crash Test

```bash
# Build RAZORFS
make clean && make

# Run crash simulation
./test_crash_simulation.sh

# Expected output (current implementation):
# ✗ FAIL: Data lost after crash (as expected)
```

### What Success Looks Like

After WAL integration:
```bash
./test_crash_simulation.sh

# Expected output:
# ✓ SUCCESS: Data survived crash!
# WAL recovery is working correctly
```

---

## Documentation Fixes Needed

### README.md

**Current (MISLEADING)**:
```markdown
#### ✅ Data Features
- **Persistence:** Shared memory storage (/dev/shm)
  - Survives unmount/remount
```

**Should Say**:
```markdown
#### ⚠️ Persistence Status
- **Current:** Shared memory storage (/dev/shm)
  - ✅ Survives: unmount/remount
  - ❌ DOES NOT SURVIVE: power loss/reboot
  - ⚠️ NOT SUITABLE FOR PRODUCTION

- **Future:** WAL-based crash recovery (implemented, not integrated)
  - When integrated: will survive crashes
  - Requires persistent WAL storage
```

### STATUS.md

**Update limitations section**:
```markdown
### Current Limitations
- ❌ **NO CRASH RECOVERY** - data lost on power failure
- ⚠️ /dev/shm is volatile (cleared on reboot)
- ⚠️ WAL implemented but not integrated
- ❌ Not production-ready for critical data
```

---

## Recommended Next Steps

### Immediate (This Week)
1. ✅ Create crash simulation test (DONE)
2. ⏳ Update documentation to clarify persistence limitations
3. ⏳ Add warning banner to README

### Short-term (1-2 Weeks)
1. Move WAL to disk storage
2. Integrate WAL into FUSE write operations
3. Add recovery call on mount
4. Test crash recovery works

### Medium-term (1-2 Months)
1. Implement disk-backed storage
2. Add hybrid RAM/disk caching
3. Comprehensive crash testing
4. Production hardening

---

## FAQ

### Q: Does RAZORFS have persistence?
**A**: Yes, but only across unmount/remount. Data **DOES NOT SURVIVE** power loss or reboot.

### Q: Is the WAL implemented?
**A**: Yes, fully implemented with 22 passing tests. But **NOT INTEGRATED** into filesystem operations.

### Q: Can I use this for production?
**A**: **NO**. Data loss on power failure makes it unsuitable for any important data.

### Q: How do I test real crash recovery?
**A**: Run `./test_crash_simulation.sh` (not `test_persistence.sh`)

### Q: When will crash recovery work?
**A**: After WAL integration (1-2 weeks of work).

### Q: Why wasn't this caught earlier?
**A**: The test (`test_persistence.sh`) was misleading - it tested unmount/remount, not crash/reboot.

---

## Conclusion

RAZORFS is an impressive technical achievement with:
- ✅ Excellent tree data structure
- ✅ Good multithreading
- ✅ Complete WAL/recovery implementation
- ✅ Comprehensive test suite

**However**, it currently has:
- ❌ **NO CRASH RECOVERY** (WAL not integrated)
- ❌ **VOLATILE STORAGE** (/dev/shm cleared on reboot)
- ⚠️ **MISLEADING TESTS** (test_persistence.sh doesn't test crashes)

**Bottom line**: Great for research and learning, **NOT READY** for any data you care about keeping.

---

**For questions or to discuss integration plan, see `docs/WAL_DESIGN.md` and `docs/RECOVERY_DESIGN.md`**
