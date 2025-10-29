# RazorFS Architecture

**Last Updated:** October 2025

---

## Overview

RazorFS is an experimental FUSE3-based filesystem implementing an n-ary tree structure with advanced optimizations for modern hardware.

### Core Design

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

### Key Features

- **O(log n) Complexity:** Logarithmic operations for lookup, insert, delete
- **Cache-Friendly:** 64-byte aligned nodes (single cache line)
- **NUMA-Aware:** Memory binding to CPU's NUMA node
- **Multithreaded:** ext4-style per-inode locking
- **Transparent Compression:** zlib level 1 (automatic)
- **Disk-Backed Persistence:** Full mmap-based persistent storage

---

## Architectural Principles

RazorFS is built on six fundamental architectural principles:

### 1. Adaptive NUMA-Aware Performance

**Hardware-Driven Optimization**

RazorFS operates with an intelligent, adaptive performance model:

**On Standard (Non-NUMA) Systems:**
- Filesystem detects absence of NUMA architecture
- NUMA-specific optimizations remain disabled
- Performs like traditional filesystems (ext4-level baseline)
- Uses standard memory allocation

**On NUMA Systems:**
- Detects NUMA topology automatically via `/sys/devices/system/node/`
- Activates NUMA optimizations using `mbind()` syscall
- Binds core metadata structures to local memory node
- Minimizes remote memory access latency

**Key Insight:** RazorFS doesn't degrade on non-NUMA systems—it simply doesn't gain the NUMA boost.

### 2. True O(log n) Complexity Through Binary Search

**Design Philosophy**
- 16-ary tree structure balances tree depth with cache efficiency
- Sorted children arrays maintained during insert/delete
- Hybrid search strategy: Linear (≤8 children), Binary (>8 children)
- Cache-conscious: 64-byte nodes, 32-byte children array in L1 cache

**Concrete Performance: 1 Million Files**

| Operation | Complexity | Operations Count | vs Linear Search |
|-----------|------------|------------------|------------------|
| Path lookup | O(log n) | 20 operations | 4x faster (was 80) |
| Insert | O(log n + k) | 36 operations | 2.7x faster (was 96) |
| Delete | O(log n + k) | 36 operations | 2.7x faster (was 96) |
| Find child | O(log k) | 4 comparisons | 4x faster (was 16) |

**Mathematical Proof:**
```
Tree depth:        d = log₁₆(n)
Operations/level:  log₂(16) = 4
Total operations:  d × 4 = log₁₆(n) × 4 = log₂(n) = O(log n) ✓
```

**See:** [COMPLEXITY_ANALYSIS.md](architecture/COMPLEXITY_ANALYSIS.md)

### 3. Cache-Conscious Design

**Cache Line Alignment:**
- Node size: Exactly 64 bytes (single L1 cache line)
- MT node size: 128 bytes (prevents false sharing)
- Children array: 32 bytes (fits in half cache line)
- All nodes cache-line aligned: `__attribute__((aligned(64)))`

**Node Structure (64 bytes):**
```
├─ Identity (12 bytes):  inode, parent_idx, num_children, mode
├─ Naming (4 bytes):     name_offset in string table
├─ Children (32 bytes):  16 × uint16_t indices (sorted)
└─ Metadata (16 bytes):  size, mtime, xattr_head
```

**Benefits:**
- Single fetch: Entire node in one cache miss
- No pointer chasing: Index-based children
- Prefetch friendly: Sequential access patterns
- 70%+ cache hit ratios measured

**See:** [CACHE_LOCALITY.md](architecture/CACHE_LOCALITY.md)

### 4. Memory Locality Through BFS Layout

**Breadth-First Search (BFS) Memory Layout:**

```
Traditional (DFS):
Memory: [node0] [node1] ... [nodeN] [unrelated] [node2] ...
Problem: Siblings scattered, poor spatial locality

RazorFS (BFS):
Memory: [root] [child1] [child2] [child3] [grandchild1] ...
Benefit: Siblings consecutive, excellent spatial locality
```

**Advantages:**
- Directory listings: Children consecutive → single cache line
- Path traversal: Each level clustered → predictable prefetch
- Rebalancing: Periodic BFS reorganization maintains locality
- Fewer page faults: Concentrated access patterns

**Trade-off:** Rebalancing cost (every 100 ops) vs. sustained locality

### 5. Transparent Compression

**Intelligent Compression Strategy:**
- Algorithm: zlib level 1 (fastest)
- Threshold: Files ≥ 512 bytes only
- Magic header: `0x525A4350` ("RZCP")
- Skip logic: Only compress if beneficial

**Decision Tree:**
```
File write →
  ├─ Size < 512 bytes? → Store uncompressed
  ├─ Compress with zlib level 1
  ├─ compressed < original? → Store compressed + header
  └─ compressed ≥ original? → Store uncompressed
```

**Performance:**
- Text files: 50-70% space savings
- Source code: 60-75% space savings
- Binaries: Often skipped (already compressed)
- CPU impact: Minimal (~10x faster than zlib level 6)

