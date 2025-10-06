# Making RAZORFS Truly Persistent (Survive Reboot)

## The Problem

Currently RAZORFS uses `/dev/shm` which is **tmpfs** (temporary filesystem in RAM):
- Fast but volatile
- Cleared on every reboot
- All data lost on power loss

## The Solution: Replace shm_open with File-Backed mmap

### What Needs to Change

Replace all `shm_open()` calls with `open() + mmap()` on real files:

```c
// BEFORE (volatile - current implementation)
int fd = shm_open("/razorfs_nodes", O_RDWR | O_CREAT, 0600);

// AFTER (persistent - survives reboot)
int fd = open("/var/lib/razorfs/nodes.dat", O_RDWR | O_CREAT, 0600);
```

### Files to Modify

#### 1. **src/shm_persist.c** - Main persistence layer

**Current code (lines ~30-50):**
```c
int shm_persist_init(struct shm_persist *sp, const char *shm_name,
                     size_t size, int existing) {
    sp->shm_name = strdup(shm_name);

    int flags = O_RDWR;
    if (!existing) {
        flags |= O_CREAT;
        sp->shm_fd = shm_open(shm_name, flags, 0600);  // ← VOLATILE!
        ftruncate(sp->shm_fd, size);
    } else {
        sp->shm_fd = shm_open(shm_name, flags, 0600);  // ← VOLATILE!
    }

    sp->base = mmap(NULL, size, PROT_READ | PROT_WRITE,
                    MAP_SHARED, sp->shm_fd, 0);
    // ...
}
```

**New implementation (disk-backed):**
```c
int shm_persist_init_disk(struct shm_persist *sp, const char *filepath,
                          size_t size, int existing) {
    sp->shm_name = strdup(filepath);  // Actually a file path now

    int flags = O_RDWR;
    if (!existing) {
        flags |= O_CREAT;
    }

    // Use regular file instead of shm_open
    sp->shm_fd = open(filepath, flags, 0600);
    if (sp->shm_fd < 0) {
        return -1;
    }

    // Ensure file is correct size
    struct stat st;
    if (fstat(sp->shm_fd, &st) < 0) {
        close(sp->shm_fd);
        return -1;
    }

    if (st.st_size < size) {
        if (ftruncate(sp->shm_fd, size) < 0) {
            close(sp->shm_fd);
            return -1;
        }
    }

    // mmap the file (this is what makes it persistent!)
    sp->base = mmap(NULL, size, PROT_READ | PROT_WRITE,
                    MAP_SHARED, sp->shm_fd, 0);
    if (sp->base == MAP_FAILED) {
        close(sp->shm_fd);
        return -1;
    }

    sp->size = size;
    return 0;
}
```

#### 2. **fuse/razorfs_mt.c** - Update initialization

**Current (volatile):**
```c
// Line ~600-620
if (nary_tree_mt_init_shm(&g_mt_fs.tree, TREE_SIZE, existing) != 0) {
    fprintf(stderr, "Failed to initialize persistent tree\n");
    return 1;
}
```

**New (persistent):**
```c
// Create data directory if it doesn't exist
const char *data_dir = "/var/lib/razorfs";
mkdir(data_dir, 0700);

// Use file-backed storage instead of /dev/shm
const char *nodes_file = "/var/lib/razorfs/nodes.dat";
const char *strings_file = "/var/lib/razorfs/strings.dat";

if (nary_tree_mt_init_disk(&g_mt_fs.tree, nodes_file, strings_file,
                           TREE_SIZE, existing) != 0) {
    fprintf(stderr, "Failed to initialize persistent tree\n");
    return 1;
}
```

#### 3. **src/nary_tree_mt.c** - Add disk-backed init function

**Add new function:**
```c
int nary_tree_mt_init_disk(struct nary_tree_mt *tree,
                           const char *nodes_file,
                           const char *strings_file,
                           size_t node_capacity,
                           int existing) {
    // Initialize node storage (file-backed)
    if (shm_persist_init_disk(&tree->persist, nodes_file,
                              node_capacity * sizeof(struct nary_node_mt),
                              existing) != 0) {
        return -1;
    }

    // Initialize string table (file-backed)
    if (string_table_init_disk(&tree->strings, strings_file,
                               STRING_TABLE_SIZE, existing) != 0) {
        shm_persist_destroy(&tree->persist);
        return -1;
    }

    tree->nodes = (struct nary_node_mt *)tree->persist.base;
    tree->capacity = node_capacity;

    if (!existing) {
        // Initialize root node
        tree->size = 1;
        nary_node_mt_init(&tree->nodes[0], 0, 1);  // Root is directory
    }

    return 0;
}
```

#### 4. **src/string_table.c** - Add disk-backed string table

**Add new function:**
```c
int string_table_init_disk(struct string_table *st,
                           const char *filepath,
                           size_t total_size,
                           int existing) {
    int fd = open(filepath, O_RDWR | O_CREAT, 0600);
    if (fd < 0) {
        return -1;
    }

    // Ensure file is correct size
    if (!existing) {
        if (ftruncate(fd, total_size) < 0) {
            close(fd);
            return -1;
        }
    }

    // mmap the file
    void *data = mmap(NULL, total_size, PROT_READ | PROT_WRITE,
                      MAP_SHARED, fd, 0);
    if (data == MAP_FAILED) {
        close(fd);
        return -1;
    }

    st->data = data;
    st->capacity = total_size;
    st->fd = fd;  // Store fd for cleanup

    if (!existing) {
        st->size = 0;
        st->is_shm = 0;
    } else {
        // Read existing size from mmap'd data
        // (assuming first 8 bytes store size)
        memcpy(&st->size, data, sizeof(size_t));
    }

    return 0;
}
```

