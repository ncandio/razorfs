# Extended Attributes (xattr) Design

**Phase**: 3
**Status**: In Progress
**Estimated Time**: 1-2 days

---

## Overview

Extended attributes (xattrs) provide a mechanism to store arbitrary metadata alongside files and directories. This design document specifies the implementation of xattr support in RAZORFS.

---

## Requirements

### Functional Requirements
- **FR1**: Support POSIX xattr operations (getxattr, setxattr, listxattr, removexattr)
- **FR2**: Support security namespaces (security.*, system.*, user.*, trusted.*)
- **FR3**: Store xattrs in shared memory for persistence
- **FR4**: Support xattr names up to 255 bytes
- **FR5**: Support xattr values up to 64KB
- **FR6**: Thread-safe xattr operations

### Non-Functional Requirements
- **NFR1**: O(1) xattr lookup per inode
- **NFR2**: Memory-efficient storage (no space wasted for files without xattrs)
- **NFR3**: Compatible with WAL/recovery system
- **NFR4**: Lock-free reads when possible

---

## Architecture

### Storage Design

```
┌─────────────────────────────────────────────────────────────┐
│                    RAZORFS Memory Layout                     │
├─────────────────────────────────────────────────────────────┤
│  N-ary Tree (nodes with inode + xattr_head ptr)             │
├─────────────────────────────────────────────────────────────┤
│  String Table (file names)                                   │
├─────────────────────────────────────────────────────────────┤
│  Xattr Pool (linked list of xattr entries per inode)        │
│  ┌────────────────────────────────────────────────────┐    │
│  │ Entry: name_offset | value_offset | value_len     │    │
│  │        flags | next_offset                         │    │
│  └────────────────────────────────────────────────────┘    │
├─────────────────────────────────────────────────────────────┤
│  Xattr String Table (names like "user.foo")                 │
├─────────────────────────────────────────────────────────────┤
│  Xattr Value Pool (variable-length values)                  │
└─────────────────────────────────────────────────────────────┘
```

### Data Structures

```c
/**
 * Xattr Entry
 * Stored in xattr pool, linked list per inode
 */
struct xattr_entry {
    uint32_t name_offset;     // Offset into xattr string table
    uint32_t value_offset;    // Offset into xattr value pool
    uint32_t value_len;       // Length of value
    uint8_t  flags;           // Namespace flags
    uint32_t next_offset;     // Next xattr in list (0 = end)
} __attribute__((aligned(16)));  // 20 bytes + padding = 32 bytes

/**
 * Xattr Pool
 * Manages xattr entries
 */
struct xattr_pool {
    struct xattr_entry *entries;  // Array of xattr entries
    uint32_t capacity;            // Total capacity
    uint32_t used;                // Number of entries used
    uint32_t free_head;           // Head of free list
    pthread_rwlock_t lock;        // Pool-wide lock
};

/**
 * Xattr Value Pool
 * Variable-length value storage
 */
struct xattr_value_pool {
    uint8_t *buffer;              // Value storage buffer
    uint32_t capacity;            // Total capacity
    uint32_t used;                // Bytes used
    uint32_t free_head;           // Head of free list
    pthread_rwlock_t lock;        // Pool-wide lock
};

/**
 * Xattr Namespace Flags
 */
#define XATTR_NS_SECURITY  0x01
#define XATTR_NS_SYSTEM    0x02
#define XATTR_NS_USER      0x04
#define XATTR_NS_TRUSTED   0x08
```

### Modified Node Structure

```c
struct nary_node {
    uint64_t inode;
    mode_t mode;
    size_t size;
    time_t mtime;

    // Xattr support
    uint32_t xattr_head;      // Offset to first xattr (0 = none)
    uint16_t xattr_count;     // Number of xattrs

    // ... existing fields ...
};
```

---

## API Design

### Core Operations

