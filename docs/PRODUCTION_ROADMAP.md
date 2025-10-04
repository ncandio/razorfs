# RAZORFS Production Roadmap

## Overview
This document outlines the implementation plan to transform RAZORFS from a prototype to a production-ready filesystem. The roadmap is divided into 5 phases, each building upon the previous one.

**Current Status**: Phase 0 Complete ✓
- ✓ Core filesystem operations working
- ✓ Multithreading with per-inode locks
- ✓ Shared memory persistence
- ✓ Compression support
- ✓ String interning with hash table
- ✓ Security fixes (path traversal, race conditions)
- ✓ Memory alignment for cache efficiency
- ✓ Comprehensive test suite

**Remaining Work**: 6 Major Features across 5 Phases

---

## Phase 1: Write-Ahead Logging (WAL) 🔥 CRITICAL
**Goal**: Make the filesystem crash-safe with journaling
**Priority**: CRITICAL - Required for production use
**Estimated Complexity**: HIGH
**Files to Create/Modify**: 5-7 files

### 1.1 Design Document
**File**: `docs/WAL_DESIGN.md`
- WAL architecture overview
- Log entry format specification
- Transaction lifecycle
- Checkpoint strategy
- Recovery protocol

### 1.2 WAL Infrastructure
**Files**: `src/wal.h`, `src/wal.c`

**Key Data Structures**:
```c
struct wal_header {
    uint32_t magic;              // 'WLOG'
    uint32_t version;
    uint64_t next_tx_id;
    uint64_t checkpoint_lsn;     // Log Sequence Number
    uint64_t head_offset;        // Circular buffer head
    uint64_t tail_offset;        // Circular buffer tail
    uint32_t checksum;
};

struct wal_entry {
    uint64_t tx_id;              // Transaction ID
    uint64_t lsn;                // Log Sequence Number
    enum wal_op_type op_type;    // INSERT, DELETE, UPDATE, CHECKPOINT
    uint32_t data_len;
    uint64_t timestamp;
    uint32_t checksum;           // CRC32 of entry
    char data[];                 // Variable-length operation data
};

enum wal_op_type {
    WAL_OP_INSERT,
    WAL_OP_DELETE,
    WAL_OP_UPDATE,
    WAL_OP_CHECKPOINT,
    WAL_OP_COMMIT,
    WAL_OP_ABORT
};
```

**Functions to Implement**:
```c
int wal_init(struct wal *wal, void *shm_buf, size_t size);
int wal_begin_tx(struct wal *wal, uint64_t *tx_id);
int wal_log_insert(struct wal *wal, uint64_t tx_id, const struct wal_insert_data *data);
int wal_log_delete(struct wal *wal, uint64_t tx_id, const struct wal_delete_data *data);
int wal_log_update(struct wal *wal, uint64_t tx_id, const struct wal_update_data *data);
int wal_commit_tx(struct wal *wal, uint64_t tx_id);
int wal_abort_tx(struct wal *wal, uint64_t tx_id);
int wal_checkpoint(struct wal *wal);
int wal_destroy(struct wal *wal);
```

### 1.3 Integration with Filesystem Operations
**Files to Modify**: `fuse/razorfs_mt.c`, `src/nary_tree_mt.c`

**Changes**:
- Wrap all mutations in transactions
- Log before applying changes
- Add fsync support to force WAL flush
- Ensure atomic operations

**Example Integration**:
```c
// In razorfs_mt_create():
uint64_t tx_id;
wal_begin_tx(&fs->wal, &tx_id);

// Log the operation
struct wal_insert_data log_data = {
    .parent_inode = parent_inode,
    .name = name,
    .mode = mode
};
wal_log_insert(&fs->wal, tx_id, &log_data);

// Apply the operation
uint16_t idx = nary_insert_mt(...);

// Commit transaction
wal_commit_tx(&fs->wal, tx_id);
```

### 1.4 Testing
**File**: `tests/unit/wal_test.cpp`

**Test Cases**:
- Basic WAL operations (init, log, commit)
- Transaction isolation
- Circular buffer wraparound
- Checksum validation
- Concurrent logging from multiple threads
- WAL buffer full handling

**Commits for Phase 1**:
1. `docs: Add WAL design specification`
2. `feat: Implement WAL infrastructure and logging`
3. `feat: Integrate WAL with filesystem operations`
4. `test: Add comprehensive WAL unit tests`
5. `docs: Update architecture documentation with WAL`

---

## Phase 2: Crash Recovery 🔥 CRITICAL
**Goal**: Implement journal replay for crash safety
**Priority**: CRITICAL - Paired with Phase 1
**Estimated Complexity**: HIGH
**Files to Create/Modify**: 3-4 files