### Storage Location Options

#### Option 1: `/var/lib/razorfs/` (System-wide, requires root)
```bash
sudo mkdir -p /var/lib/razorfs
sudo chown $USER:$USER /var/lib/razorfs
```
- **Pros:** Standard location, survives user logout
- **Cons:** Needs root setup, shared across all users

#### Option 2: `/tmp/razorfs_data/` (Simple, no root needed)
```bash
mkdir -p /tmp/razorfs_data
```
- **Pros:** No permissions needed, easy testing
- **Cons:** Still cleared on reboot (but `/tmp` on many systems is actually on disk)

#### Option 3: `~/.local/share/razorfs/` (Per-user)
```bash
mkdir -p ~/.local/share/razorfs
```
- **Pros:** User-specific, follows XDG spec, survives reboot
- **Cons:** Each user has separate data

#### Option 4: Configurable path (Best for production)
```c
// Add command-line option
const char *data_dir = getenv("RAZORFS_DATA_DIR");
if (!data_dir) {
    data_dir = "/var/lib/razorfs";  // Default
}
```

### Implementation Plan

**Quick Fix (30 minutes):**
```bash
# 1. Add shm_persist_init_disk() to src/shm_persist.c
# 2. Add nary_tree_mt_init_disk() to src/nary_tree_mt.c
# 3. Add string_table_init_disk() to src/string_table.c
# 4. Update fuse/razorfs_mt.c to use disk paths
# 5. Rebuild and test
```

**Proper Implementation (2-3 hours):**
```bash
# 1. Refactor shm_persist to be storage-agnostic
# 2. Add metadata header to mmap'd files (version, magic, size)
# 3. Add file locking (flock) for multi-process safety
# 4. Add data directory management
# 5. Update all tests
# 6. Add migration tool (shm → disk)
```

### Example: Minimal Change

**File: `src/shm_persist.h`**
```c
// Add new function
int shm_persist_init_disk(struct shm_persist *sp, const char *filepath,
                          size_t size, int existing);
```

**File: `src/shm_persist.c`**
```c
int shm_persist_init_disk(struct shm_persist *sp, const char *filepath,
                          size_t size, int existing) {
    sp->shm_name = strdup(filepath);

    int flags = O_RDWR | (existing ? 0 : O_CREAT);
    sp->shm_fd = open(filepath, flags, 0600);
    if (sp->shm_fd < 0) {
        return -1;
    }

    if (!existing && ftruncate(sp->shm_fd, size) < 0) {
        close(sp->shm_fd);
        return -1;
    }

    sp->base = mmap(NULL, size, PROT_READ | PROT_WRITE,
                    MAP_SHARED, sp->shm_fd, 0);
    if (sp->base == MAP_FAILED) {
        close(sp->shm_fd);
        return -1;
    }

    sp->size = size;
    return 0;
}
```

**File: `fuse/razorfs_mt.c`**
```c
// Replace shm_open path with disk path
- const char *shm_path = "/razorfs_nodes";
+ const char *nodes_path = "/tmp/razorfs_data/nodes.dat";

- if (shm_persist_init(&g_mt_fs.tree.persist, shm_path, size, existing) != 0) {
+ if (shm_persist_init_disk(&g_mt_fs.tree.persist, nodes_path, size, existing) != 0) {
```

### Testing After Changes

```bash
# 1. Build with new disk-backed storage
make clean && make

# 2. Mount
./razorfs /tmp/razorfs_mount

# 3. Create data
echo "Test data" > /tmp/razorfs_mount/test.txt

# 4. Unmount
fusermount3 -u /tmp/razorfs_mount

# 5. Check data files exist on disk
ls -lh /tmp/razorfs_data/
# Should show: nodes.dat, strings.dat

# 6. Remount
./razorfs /tmp/razorfs_mount

# 7. Verify data survived
cat /tmp/razorfs_mount/test.txt
# Should output: "Test data"

# 8. REBOOT SIMULATION
# Kill process, clear /dev/shm (doesn't matter anymore!)
pkill -9 razorfs
rm -rf /dev/shm/razorfs*

# 9. Remount after "reboot"
./razorfs /tmp/razorfs_mount

# 10. Data should STILL be there!
cat /tmp/razorfs_mount/test.txt
# Should STILL output: "Test data" ✅
```

### Summary

**Current State:**
```
RAZORFS → /dev/shm (tmpfs in RAM) → Lost on reboot ❌
```

**After Changes:**
```
RAZORFS → /tmp/razorfs_data/*.dat (mmap'd files) → Survives reboot ✅
            ↓
          WAL → /tmp/razorfs_wal.log (already disk-backed) → Survives crash ✅
```

**Result:** True persistence with crash recovery!

---

Would you like me to implement this change now? It's a relatively simple modification to make RAZORFS truly persistent.
