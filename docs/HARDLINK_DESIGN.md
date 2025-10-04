# Hardlink Support Design

**Phase**: 4
**Status**: In Progress
**Estimated Time**: 1-2 days

---

## Overview

Hardlinks allow multiple directory entries (dentries) to reference the same inode. This design document specifies the implementation of hardlink support in RAZORFS by separating inodes from directory entries.

---

## Requirements

### Functional Requirements
- **FR1**: Support multiple hardlinks to the same file
- **FR2**: Implement reference counting for inodes
- **FR3**: Support `link()` and `unlink()` system calls
- **FR4**: Preserve data when last hardlink is removed only
- **FR5**: Share inode metadata across all hardlinks
- **FR6**: Thread-safe hardlink operations

### Non-Functional Requirements
- **NFR1**: O(1) inode lookup by inode number
- **NFR2**: O(log n) dentry lookup by name (unchanged)
- **NFR3**: Memory-efficient storage
- **NFR4**: Compatible with existing WAL/recovery system

---

## Current Architecture Issues

### Problem 1: Inodes Embedded in Tree Nodes

Currently, RAZORFS stores inode metadata directly in `nary_node`:

```c
struct nary_node {
    uint32_t inode;           // Unique inode number
    uint32_t parent_idx;      // Parent directory index
    uint16_t num_children;    // Child count
    uint16_t mode;            // File type and permissions
    uint32_t name_offset;     // Name in string table
    uint16_t children[16];    // Child indices
    uint64_t size;            // File size
    uint32_t mtime;           // Modification time
    uint32_t xattr_head;      // Extended attributes
};
```

**Issue**: Each tree node represents BOTH a directory entry (dentry) AND an inode. This prevents hardlinks because:
- Each node has exactly one name
- Each node has exactly one parent
- Deleting the node deletes both the name and the inode

### Problem 2: No Reference Counting

Without reference counting, we can't track how many hardlinks point to an inode.

---

## New Architecture

### Separation of Concerns

```
┌─────────────────────────────────────────────────────────────┐
│                    RAZORFS New Layout                        │
├─────────────────────────────────────────────────────────────┤
│  Directory Tree (dentries - names and hierarchy)            │
│  ┌────────────────────────────────────────────────────┐    │
│  │ Dentry: parent_idx | name_offset | inode_num      │    │
│  └────────────────────────────────────────────────────┘    │
├─────────────────────────────────────────────────────────────┤
│  Inode Table (file metadata)                                │
│  ┌────────────────────────────────────────────────────┐    │
│  │ Inode: mode | size | mtime | nlink | xattr_head   │    │
│  └────────────────────────────────────────────────────┘    │
└─────────────────────────────────────────────────────────────┘
```

### Key Changes

1. **Dentry (Directory Entry)**: Stores name and parent-child relationships
2. **Inode**: Stores file metadata (size, permissions, timestamps, xattrs)
3. **Mapping**: Dentries reference inodes by inode number
4. **Reference Count**: Each inode tracks how many dentries point to it

---

## Data Structures

### New Dentry Structure

```c
/**
 * Directory Entry (Dentry)
 * Represents a name in a directory that points to an inode
 * Size: 64 bytes (1 cache line)
 */
struct nary_dentry {
    /* Identity (12 bytes) */
    uint32_t dentry_id;                         /* Unique dentry ID */
    uint32_t parent_idx;                        /* Parent directory index */
    uint16_t num_children;                      /* Count of children (dirs only) */
    uint16_t _pad1;                             /* Padding */

    /* Naming (4 bytes) */
    uint32_t name_offset;                       /* Offset in string table */

    /* Inode reference (4 bytes) */
    uint32_t inode_num;                         /* Inode number */

    /* Children indices (32 bytes) */
    uint16_t children[16];                      /* Child dentry indices */

    /* Padding (12 bytes to reach 64) */
    uint32_t _pad2[3];
} __attribute__((aligned(64)));
```

### New Inode Structure

```c
/**
 * Inode Structure
 * Stores file metadata shared across all hardlinks
 * Size: 64 bytes (1 cache line)
 */
struct razorfs_inode {
    /* Identity (8 bytes) */
    uint32_t inode_num;                         /* Unique inode number */
    uint16_t nlink;                             /* Number of hardlinks */
    uint16_t mode;                              /* File type and permissions */

    /* Timestamps (12 bytes) */
    uint32_t atime;                             /* Access time */
    uint32_t mtime;                             /* Modification time */
    uint32_t ctime;                             /* Change time */

    /* Size and attributes (12 bytes) */
    uint64_t size;                              /* File size in bytes */
    uint32_t xattr_head;                        /* Extended attributes */

    /* Data location (32 bytes) */
    /* For small files: inline data */
    /* For large files: extent pointers (Phase 5) */
    uint8_t data[32];

} __attribute__((aligned(64)));
```