### 2.1 Recovery Design
**File**: `docs/RECOVERY_DESIGN.md`
- Recovery algorithm (ARIES-style)
- Three-phase recovery: Analysis, Redo, Undo
- Idempotency guarantees
- Partial transaction handling

### 2.2 Recovery Implementation
**Files**: `src/recovery.h`, `src/recovery.c`

**Key Functions**:
```c
int recovery_init(struct recovery_ctx *ctx, struct wal *wal, struct nary_tree_mt *tree);
int recovery_scan_log(struct recovery_ctx *ctx);
int recovery_redo_phase(struct recovery_ctx *ctx);
int recovery_undo_phase(struct recovery_ctx *ctx);
int recovery_run(struct recovery_ctx *ctx);
```

**Recovery Phases**:
1. **Analysis Phase**: Scan WAL to determine state
2. **Redo Phase**: Replay committed transactions
3. **Undo Phase**: Roll back uncommitted transactions

### 2.3 Mount-time Recovery
**Files to Modify**: `fuse/razorfs_mt.c`, `src/shm_persist.c`

**Changes**:
- Check WAL header on mount
- Detect unclean shutdown
- Run recovery before filesystem becomes available
- Mark filesystem as clean after recovery

### 2.4 Testing
**File**: `tests/unit/recovery_test.cpp`

**Test Cases**:
- Clean shutdown recovery (no-op)
- Single uncommitted transaction rollback
- Multiple committed transactions replay
- Partial transaction in log
- Corrupted log entry detection
- Recovery after simulated crash

**Integration Test**:
**File**: `tests/integration/crash_test.cpp`
- Simulate crashes at various points
- Verify data integrity after recovery
- Test concurrent operations during crash

**Commits for Phase 2**:
1. `docs: Add crash recovery design specification`
2. `feat: Implement three-phase recovery algorithm`
3. `feat: Integrate recovery with mount process`
4. `test: Add recovery unit and integration tests`
5. `docs: Add recovery troubleshooting guide`

---

## Phase 3: Extended Attributes (xattr) ⚡ HIGH PRIORITY
**Goal**: Support extended attributes for metadata
**Priority**: HIGH - Needed for SELinux, ACLs, user metadata
**Estimated Complexity**: MEDIUM
**Files to Create/Modify**: 4-5 files

### 3.1 Design Document
**File**: `docs/XATTR_DESIGN.md`
- xattr storage strategy
- Namespace support (user., trusted., security., system.)
- Size limits
- Performance considerations

### 3.2 Data Structures
**Files to Modify**: `src/nary_tree_mt.h`, `src/nary_node.h`

**Add to node**:
```c
struct xattr_entry {
    uint32_t name_offset;        // In string table
    uint32_t value_offset;       // In xattr value pool
    uint16_t value_len;
    uint8_t namespace;           // USER, TRUSTED, SECURITY, SYSTEM
    uint8_t flags;
};

struct nary_node {
    // ... existing fields ...
    uint32_t xattr_offset;       // Offset to xattr array in pool
    uint16_t xattr_count;        // Number of xattrs
    uint16_t xattr_capacity;     // Allocated capacity
};
```

### 3.3 xattr Pool Implementation
**Files**: `src/xattr_pool.h`, `src/xattr_pool.c`

**Functions**:
```c
int xattr_pool_init(struct xattr_pool *pool, size_t capacity);
int xattr_pool_set(struct xattr_pool *pool, uint16_t node_idx,
                   const char *name, const void *value, size_t size, int flags);
int xattr_pool_get(struct xattr_pool *pool, uint16_t node_idx,
                   const char *name, void *value, size_t size);
int xattr_pool_list(struct xattr_pool *pool, uint16_t node_idx,
                    char *list, size_t size);
int xattr_pool_remove(struct xattr_pool *pool, uint16_t node_idx,
                      const char *name);
```

### 3.4 FUSE Integration
**Files to Modify**: `fuse/razorfs_mt.c`

**Add FUSE operations**:
```c
static int razorfs_mt_setxattr(const char *path, const char *name,
                               const char *value, size_t size, int flags);
static int razorfs_mt_getxattr(const char *path, const char *name,
                               char *value, size_t size);
static int razorfs_mt_listxattr(const char *path, char *list, size_t size);
static int razorfs_mt_removexattr(const char *path, const char *name);
```

### 3.5 Testing
**File**: `tests/unit/xattr_test.cpp`

**Test Cases**:
- Set/get/remove xattrs
- Namespace handling
- Size limits
- Multiple xattrs per file
- xattr persistence across remount
- Concurrent xattr operations

**Commits for Phase 3**:
1. `docs: Add xattr design specification`
2. `feat: Implement xattr pool and storage`
3. `feat: Add FUSE xattr operations`
4. `test: Add comprehensive xattr tests`
5. `feat: Integrate xattr with WAL for crash safety`

---