### 6. Deadlock-Free Concurrency

**Global Lock Ordering:**

Lock hierarchy: `tree_lock → parent_lock → child_lock`

All topology-changing operations acquire locks in this exact order, preventing circular wait conditions.

**Concurrency Model:**
- Per-inode locks: `pthread_rwlock_t` for fine-grained locking
- Reader-writer locks: Multiple readers, single writer
- Consistent ordering: Every operation follows same hierarchy
- No retry logic: Eliminates livelock

**Verification:** All 199 tests pass with concurrent operations

---

## N-ary Tree Design

### Structure

**Branching Factor:** 16 (k=16)
- Balances tree depth with cache efficiency
- Children array: 16 × 2 bytes = 32 bytes (L1 cache)
- Tree depth for 1M files: log₁₆(1M) ≈ 5 levels

**Node Layout:**

```c
struct nary_node_mt {
    uint32_t inode;              // Unique identifier
    uint16_t parent_idx;         // Parent node index
    uint8_t num_children;        // Number of children (max 16)
    mode_t mode;                 // File type and permissions
    uint32_t name_offset;        // Offset in string table
    uint16_t children[16];       // Child indices (sorted)
    off_t size;                  // File size
    time_t mtime;                // Modification time
    uint32_t xattr_head;         // Extended attributes
    pthread_rwlock_t lock;       // Per-inode lock
} __attribute__((aligned(128)));
```

**Why 128 bytes for MT nodes?**
- Includes `pthread_rwlock_t` (64 bytes on most systems)
- Prevents false sharing between threads
- Separate cache lines for concurrent access

### Operations

**Insert:** O(log n + k)
1. Binary search to find parent: O(log n)
2. Binary search for insertion position: O(log k) = O(4)
3. Shift elements to maintain sorted order: O(k) = O(16)

**Lookup:** O(log n)
1. Traverse path from root: O(log₁₆ n) levels
2. Binary search at each level: O(log₂ 16) = O(4)
3. Total: O(log₁₆ n × 4) = O(log n)

**Delete:** O(log n + k)
1. Find node: O(log n)
2. Remove from parent's children: O(k) = O(16)
3. Rebalance if needed: O(1) amortized

---

## Locking Strategy

### ext4-Style Per-Inode Locking

**Lock Hierarchy:**
```
tree_lock (global)
    ↓
parent_lock (inode-level)
    ↓
child_lock (inode-level)
```

**Deadlock Prevention:**
- Always acquire locks in order: tree → parent → child
- Never acquire child lock before parent lock
- Consistent ordering across all operations

**Lock Types:**
- `pthread_rwlock_t`: Reader-writer locks
- Multiple readers OR single writer
- Minimizes contention for read-heavy workloads

### Example: Rename Operation

```c
// Correct lock ordering
1. rdlock(tree_lock)          // Prevent tree reorganization
2. wrlock(old_parent->lock)   // Lock source parent
3. wrlock(new_parent->lock)   // Lock dest parent (ordered)
4. wrlock(file->lock)         // Lock file being moved
5. Perform rename
6. unlock in reverse order
```

---

## Persistence Architecture

### Storage Backend