### Inode Table

```c
/**
 * Inode Table
 * Hash table for O(1) inode lookup by inode number
 */
struct inode_table {
    struct razorfs_inode *inodes;              /* Array of inodes */
    uint32_t capacity;                         /* Total capacity */
    uint32_t used;                             /* Number of inodes used */
    uint32_t free_head;                        /* Head of free list */
    pthread_rwlock_t lock;                     /* Table-wide lock */

    /* Hash table for fast lookup */
    uint32_t *hash_table;                      /* Hash inode_num -> index */
    uint32_t hash_capacity;                    /* Hash table size */
};
```

---

## API Design

### Core Operations

```c
/**
 * Initialize inode table
 */
int inode_table_init(struct inode_table *table, uint32_t capacity);

/**
 * Allocate a new inode
 * Returns inode number on success, 0 on error
 */
uint32_t inode_alloc(struct inode_table *table, mode_t mode);

/**
 * Lookup inode by number
 * Returns pointer to inode, or NULL if not found
 */
struct razorfs_inode* inode_lookup(struct inode_table *table, uint32_t inode_num);

/**
 * Increment link count (for hardlink creation)
 * Returns 0 on success, -errno on error
 */
int inode_link(struct inode_table *table, uint32_t inode_num);

/**
 * Decrement link count (for unlink)
 * If nlink reaches 0, inode is freed
 * Returns 0 on success, -errno on error
 */
int inode_unlink(struct inode_table *table, uint32_t inode_num);

/**
 * Update inode metadata
 */
int inode_update(struct inode_table *table, uint32_t inode_num,
                 uint64_t size, uint32_t mtime);

/**
 * Destroy inode table
 */
void inode_table_destroy(struct inode_table *table);
```

### Dentry Operations

```c
/**
 * Create a new dentry (directory entry)
 * Links a name to an existing inode
 */
uint16_t dentry_create(struct nary_tree_mt *tree,
                       uint16_t parent_idx,
                       const char *name,
                       uint32_t inode_num);

/**
 * Remove a dentry
 * Decrements inode link count
 */
int dentry_remove(struct nary_tree_mt *tree, uint16_t dentry_idx);

/**
 * Lookup dentry by name
 * Returns dentry index, or NARY_INVALID_IDX if not found
 */
uint16_t dentry_lookup(struct nary_tree_mt *tree,
                       uint16_t parent_idx,
                       const char *name);
```

---

## Hardlink Operations

### Creating a Hardlink

```c
/**
 * Create a hardlink
 *
 * Example: link("/foo/file1", "/bar/file2")
 *
 * 1. Lookup source dentry "/foo/file1"
 * 2. Get inode_num from source dentry
 * 3. Create new dentry "/bar/file2" with same inode_num
 * 4. Increment inode->nlink
 */
int razorfs_link(const char *oldpath, const char *newpath) {
    /* Parse paths */
    struct path old = parse_path(oldpath);
    struct path new = parse_path(newpath);

    /* Lookup source dentry */
    uint16_t old_idx = dentry_lookup(tree, old.parent, old.name);
    if (old_idx == NARY_INVALID_IDX) return -ENOENT;

    /* Get inode number */
    struct nary_dentry *dentry = &tree->dentries[old_idx];
    uint32_t inode_num = dentry->inode_num;

    /* Check if target already exists */
    if (dentry_lookup(tree, new.parent, new.name) != NARY_INVALID_IDX) {
        return -EEXIST;
    }

    /* Create new dentry pointing to same inode */
    uint16_t new_idx = dentry_create(tree, new.parent, new.name, inode_num);
    if (new_idx == NARY_INVALID_IDX) return -ENOSPC;

    /* Increment link count */
    return inode_link(inode_table, inode_num);
}
```

### Unlinking

```c
/**
 * Unlink a file
 *
 * 1. Remove dentry from tree
 * 2. Decrement inode->nlink
 * 3. If nlink == 0, free inode and data
 */
int razorfs_unlink(const char *path) {
    /* Parse path */
    struct path p = parse_path(path);

    /* Lookup dentry */
    uint16_t idx = dentry_lookup(tree, p.parent, p.name);
    if (idx == NARY_INVALID_IDX) return -ENOENT;

    /* Get inode number before removing dentry */
    struct nary_dentry *dentry = &tree->dentries[idx];
    uint32_t inode_num = dentry->inode_num;

    /* Remove dentry */
    dentry_remove(tree, idx);

    /* Decrement link count (frees inode if nlink == 0) */
    return inode_unlink(inode_table, inode_num);
}
```

---

## Migration Strategy

### Phase 4.1: Add Inode Table (This Phase)

1. Create `src/inode_table.h` and `src/inode_table.c`
2. Implement inode allocation and lookup
3. Keep existing nary_tree as-is for compatibility
4. Add parallel inode table for testing

