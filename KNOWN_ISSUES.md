# RAZORFS Known Issues and Technical Debt

**Last Updated**: October 2, 2025
**Version**: 0.9.0-alpha

## 🔴 Critical Issues

### 1. Misleading Complexity Claims

**Issue**: The README and documentation claim O(log n) complexity for file operations, but the actual implementation has different characteristics.

**Current Reality**:
- File path lookup is **O(depth)** where depth = number of components in the path
- Example: `/a/b/c/file.txt` requires 4 lookups (root → a → b → c → file.txt)
- Each directory lookup is O(1) via hash table (cache_optimized_filesystem.hpp:320-355)
- This is NOT O(log n) relative to total number of files in the filesystem

**Code Reference**:
```cpp
// src/cache_optimized_filesystem.hpp:357-375
// O(depth) path lookup with O(1) per directory
CacheOptimizedNode* find_by_path(const std::string& path) {
    // ... splits path and iterates through components
    for (const std::string& component : components) {
        current = find_child_optimized(current, component);
    }
}
```

**Impact**:
- Performance is proportional to directory depth, not total file count
- Misleading benchmarks and documentation

**Fix Required**:
- Update all documentation to accurately describe O(depth) complexity
- Clarify that this is O(1) per directory via hash tables
- Remove or correct O(log n) claims from README

**Status**: Documentation update in progress

---

### 2. Crash-Safety Not Implemented

**Issue**: The README claims "crash-safe journaling" and "production-ready data integrity," but journaling is a stub implementation.

**Current Reality**:
```cpp
// src/razorfs_persistence.cpp:61-64
// NOTE: Journaling functions are not used in this simplified persistence model.
bool Journal::write_entry(...) { return true; }  // STUB!
bool Journal::replay_journal(...) { return true; }  // STUB!
bool Journal::checkpoint() { return true; }  // STUB!
```

**Problems**:
1. No actual journal entries are written
2. No WAL (Write-Ahead Logging)
3. No atomic operations
4. Power failure during write → corrupted filesystem
5. No recovery mechanism on mount

**Code Reference**:
- `src/razorfs_persistence.cpp:62-64` - Stub implementations
- `src/razorfs_persistence.cpp:143-150` - Non-atomic save operation

**Impact**:
- **CRITICAL**: Data loss on power failure or crash
- Filesystem corruption risk
- Not safe for any real-world use

**Fix Required**:
1. Implement proper WAL journaling:
   - Write journal entry before modifying data
   - fsync() journal
   - Apply changes
   - Mark journal entry complete
2. Implement journal replay on mount
3. Add checksums for integrity verification
4. Implement atomic two-phase commit

**Status**: High priority, planned for v1.0.0-beta

---

### 3. Inefficient I/O for Large Files

**Issue**: Read and write operations are inefficient and may not scale to large files.

**Current Reality**:
- Block-based I/O is implemented (4KB blocks)
- However, some code paths still operate on entire file contents
- Memory allocation for full file content in some operations

**Code Reference**:
```cpp
// fuse/razorfs_fuse.cpp:137-171
int read_blocks(uint64_t inode, char* buf, size_t size, off_t offset) {
    // Decompresses entire block even for partial reads
    std::string block_data = block.compressed ?
        EnhancedCompressionEngine::decompress_block(block.data, block.original_size) :
        block.data;
}
```

**Problems**:
1. Decompressing entire 4KB block for 1-byte read
2. Memory overhead for compressed blocks
3. No read-ahead or caching strategy
4. Write amplification with compression

**Impact**:
- Poor performance for large files (>100MB)
- Excessive memory usage
- Slow random access patterns

**Fix Required**:
1. Implement partial block decompression (if possible with zlib)
2. Add block-level caching
3. Implement read-ahead for sequential access
4. Optimize write coalescing

**Status**: Planned for v1.1

---

### 4. Coarse-Grained Locking

**Issue**: A single mutex/lock protects large portions of the filesystem, creating contention.

**Current Reality**:
```cpp
// src/cache_optimized_filesystem.hpp:288-289
mutable std::shared_mutex tree_mutex_;  // Protects entire tree

// Used in almost every operation:
std::unique_lock<std::shared_mutex> lock(tree_mutex_);
```