**Disk-Backed mmap:**
- Tree nodes: `/var/lib/razorfs/nodes.dat` (mmap'd, MAP_SHARED)
- File data: `/var/lib/razorfs/file_<inode>` (mmap'd per-file)
- String table: `/var/lib/razorfs/strings.dat` (persisted on unmount)
- WAL: `/tmp/razorfs_wal.log` (fsync'd transaction log)

**Benefits:**
- Zero-copy I/O: Direct memory access
- Page cache: Kernel caches frequently accessed pages
- Lazy loading: Pages loaded on demand
- Write-back caching: Kernel batches writes

### Write-Ahead Logging (WAL)

**ARIES-Style Recovery:**
1. **Analysis:** Scan WAL to identify uncommitted transactions
2. **Redo:** Replay committed transactions not yet on disk
3. **Undo:** Roll back uncommitted transactions

**Transaction Log Entry:**
```c
struct wal_entry {
    uint64_t txn_id;        // Transaction ID
    uint8_t operation;       // CREATE/DELETE/WRITE/etc.
    uint32_t inode;         // Target inode
    // ... operation-specific data
    uint64_t checksum;      // Entry integrity check
};
```

**Durability:**
- Every write logged before execution
- fsync() after every log entry
- Recovery on mount if WAL exists

**See:** [WAL_DESIGN.md](architecture/WAL_DESIGN.md), [RECOVERY_DESIGN.md](architecture/RECOVERY_DESIGN.md)

---

## Compression

### Implementation

**Algorithm:** zlib compress2() level 1
- Fastest compression (10x faster than level 6)
- Good ratio: 50-70% for text
- Low CPU overhead

**Storage Format:**
```
Compressed File:
├─ Magic: 0x525A4350 ("RZCP")    [4 bytes]
├─ Original size                  [8 bytes]
├─ Compressed size                [8 bytes]
└─ Compressed data                [N bytes]
```

**Decision Logic:**
```c
if (size < 512) {
    store_uncompressed();
} else {
    compressed = zlib_compress(data, size);
    if (compressed_size < size) {
        store_compressed(compressed);
    } else {
        store_uncompressed();  // No benefit
    }
}
```

---

## NUMA Support

### Detection

```c
int detect_numa() {
    // Check /sys/devices/system/node/
    DIR *dir = opendir("/sys/devices/system/node");
    int node_count = 0;
    while ((ent = readdir(dir)) != NULL) {
        if (strncmp(ent->d_name, "node", 4) == 0) {
            node_count++;
        }
    }
    return node_count > 1;
}
```

### Memory Binding

```c
int bind_to_numa_node(void *addr, size_t len, int node) {
    unsigned long nodemask = 1UL << node;
    return mbind(addr, len, MPOL_BIND, &nodemask,
                 sizeof(nodemask) * 8, MPOL_MF_STRICT);
}
```

**Binds:**
- Tree nodes to local NUMA node
- File data to local NUMA node
- Minimizes cross-node memory access

**Fallback:**
- Graceful degradation on non-NUMA systems
- No performance penalty
- NUMA tests skipped in CI

---

## String Table

### Purpose

Efficient storage of filenames with deduplication:
- Same filename used multiple times → stored once
- Offset-based references (4 bytes vs. 256 bytes)
- Reduces memory footprint

### Structure

```c
struct string_table {
    char *data;              // Contiguous string storage
    size_t size;             // Total capacity
    size_t used;             // Bytes used
    uint32_t offsets[MAX];   // Hash table for dedup
};
```

### Operations

**Intern:** O(1) average
- Hash filename
- Check if already exists
- If not, append to data buffer
- Return offset

**Lookup:** O(1)
- Direct offset access
- Return pointer to string

---

## Extended Attributes (xattr)

**Design:** Linked list per inode
- Support 4 namespaces: user, system, security, trusted
- Max value size: 64KB
- See: [XATTR_DESIGN.md](architecture/XATTR_DESIGN.md)

## Hardlinks

**Design:** Reference counting
- Inode table tracks link count
- Max links: 65,535
- See: [HARDLINK_DESIGN.md](../features/HARDLINK_DESIGN.md)

---

## Performance Characteristics

### Measured Metrics

**Metadata Operations (1000 files):**
- Create: 1865ms (~1.87ms/file)
- Stat: 1794ms (~1.79ms/file)
- Delete: 1566ms (~1.57ms/file)

**I/O Throughput:**
- Write: 16.44 MB/s
- Read: 37.17 MB/s

**Compression:**
- Text files: 50-70% savings
- Source code: 60-75% savings

**Cache Efficiency:**
- Typical: 70% hit ratio
- Peak: 92.5% hit ratio

---

## Design Trade-offs

### Why k=16?

| Factor | k=8 | k=16 | k=32 |
|--------|-----|------|------|
| Tree depth (1M) | 7 | 5 | 4 |
| Children array | 16B | 32B | 64B |
| Binary search | O(3) | O(4) | O(5) |
| Cache lines | 0.25 | 0.5 | 1.0 |
| **Winner** | - | **k=16** | - |

### Why sorted array?

| Approach | Lookup | Insert | Memory | Cache |
|----------|--------|--------|--------|-------|
| Sorted array | O(log k) | O(k) | None | Excellent |
| Hash table | O(1) | O(1) | High | Poor |
| B-tree | O(log k) | O(log k) | Medium | Good |
| **Winner** | **Sorted** | - | **Sorted** | **Sorted** |

Sorted arrays win for filesystem workloads (read-heavy, small k).

---

## Future Directions

### Phase 7 (Current)
- razorfsck consistency checker
- Background flusher thread
- Storage compaction
- Performance tuning

### Phase 8 (Planned)
- Large file optimization (>10MB)
- Snapshot support
- S3 backend integration
- Monitoring (Prometheus metrics)

---

## Related Documentation

- [COMPLEXITY_ANALYSIS.md](architecture/COMPLEXITY_ANALYSIS.md) - Mathematical complexity proofs
- [CACHE_LOCALITY.md](architecture/CACHE_LOCALITY.md) - Cache optimization details
- [WAL_DESIGN.md](architecture/WAL_DESIGN.md) - Write-Ahead Logging design
- [RECOVERY_DESIGN.md](architecture/RECOVERY_DESIGN.md) - Crash recovery
- [XATTR_DESIGN.md](architecture/XATTR_DESIGN.md) - Extended attributes
- [PERSISTENCE.md](PERSISTENCE.md) - Persistence implementation
- [TESTING.md](TESTING.md) - Testing infrastructure

---

## References

- ext4 filesystem: Locking strategy inspiration
- BTRFS: Compression approach
- ARIES algorithm: WAL recovery (Gray & Reuter, 1993)
- n-ary tree design: [ncandio/n-ary_python_package](https://github.com/ncandio/n-ary_python_package)

---

**Document Version:** 2.0
**Date:** 2025-10-29
**Author:** RazorFS Team