### Phase 4.2: Refactor Dentry Structure

1. Modify `nary_node` to become `nary_dentry`
2. Remove metadata fields (size, mtime, mode) from dentry
3. Add `inode_num` field to dentry
4. Update all operations to use inode table

### Phase 4.3: Implement Hardlink Operations

1. Implement `razorfs_link()` in FUSE layer
2. Update `razorfs_unlink()` to use reference counting
3. Update `razorfs_getattr()` to fetch from inode table

---

## Compatibility Considerations

### Backward Compatibility

**Issue**: Existing code expects metadata in nary_node

**Solution**: Gradual migration
1. Add inode table alongside existing tree
2. Populate inode table during create/write operations
3. Deprecate direct access to node metadata
4. Eventually remove metadata from nary_node

### WAL Integration

**New WAL Operations**:
```c
enum wal_op_type {
    // ... existing ops ...
    WAL_OP_INODE_ALLOC = 12,
    WAL_OP_INODE_UPDATE = 13,
    WAL_OP_INODE_FREE = 14,
    WAL_OP_LINK = 15,
};

struct wal_link_data {
    uint32_t inode_num;
    uint32_t parent_idx;
    uint32_t name_offset;
};
```

---

## Memory Layout

### Inode Table Layout

```
Offset  Size  Field
------  ----  -----
0       4     inode_num
4       2     nlink
6       2     mode
8       4     atime
12      4     mtime
16      4     ctime
20      8     size
28      4     xattr_head
32      32    data (inline or extent pointers)
```

### Example: File with 3 Hardlinks

```
Inode Table[42]:
  inode_num = 42
  nlink = 3
  mode = 0644
  size = 1024
  mtime = 1696377600

Dentry Tree:
  /home/user/file1   -> inode 42
  /home/user/file2   -> inode 42
  /tmp/backup        -> inode 42
```

---

## Testing Strategy

### Unit Tests (15-20 tests)

1. **Inode Table**:
   - Allocate/free inodes
   - Lookup by inode number
   - Reference counting
   - Hash table collisions

2. **Hardlink Operations**:
   - Create hardlink (link)
   - Remove hardlink (unlink)
   - Last link removal frees inode
   - Link count tracking

3. **Edge Cases**:
   - Maximum hardlinks
   - Cross-directory hardlinks
   - Link to self (should fail)
   - Link to directory (should fail)

4. **Concurrent Access**:
   - Parallel link/unlink
   - Race conditions in reference counting

5. **Integration**:
   - Hardlinks with xattrs
   - Hardlinks with WAL/recovery
   - Persistence across remount

---

## Performance Considerations

### Expected Performance

- **Inode Lookup**: O(1) via hash table
- **Link Creation**: O(log n) for dentry + O(1) for inode
- **Unlink**: O(log n) for dentry + O(1) for inode
- **Memory Overhead**: 64 bytes per inode (separate from dentry)

### Optimizations

1. **Hash Table**: Use prime number size to reduce collisions
2. **Free List**: Reuse freed inode slots
3. **Lock Granularity**: Per-inode locks (future)
4. **Cache Locality**: 64-byte aligned inodes

---

## Limitations

### Hard Limits

```c
#define INODE_MAX_LINKS     65535   /* Max hardlinks per inode (uint16_t) */
#define INODE_TABLE_SIZE    65536   /* Max inodes with 16-bit numbers */
```

### Restrictions

1. **No directory hardlinks**: Prevents cycles (POSIX requirement)
2. **No cross-filesystem links**: RAZORFS only
3. **Link count overflow**: Enforced at INODE_MAX_LINKS

---

## Error Handling

### Error Codes

- **EEXIST**: Target already exists (link)
- **ENOENT**: Source doesn't exist (link/unlink)
- **EPERM**: Attempted to link a directory
- **EMLINK**: Too many links (exceeds INODE_MAX_LINKS)
- **EISDIR**: Attempted to unlink a directory (use rmdir)
- **ENOSPC**: No space in inode table

---

## Future Enhancements

### Phase 4+

- [ ] Optimize inode table with better hash function
- [ ] Per-inode locks instead of table-wide lock
- [ ] Inode cache for frequently accessed inodes
- [ ] Lazy inode allocation (allocate on first write)
- [ ] Inode generation numbers (for NFS support)
- [ ] Hard link quotas per user

---

## References

- [POSIX link() specification](https://pubs.opengroup.org/onlinepubs/9699919799/functions/link.html)
- [Linux VFS inode design](https://www.kernel.org/doc/html/latest/filesystems/vfs.html)
- [ext4 inode structure](https://ext4.wiki.kernel.org/index.php/Ext4_Disk_Layout#Inode_Table)

---

**Next Steps**: Implement inode table in `src/inode_table.c`