## Phase 4: Hardlink Support ⚡ HIGH PRIORITY
**Goal**: Support multiple directory entries for same inode
**Priority**: HIGH - Common filesystem feature
**Estimated Complexity**: MEDIUM
**Files to Create/Modify**: 5-6 files

### 4.1 Design Document
**File**: `docs/HARDLINK_DESIGN.md`
- Inode reference counting
- Directory entry separation
- Link/unlink semantics
- Interaction with deletion

### 4.2 Architecture Changes
**Files to Modify**: `src/nary_tree_mt.h`, `src/nary_node.h`

**Major Change**: Separate inodes from directory entries

**Current Architecture**:
```
Node = Directory Entry + Inode (1:1)
```

**New Architecture**:
```
Directory Entry (dentry) -> Inode (N:1)
```

**New Data Structures**:
```c
struct inode_mt {
    uint32_t inode_num;          // Unique inode number
    uint32_t nlink;              // Reference count (link count)
    mode_t mode;
    uid_t uid;
    gid_t gid;
    size_t size;
    time_t atime, mtime, ctime;
    uint32_t xattr_offset;
    void *data;                  // File data pointer
    pthread_rwlock_t lock;
};

struct dentry {
    uint32_t name_offset;        // In string table
    uint32_t inode_num;          // Points to inode
    uint16_t parent_idx;
};

struct nary_node_mt {
    struct dentry dentry;
    uint16_t children[NARY_BRANCHING_FACTOR];
    uint16_t num_children;
    pthread_rwlock_t lock;
};
```

### 4.3 Inode Table Implementation
**Files**: `src/inode_table.h`, `src/inode_table.c`

**Functions**:
```c
int inode_table_init(struct inode_table *table, size_t capacity);
uint32_t inode_alloc(struct inode_table *table);
struct inode_mt* inode_get(struct inode_table *table, uint32_t inode);
int inode_link(struct inode_table *table, uint32_t inode);
int inode_unlink(struct inode_table *table, uint32_t inode);
int inode_table_destroy(struct inode_table *table);
```

### 4.4 FUSE Integration
**Files to Modify**: `fuse/razorfs_mt.c`

**Add FUSE operation**:
```c
static int razorfs_mt_link(const char *from, const char *to);
```

**Modify**:
```c
static int razorfs_mt_unlink(const char *path) {
    // Decrement link count
    // Only delete inode when nlink == 0
}
```

### 4.5 Testing
**File**: `tests/unit/hardlink_test.cpp`

**Test Cases**:
- Create hardlink
- Multiple hardlinks to same file
- stat() shows correct nlink count
- Modify through one link, see through another
- Delete one link, others remain valid
- Delete last link, inode freed
- Hardlink across directories

**Commits for Phase 4**:
1. `docs: Add hardlink design specification`
2. `refactor: Separate inodes from directory entries`
3. `feat: Implement inode table with reference counting`
4. `feat: Add link/unlink operations`
5. `test: Add comprehensive hardlink tests`
6. `feat: Integrate hardlinks with WAL`

---

## Phase 5: Large File Optimization + mmap 🚀 PERFORMANCE
**Goal**: Optimize for files >10MB and add mmap support
**Priority**: MEDIUM - Performance improvement
**Estimated Complexity**: HIGH
**Files to Create/Modify**: 6-8 files

### 5.1 Design Document
**File**: `docs/LARGE_FILE_DESIGN.md`
- Extent-based storage
- Block allocation strategy
- Sparse file support
- Read-ahead/write-behind
- mmap integration

### 5.2 Extent-Based Storage
**Files**: `src/extent.h`, `src/extent.c`

**Data Structures**:
```c
#define BLOCK_SIZE 4096

struct extent {
    uint64_t logical_offset;     // Offset in file
    uint64_t physical_offset;    // Offset in storage
    uint32_t length;             // In blocks
    uint32_t flags;              // COMPRESSED, SPARSE, etc.
};

struct extent_tree {
    struct extent *extents;      // Array of extents
    uint32_t count;
    uint32_t capacity;
};
```

**Functions**:
```c
int extent_tree_init(struct extent_tree *tree);
int extent_alloc(struct extent_tree *tree, uint64_t offset, uint32_t blocks);
struct extent* extent_find(struct extent_tree *tree, uint64_t offset);
int extent_split(struct extent_tree *tree, uint64_t offset);
int extent_merge(struct extent_tree *tree, uint32_t idx);
```

### 5.3 Block Allocator
**Files**: `src/block_alloc.h`, `src/block_alloc.c`

**Functions**:
```c
int block_alloc_init(struct block_allocator *alloc, size_t total_blocks);
uint64_t block_alloc(struct block_allocator *alloc, uint32_t count);
int block_free(struct block_allocator *alloc, uint64_t offset, uint32_t count);
```