**Problems**:
1. Read operations block other reads unnecessarily in some paths
2. Write operations block entire filesystem
3. Poor scaling with concurrent access
4. Particularly bad for multi-core systems

**Impact**:
- Severe performance bottleneck under concurrent load
- NUMA performance claims are undermined by serialization
- Multi-threaded benchmarks will show poor scaling

**Fix Required**:
1. Implement per-directory locking
2. Use lock-free data structures where possible (hash tables)
3. Implement RCU (Read-Copy-Update) for read-heavy workloads
4. Add per-inode locks for file content

**Status**: Planned for v1.1

---

### 5. Data-Loss Bug in Persistence

**Issue**: Critical bug in persistence reload mechanism leads to data loss.

**Current Reality**:
The exact bug needs to be identified through detailed code review, but likely related to:

1. **String table reconstruction issue**:
   - Offsets may not be preserved correctly on reload
   - String deduplication map not rebuilt properly

2. **Inode map reconstruction**:
   - Parent-child relationships may be corrupted
   - Hash table state not saved/restored

3. **Missing data synchronization**:
   - File content not flushed before saving metadata
   - Blocks not persisted correctly

**Code Reference**:
- `src/razorfs_persistence.cpp:143-287` - Save/load implementation
- Likely in `load_from_file()` function

**Impact**:
- **CRITICAL**: Data loss after filesystem remount
- Corruption of directory structure
- Loss of file content

**Fix Required**:
1. Add comprehensive persistence tests
2. Verify string table offset consistency
3. Ensure all data structures are correctly serialized
4. Add validation checksums
5. Implement crash recovery tests

**Status**: **URGENT** - Blocking release

---

## 🟡 Medium Priority Issues

### 6. Incomplete POSIX Compliance

**Missing**:
- Extended attributes (xattr)
- Hard links
- Symbolic link handling is basic
- No file locking (flock, fcntl)
- No mmap support

**Status**: Planned for v1.5

---

### 7. Memory Management Issues

**Problems**:
- No limits on string table growth
- No eviction policy for blocks
- Unbounded memory usage with many small files
- No memory pressure handling

**Status**: Planned for v1.1

---

### 8. Performance Testing Validity

**Issue**: Current performance tests may not reflect real-world scenarios:
- Tests use small files (mostly < 1MB)
- No concurrent access testing
- No large file tests (>1GB)
- Cache-warm benchmarks (not cold-start)

**Status**: Test suite improvements planned

---

## 📊 Technical Debt Summary

| Issue | Severity | Status | Target Version |
|-------|----------|--------|----------------|
| Misleading complexity claims | High | In Progress | v0.9.1 |
| No crash safety | **Critical** | Planned | v1.0.0-beta |
| Inefficient I/O | High | Planned | v1.1 |
| Coarse-grained locking | High | Planned | v1.1 |
| Data-loss bug | **Critical** | Urgent | v0.9.1 |
| Incomplete POSIX | Medium | Planned | v1.5 |
| Memory management | Medium | Planned | v1.1 |
| Test coverage | Medium | Ongoing | v1.0 |

---

## 🔧 Immediate Action Items

1. **Fix README documentation** (This week)
   - Correct complexity claims
   - Add "Known Issues" section
   - Downgrade version to 0.9.0-alpha

2. **Identify and fix persistence bug** (Priority 1)
   - Add persistence round-trip tests
   - Fix data reconstruction logic

3. **Implement basic journaling** (Priority 2)
   - Start with simple WAL
   - Add fsync() calls
   - Implement replay on mount

4. **Improve locking** (Priority 3)
   - Start with per-directory locks
   - Profile contention points

---

## 📝 Notes

This document is a living record of known technical debt. All issues should be:
1. Tracked in GitHub issues
2. Referenced in code comments where relevant
3. Updated as fixes are implemented

**Transparency is critical** - these issues are documented openly to:
- Set realistic expectations
- Guide development priorities
- Enable community contributions
- Maintain project integrity