```c
/**
 * Initialize xattr subsystem
 */
int xattr_init(struct xattr_pool *pool,
               struct xattr_value_pool *values,
               struct string_table *names,
               uint32_t max_entries,
               uint32_t value_pool_size);

/**
 * Cleanup xattr subsystem
 */
void xattr_destroy(struct xattr_pool *pool,
                   struct xattr_value_pool *values);

/**
 * Get xattr value
 * Returns 0 on success, -ENODATA if not found, -ERANGE if buffer too small
 */
int xattr_get(struct xattr_pool *pool,
              struct xattr_value_pool *values,
              struct string_table *names,
              uint32_t xattr_head,
              const char *name,
              void *value,
              size_t size);

/**
 * Set xattr value
 * Returns new xattr_head offset, or -errno on error
 */
int xattr_set(struct xattr_pool *pool,
              struct xattr_value_pool *values,
              struct string_table *names,
              uint32_t *xattr_head,
              uint16_t *xattr_count,
              const char *name,
              const void *value,
              size_t size,
              int flags);

/**
 * List xattr names
 * Returns total size needed, fills buffer up to size
 */
ssize_t xattr_list(struct xattr_pool *pool,
                   struct string_table *names,
                   uint32_t xattr_head,
                   char *list,
                   size_t size);

/**
 * Remove xattr
 * Returns 0 on success, -ENODATA if not found
 */
int xattr_remove(struct xattr_pool *pool,
                 struct xattr_value_pool *values,
                 struct string_table *names,
                 uint32_t *xattr_head,
                 uint16_t *xattr_count,
                 const char *name);
```

---

## Implementation Plan

### Phase 3.1: Core Infrastructure (4-6 hours)
- [ ] Create `src/xattr.h` with data structures
- [ ] Create `src/xattr.c` with pool management
- [ ] Implement xattr_init/destroy
- [ ] Implement memory allocation for entries and values
- [ ] Add free list management

### Phase 3.2: Operations (4-6 hours)
- [ ] Implement xattr_get
- [ ] Implement xattr_set
- [ ] Implement xattr_list
- [ ] Implement xattr_remove
- [ ] Add namespace validation
- [ ] Add size limit enforcement

### Phase 3.3: FUSE Integration (2-3 hours)
- [ ] Add xattr fields to nary_node
- [ ] Implement razorfs_getxattr
- [ ] Implement razorfs_setxattr
- [ ] Implement razorfs_listxattr
- [ ] Implement razorfs_removexattr
- [ ] Update razorfs_unlink to free xattrs

### Phase 3.4: Testing (2-3 hours)
- [ ] Create `tests/unit/xattr_test.cpp`
- [ ] Test basic get/set/list/remove
- [ ] Test namespace validation
- [ ] Test size limits
- [ ] Test concurrent access
- [ ] Test shared memory persistence
- [ ] Integration tests with real filesystem

---

## Memory Layout

### Xattr Entry Layout
```
Offset  Size  Field
------  ----  -----
0       4     name_offset
4       4     value_offset
8       4     value_len
12      1     flags
13      3     (padding)
16      4     next_offset
20      12    (padding to 32 bytes)
```

### Example: File with 2 xattrs
```
Node:
  inode = 12345
  xattr_head = 100  (offset in xattr pool)
  xattr_count = 2

Xattr Pool[100]:
  name_offset = 50     -> "user.comment"
  value_offset = 200   -> "This is my file"
  value_len = 15
  flags = XATTR_NS_USER
  next_offset = 101

Xattr Pool[101]:
  name_offset = 51     -> "user.author"
  value_offset = 220   -> "Alice"
  value_len = 5
  flags = XATTR_NS_USER
  next_offset = 0      (end of list)
```

---

## Namespace Support

### Supported Namespaces

1. **security.*** - Security attributes (SELinux, capabilities)
   - Requires CAP_SYS_ADMIN for modification
   - Always readable

2. **system.*** - System attributes (ACLs, etc.)
   - Requires appropriate permissions
   - Used for POSIX ACLs

3. **user.*** - User attributes
   - Regular users can set on files they own
   - Most common namespace

4. **trusted.*** - Trusted attributes
   - Requires CAP_SYS_ADMIN
   - For system administration

### Validation Rules

```c
static int validate_xattr_name(const char *name, uint8_t *flags_out) {
    if (strncmp(name, "security.", 9) == 0) {
        *flags_out = XATTR_NS_SECURITY;
        return 0;
    }
    if (strncmp(name, "system.", 7) == 0) {
        *flags_out = XATTR_NS_SYSTEM;
        return 0;
    }
    if (strncmp(name, "user.", 5) == 0) {
        *flags_out = XATTR_NS_USER;
        return 0;
    }
    if (strncmp(name, "trusted.", 8) == 0) {
        *flags_out = XATTR_NS_TRUSTED;
        return 0;
    }
    return -EOPNOTSUPP;  // Invalid namespace
}
```

---

## Locking Strategy

### Read Operations
- Acquire read lock on xattr pool
- Read lock allows concurrent getxattr/listxattr
- No lock on individual entries (read-only traversal)

### Write Operations
- Acquire write lock on xattr pool
- Write lock for setxattr/removexattr
- Prevents concurrent modifications