### 5.4 Read-Ahead / Write-Behind
**Files**: `src/readahead.h`, `src/readahead.c`

**Strategy**:
- Sequential read detection
- Prefetch next N blocks
- Async write coalescing
- Flush on fsync/close

### 5.5 mmap Support
**Files to Modify**: `fuse/razorfs_mt.c`

**Add FUSE operations**:
```c
static int razorfs_mt_mmap(const char *path, struct fuse_file_info *fi,
                           void *addr, size_t length, off_t offset, int prot);
static int razorfs_mt_munmap(const char *path, struct fuse_file_info *fi,
                             void *addr, size_t length);
```

**Challenges**:
- Page cache integration
- Shared vs private mappings
- Dirty page tracking
- msync() support

### 5.6 Sparse File Support
**Implementation**:
- Detect zero-filled blocks
- Don't allocate storage for holes
- Return zeros on read of sparse regions

### 5.7 Testing
**Files**:
- `tests/unit/extent_test.cpp`
- `tests/unit/block_alloc_test.cpp`
- `tests/integration/large_file_test.cpp`
- `tests/integration/mmap_test.cpp`

**Test Cases**:
- Files >100MB
- Sequential/random I/O patterns
- Sparse file operations
- mmap read/write
- mmap concurrent access
- Extent tree operations

**Commits for Phase 5**:
1. `docs: Add large file and mmap design specifications`
2. `feat: Implement extent-based storage`
3. `feat: Add block allocator`
4. `feat: Implement read-ahead and write-behind`
5. `feat: Add mmap support`
6. `feat: Implement sparse file support`
7. `test: Add comprehensive large file tests`
8. `perf: Benchmark and optimize large file operations`

---

## Phase 6: Final Integration & Production Hardening 🎯
**Goal**: Polish and prepare for production deployment
**Priority**: CRITICAL
**Estimated Complexity**: MEDIUM

### 6.1 Performance Optimization
- Profile with perf/valgrind
- Optimize hot paths
- Reduce lock contention
- Add performance monitoring

### 6.2 Documentation
**Files to Create**:
- `docs/DEPLOYMENT_GUIDE.md`
- `docs/PERFORMANCE_TUNING.md`
- `docs/TROUBLESHOOTING.md`
- `docs/API_REFERENCE.md`

### 6.3 Production Checklist
- [ ] All tests passing (unit + integration + stress)
- [ ] Zero memory leaks (valgrind clean)
- [ ] Zero race conditions (helgrind clean)
- [ ] Performance benchmarks documented
- [ ] Crash recovery tested extensively
- [ ] Security audit complete
- [ ] Documentation complete
- [ ] Deployment guide ready

### 6.4 Final Commits
1. `docs: Add complete deployment guide`
2. `docs: Add performance tuning guide`
3. `perf: Final optimization pass`
4. `test: Add stress tests for production scenarios`
5. `chore: Prepare v1.0.0 release`

---

## Implementation Timeline (Estimated)

| Phase | Feature | Complexity | Est. Time | Priority |
|-------|---------|------------|-----------|----------|
| 1 | WAL | HIGH | 2-3 days | CRITICAL |
| 2 | Crash Recovery | HIGH | 2-3 days | CRITICAL |
| 3 | xattr Support | MEDIUM | 1-2 days | HIGH |
| 4 | Hardlinks | MEDIUM | 1-2 days | HIGH |
| 5 | Large Files + mmap | HIGH | 3-4 days | MEDIUM |
| 6 | Production Hardening | MEDIUM | 1-2 days | CRITICAL |

**Total Estimated Time**: 10-16 days of focused development

---

## Success Criteria

### Phase 1-2 (WAL + Recovery)
- ✓ All filesystem operations are transactional
- ✓ Crash at any point can be recovered
- ✓ No data loss for committed transactions
- ✓ Uncommitted transactions are rolled back
- ✓ Performance overhead <10%

### Phase 3 (xattr)
- ✓ All standard xattr operations work
- ✓ Compatible with standard tools (setfattr, getfattr)
- ✓ SELinux contexts can be stored

### Phase 4 (Hardlinks)
- ✓ Multiple hardlinks to same file work correctly
- ✓ Reference counting accurate
- ✓ Compatible with standard tools (ln, stat)

### Phase 5 (Large Files + mmap)
- ✓ Files >1GB supported efficiently
- ✓ mmap read/write works correctly
- ✓ Sparse files save space
- ✓ Performance competitive with ext4

### Phase 6 (Production)
- ✓ Zero critical bugs
- ✓ Complete documentation
- ✓ Ready for production deployment

---

## Notes

- Each phase builds on previous phases
- WAL must be integrated into all new features
- All changes must include tests
- All commits must pass CI checks
- Documentation updates required for each phase

**Start Date**: 2025-10-04
**Target Completion**: 2025-10-20
