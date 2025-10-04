# Large File Support Design

**Phase**: 5
**Status**: In Progress
**Estimated Time**: 3-4 days

---

## Overview

This design document specifies support for large files (>10MB) in RAZORFS through extent-based storage, block allocation, and sparse file support. Currently, files are limited by inline storage (32 bytes in inode). This phase adds scalable file storage.

---

## Requirements

### Functional Requirements
- **FR1**: Support files up to 1TB
- **FR2**: Efficient storage for large files via extents
- **FR3**: Sparse file support (holes don't consume space)
- **FR4**: Block allocator with free space management
- **FR5**: Read-ahead and write-behind caching
- **FR6**: mmap() support for memory-mapped I/O

### Non-Functional Requirements
- **NFR1**: O(log n) block lookup via extent tree
- **NFR2**: Minimize fragmentation
- **NFR3**: Efficient space reclamation
- **NFR4**: Compatible with existing WAL/recovery system

---

## Current Limitations

### Inline Storage Only

Currently, inodes store only 32 bytes of inline data:

```c
struct razorfs_inode {
    // ... metadata ...
    uint8_t data[32];  // Only 32 bytes!
};
```

**Issues**:
- Maximum file size: 32 bytes
- No support for large files
- Wasted space for empty files

---

## New Architecture

### Extent-Based Storage

```
┌─────────────────────────────────────────────────────────────┐
│                    RAZORFS Storage Layout                    │
├─────────────────────────────────────────────────────────────┤
│  Inode (64 bytes)                                            │
│  ┌────────────────────────────────────────────────────┐    │
│  │ size | mode | timestamps                          │    │
│  │ extents[4] (inline extent descriptors)            │    │
│  └────────────────────────────────────────────────────┘    │
├─────────────────────────────────────────────────────────────┤
│  Block Allocator                                             │
│  ┌────────────────────────────────────────────────────┐    │
│  │ Bitmap: tracks free/used blocks                   │    │
│  │ Block size: 4KB (default)                         │    │
│  └────────────────────────────────────────────────────┘    │
├─────────────────────────────────────────────────────────────┤
│  Data Blocks (4KB each)                                      │
│  ┌────────────────────────────────────────────────────┐    │
│  │ Block 0: [file data]                              │    │
│  │ Block 1: [file data]                              │    │
│  │ Block N: [extent tree for large files]           │    │
│  └────────────────────────────────────────────────────┘    │
└─────────────────────────────────────────────────────────────┘
```

---

## Data Structures

### Extent Descriptor

```c
/**
 * Extent Descriptor
 * Describes a contiguous range of blocks
 * Size: 16 bytes
 */
struct extent {
    uint64_t logical_offset;    /* Logical offset in file (bytes) */
    uint32_t block_num;         /* Starting block number */
    uint32_t num_blocks;        /* Number of contiguous blocks */
} __attribute__((packed));

/* Special block numbers */
#define EXTENT_HOLE  0          /* Sparse hole (no allocation) */
```

### Updated Inode Structure

```c
/**
 * Inode with Extent Support
 * Size: 64 bytes (unchanged)
 */
struct razorfs_inode {
    /* Identity (8 bytes) */
    uint32_t inode_num;
    uint16_t nlink;
    uint16_t mode;

    /* Timestamps (12 bytes) */
    uint32_t atime;
    uint32_t mtime;
    uint32_t ctime;

    /* Size and attributes (12 bytes) */
    uint64_t size;                  /* File size in bytes */
    uint32_t xattr_head;

    /* Extent storage (32 bytes) */
    union {
        uint8_t inline_data[32];    /* For small files (<= 32 bytes) */

        struct {
            struct extent extents[2];  /* 2 inline extents (32 bytes) */
        } small;                       /* For files up to ~8MB */

        struct {
            uint32_t extent_tree_block; /* Block containing extent tree */
            uint32_t num_extents;       /* Total number of extents */
            uint8_t _pad[24];
        } large;                        /* For files > 8MB */
    } storage;

} __attribute__((packed, aligned(64)));
```

### Extent Tree (For Large Files)

```c
/**
 * Extent Tree Node
 * Stored in data blocks for large files
 * Size: 4KB block
 */
struct extent_tree_node {
    uint32_t num_extents;              /* Number of extents in this node */
    uint32_t _pad;
    struct extent extents[254];        /* 254 extents per 4KB block */
} __attribute__((packed));

/* With 254 extents per block, and assuming 1MB per extent:
 * Single extent tree block can map: 254 * 1MB = 254MB
 * Two-level tree: 254 * 254MB = ~64GB
 * Three-level tree: 254 * 64GB = ~16TB
 */
```

### Block Allocator

```c
/**
 * Block Allocator
 * Manages free space using bitmap
 */
struct block_allocator {
    uint32_t *bitmap;               /* Bitmap: 1 bit per block */
    uint32_t total_blocks;          /* Total number of blocks */
    uint32_t free_blocks;           /* Number of free blocks */
    uint32_t block_size;            /* Block size (default 4096) */
    uint32_t hint;                  /* Allocation hint (last allocated) */
    pthread_rwlock_t lock;          /* Allocator lock */

    /* Storage backing */
    uint8_t *storage;               /* Memory pool for data blocks */
    size_t storage_size;            /* Total storage size */
};

#define BLOCK_SIZE_DEFAULT  4096    /* 4KB blocks */
#define BLOCKS_PER_MB       256     /* 4KB blocks per MB */
```

---

## File Size Categories

### Small Files (<= 32 bytes)
- **Storage**: Inline in inode
- **Access**: Direct from inode
- **Overhead**: 0 extra blocks

### Medium Files (33 bytes - ~8MB)
- **Storage**: 2 inline extents
- **Max size**: 2 extents × ~4MB = ~8MB
- **Overhead**: Data blocks only

### Large Files (> 8MB)
- **Storage**: Extent tree in separate block
- **Max size**: Limited by extent tree depth
- **Overhead**: 1+ extent tree blocks

---

## API Design

### Block Allocator Operations

```c
/**
 * Initialize block allocator
 */
int block_alloc_init(struct block_allocator *alloc,
                     uint32_t total_blocks,
                     uint32_t block_size);

/**
 * Allocate contiguous blocks
 * Returns starting block number, or 0 on failure
 */
uint32_t block_alloc(struct block_allocator *alloc, uint32_t num_blocks);

/**
 * Free blocks
 */
int block_free(struct block_allocator *alloc,
               uint32_t block_num,
               uint32_t num_blocks);

/**
 * Get block statistics
 */
void block_stats(struct block_allocator *alloc,
                 uint32_t *total,
                 uint32_t *free,
                 uint32_t *used);

/**
 * Get block address
 */
void* block_get_addr(struct block_allocator *alloc, uint32_t block_num);

/**
 * Destroy allocator
 */
void block_alloc_destroy(struct block_allocator *alloc);
```

### File Operations with Extents

```c
/**
 * Write data to file at offset
 * Allocates extents as needed
 */
ssize_t extent_write(struct razorfs_inode *inode,
                    struct block_allocator *alloc,
                    const void *buf,
                    size_t size,
                    off_t offset);

/**
 * Read data from file at offset
 * Handles sparse holes
 */
ssize_t extent_read(struct razorfs_inode *inode,
                   struct block_allocator *alloc,
                   void *buf,
                   size_t size,
                   off_t offset);

/**
 * Truncate file to size
 * Frees unused blocks
 */
int extent_truncate(struct razorfs_inode *inode,
                   struct block_allocator *alloc,
                   off_t new_size);

/**
 * Map logical offset to block number
 * Returns EXTENT_HOLE for sparse regions
 */
uint32_t extent_map(struct razorfs_inode *inode,
                   off_t offset);
```

---

## Sparse File Support

### Sparse Holes

Sparse files contain "holes" - regions that are logically present but not physically allocated.

```c
/* Example: 1GB file with 99% sparse */
File size: 1GB (logical)
Allocated: 10MB (physical)
Savings: 990MB

Extents:
  [0, 1MB]    -> Block 100-355     (allocated)
  [1MB, 10MB] -> EXTENT_HOLE       (sparse)
  [10MB, 11MB]-> Block 356-611     (allocated)
```

### Implementation

```c
/**
 * Create sparse hole
 * No block allocation, just update size
 */
int extent_punch_hole(struct razorfs_inode *inode,
                     struct block_allocator *alloc,
                     off_t offset,
                     off_t len);

/**
 * Read from sparse hole returns zeros
 */
if (block_num == EXTENT_HOLE) {
    memset(buf, 0, read_size);
    return read_size;
}
```

---

## Memory-Mapped I/O (mmap)

### Design

RAZORFS can support mmap() by mapping file extents directly into process address space.

```c
/**
 * Map file into memory
 * FUSE: implement mmap callback
 */
void* razorfs_mmap(const char *path, size_t length,
                  int prot, int flags, off_t offset);

/**
 * Implementation:
 * 1. Lookup inode
 * 2. Find extent containing offset
 * 3. Map block storage into address space
 * 4. Return mapped pointer
 */
```

### Challenges

- **COW (Copy-on-Write)**: Writes need to allocate new blocks
- **Page Faults**: Handle faults for sparse regions
- **Coherency**: Keep mmap and regular I/O coherent

**Note**: Full mmap support deferred to Phase 5+ due to complexity.

---

## Block Allocation Strategy

### First-Fit with Hints

```c
uint32_t block_alloc(struct block_allocator *alloc, uint32_t num_blocks) {
    /* Start search from hint */
    uint32_t start = alloc->hint;

    /* Search for contiguous free blocks */
    for (uint32_t i = start; i < alloc->total_blocks; i++) {
        if (is_contiguous_free(alloc, i, num_blocks)) {
            mark_allocated(alloc, i, num_blocks);
            alloc->hint = i + num_blocks;  /* Update hint */
            return i;
        }
    }

    /* Wrap around and search from beginning */
    for (uint32_t i = 0; i < start; i++) {
        if (is_contiguous_free(alloc, i, num_blocks)) {
            mark_allocated(alloc, i, num_blocks);
            alloc->hint = i + num_blocks;
            return i;
        }
    }

    return 0;  /* No space */
}
```

### Allocation Policies

1. **Small files**: Allocate 1 block at a time
2. **Sequential writes**: Pre-allocate ahead
3. **Random writes**: Allocate on-demand
4. **Defragmentation**: Background task (future)

---

## Extent Management

### Extent Merging

Merge adjacent extents to reduce fragmentation:

```c
/* Before: */
Extent 1: [0, 4KB]   -> Block 100
Extent 2: [4KB, 8KB] -> Block 101

/* After merge: */
Extent 1: [0, 8KB]   -> Block 100-101
```

### Extent Splitting

Split extents when truncating or punching holes:

```c
/* Before: */
Extent: [0, 12KB] -> Block 100-102

/* After truncate to 8KB: */
Extent: [0, 8KB]  -> Block 100-101
(Block 102 freed)
```

---

## WAL Integration

### New WAL Operations

```c
enum wal_op_type {
    // ... existing ops ...
    WAL_OP_BLOCK_ALLOC = 20,
    WAL_OP_BLOCK_FREE = 21,
    WAL_OP_EXTENT_ADD = 22,
    WAL_OP_EXTENT_REMOVE = 23,
};

struct wal_block_alloc_data {
    uint32_t block_num;
    uint32_t num_blocks;
};

struct wal_extent_data {
    uint32_t inode_num;
    struct extent ext;
};
```

### Recovery

During recovery, replay block allocations and extent modifications to reconstruct file layout.

---

## Performance Optimizations

### Read-Ahead

Pre-fetch blocks for sequential reads:

```c
/**
 * Read-ahead window: 128KB
 * If reading sequentially, pre-fetch next 32 blocks
 */
#define READAHEAD_WINDOW  (128 * 1024)
#define READAHEAD_BLOCKS  32
```

### Write-Behind

Buffer writes and flush asynchronously:

```c
/**
 * Write buffer: 1MB
 * Accumulate writes, flush periodically or on fsync
 */
#define WRITEBUF_SIZE  (1 * 1024 * 1024)
```

### Block Cache

Cache frequently accessed blocks in memory:

```c
/**
 * LRU cache for hot blocks
 * Size: 64MB (16384 blocks)
 */
#define CACHE_SIZE  (64 * 1024 * 1024)
```

---

## Testing Strategy

### Unit Tests (20-25 tests)

1. **Block Allocator**:
   - Allocate/free single block
   - Allocate contiguous blocks
   - Fragmentation handling
   - Bitmap correctness
   - Full storage

2. **Extent Management**:
   - Add/remove extents
   - Extent merging
   - Extent splitting
   - Lookup by offset
   - Sparse holes

3. **File Operations**:
   - Write small file (inline)
   - Write medium file (2 extents)
   - Write large file (extent tree)
   - Read across extents
   - Sparse file I/O
   - Truncate operations

4. **Edge Cases**:
   - Write to sparse hole
   - Truncate to sparse
   - Extent tree overflow
   - Out of space

---

## Limitations

### Block Size

- Fixed at 4KB (standard page size)
- Future: Configurable block sizes

### Max File Size

- **Inline**: 32 bytes
- **Small extents**: ~8MB (2 extents × 4MB)
- **Extent tree (1-level)**: ~1GB (254 extents × 4MB)
- **Extent tree (2-level)**: ~256GB
- **Extent tree (3-level)**: ~64TB

### Fragmentation

- Internal fragmentation: Up to 4KB per file (last block)
- External fragmentation: Handled by first-fit allocator
- Future: Background defragmentation

---

## Implementation Plan

### Phase 5.1: Block Allocator (1 day)
- [ ] Implement bitmap-based allocator
- [ ] Add allocation/free operations
- [ ] Add statistics and debugging
- [ ] Unit tests for allocator

### Phase 5.2: Extent Storage (1-2 days)
- [ ] Update inode structure
- [ ] Implement inline extents
- [ ] Implement extent tree for large files
- [ ] Extent merging/splitting
- [ ] Unit tests for extents

### Phase 5.3: File I/O (1 day)
- [ ] Implement extent_write()
- [ ] Implement extent_read()
- [ ] Implement extent_truncate()
- [ ] Sparse file support
- [ ] Unit tests for I/O

### Phase 5.4: Optimization (1 day)
- [ ] Read-ahead
- [ ] Write-behind buffering
- [ ] Block cache (optional)
- [ ] Performance benchmarks

---

## Future Enhancements

### Phase 5+
- [ ] Compression at block level
- [ ] Deduplication (same block content)
- [ ] Background defragmentation
- [ ] Online resize
- [ ] Quota support per inode
- [ ] Full mmap() support

---

## References

- [ext4 extent documentation](https://ext4.wiki.kernel.org/index.php/Ext4_Disk_Layout#Extent_Tree)
- [Linux VFS and file I/O](https://www.kernel.org/doc/html/latest/filesystems/vfs.html)
- [FUSE mmap support](https://libfuse.github.io/doxygen/structfuse__operations.html#a6fbb6c44b1b47c0cfb1c190383483d04)

---

**Next Steps**: Implement block allocator in `src/block_alloc.c`