### Lock-Free Optimization (Future)
- Use atomic operations for xattr_head pointer
- Copy-on-write for modifications
- RCU-style deferred reclamation

---

## Size Limits

```c
#define XATTR_NAME_MAX      255     // Maximum name length
#define XATTR_SIZE_MAX      65536   // Maximum value size (64KB)
#define XATTR_LIST_MAX      65536   // Maximum total list size
```

### Size Validation
- Name length: 1-255 bytes
- Value length: 0-65536 bytes
- Total xattrs per file: Limited by pool capacity
- List output: Up to 65536 bytes total

---

## WAL Integration (Future)

### WAL Operations for Xattr
```c
enum wal_op_type {
    // ... existing ops ...
    WAL_OP_XATTR_SET = 10,
    WAL_OP_XATTR_REMOVE = 11
};

struct wal_xattr_data {
    uint64_t inode;
    uint32_t name_offset;
    uint32_t value_len;
    uint8_t flags;
    // Followed by value data
};
```

### Recovery
- Replay XATTR_SET operations
- Replay XATTR_REMOVE operations
- Idempotency: Check if xattr already exists/removed

---

## Performance Considerations

### Expected Performance
- **Get**: O(n) where n = number of xattrs per file (typically < 10)
- **Set**: O(n) for update, O(1) for append
- **List**: O(n)
- **Remove**: O(n)

### Optimizations
- Hash table for files with many xattrs (future)
- Cache frequently accessed xattrs (future)
- Bloom filter for "has no xattrs" (future)

### Memory Overhead
- 32 bytes per xattr entry
- Variable value storage
- Separate string table for names (with deduplication)
- Zero overhead for files without xattrs

---

## Testing Strategy

### Unit Tests (15-20 tests)
1. Pool initialization and cleanup
2. Basic get/set operations
3. List operation with multiple xattrs
4. Remove operation
5. Namespace validation
6. Size limit enforcement (name, value, total)
7. Free list management
8. Concurrent access (multithreaded)
9. Shared memory persistence
10. Edge cases (empty values, max sizes, etc.)

### Integration Tests
1. FUSE operations through actual filesystem
2. Test with `getfattr`/`setfattr` tools
3. Persistence across mount/unmount
4. Large number of xattrs per file
5. Mixed workload (set/get/remove/list)

---

## Error Handling

### Error Codes
- **ENODATA**: Xattr not found
- **ERANGE**: Buffer too small
- **ENOSPC**: No space in pool
- **EOPNOTSUPP**: Invalid namespace
- **ENAMETOOLONG**: Name too long
- **E2BIG**: Value too large
- **EEXIST**: XATTR_CREATE and already exists
- **ENODATA**: XATTR_REPLACE and doesn't exist

---

## Example Usage

### Setting an xattr
```c
struct xattr_pool pool;
struct xattr_value_pool values;
struct string_table names;

// Initialize
xattr_init(&pool, &values, &names, 1024, 1024 * 1024);

// Set xattr on inode
uint32_t xattr_head = 0;
uint16_t xattr_count = 0;

int ret = xattr_set(&pool, &values, &names,
                    &xattr_head, &xattr_count,
                    "user.comment", "Hello World", 11, 0);

// xattr_head now points to first entry
// xattr_count = 1
```

### Getting an xattr
```c
char buffer[256];
int ret = xattr_get(&pool, &values, &names,
                    xattr_head, "user.comment",
                    buffer, sizeof(buffer));

// buffer = "Hello World"
// ret = 11 (value length)
```

### Listing xattrs
```c
char list[1024];
ssize_t ret = xattr_list(&pool, &names, xattr_head, list, sizeof(list));

// list = "user.comment\0user.author\0"
// ret = total size needed
```

---

## Future Enhancements

### Phase 3+
- [ ] Hash table for fast lookup (when > 10 xattrs)
- [ ] Compression for large xattr values
- [ ] WAL integration for crash recovery
- [ ] Bloom filter optimization
- [ ] Copy-on-write for lock-free reads
- [ ] Quota enforcement per namespace
- [ ] Statistics (total xattrs, memory usage, etc.)

---

## References

- [Linux xattr man page](https://man7.org/linux/man-pages/man7/xattr.7.html)
- [FUSE xattr operations](https://libfuse.github.io/doxygen/structfuse__operations.html)
- [Extended Attributes (Wikipedia)](https://en.wikipedia.org/wiki/Extended_file_attributes)

---

**Next Steps**: Implement core infrastructure in `src/xattr.c`
