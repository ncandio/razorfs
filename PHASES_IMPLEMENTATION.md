# RAZORFS Implementation Phases - Complete Rewrite Plan

**Project Goal**: Transform RAZORFS from hash-table-based implementation to true n-ary tree filesystem with O(log n) operations, NUMA optimization, cache-friendly design, and proper multithreading.

**Implementation Strategy**: FUSE userspace first, then kernel module migration.

---

## 📋 PHASE 1: Core N-ary Tree Implementation (FUSE)
**Duration**: 2 weeks
**Status**: NOT STARTED
**Assignable to**: Any agent with C programming knowledge

### Objectives
- ✓ Implement true n-ary tree with O(log n) operations
- ✓ Contiguous array-based storage (no pointer chasing)
- ✓ Breadth-first memory layout for cache locality
- ✓ Lazy balancing every 100 operations
- ✓ Pure C implementation, zero templates

### Deliverables

#### 1.1 Node Structure (`src/nary_node.h`)
```c
#define NARY_BRANCHING_FACTOR 16
#define CACHE_LINE_SIZE 64

// EXACTLY 64 bytes - single cache line
struct __attribute__((aligned(64))) nary_node {
    uint32_t inode;                           // 4 bytes
    uint32_t parent_idx;                      // 4 bytes
    uint16_t num_children;                    // 2 bytes
    uint16_t mode;                            // 2 bytes (S_IFDIR|S_IFREG)
    uint32_t name_offset;                     // 4 bytes (string table)
    uint16_t children[NARY_BRANCHING_FACTOR]; // 32 bytes (indices)
    uint64_t size;                            // 8 bytes
    uint64_t mtime;                           // 8 bytes
    // Total: 64 bytes
};
```

#### 1.2 Tree Operations (`src/nary_tree.c`)
```c
// Core operations (all O(log n))
uint32_t nary_find_child(struct nary_tree *tree, uint32_t parent_idx,
                         const char *name);
uint32_t nary_insert(struct nary_tree *tree, uint32_t parent_idx,
                     const char *name, uint16_t mode);
int nary_delete(struct nary_tree *tree, uint32_t idx);
void nary_rebalance(struct nary_tree *tree);  // Lazy, every 100 ops

// Tree structure
struct nary_tree {
    struct nary_node *nodes;      // Contiguous array
    uint32_t capacity;
    uint32_t used;
    uint32_t root_idx;
    uint32_t op_count;            // For lazy balancing
    struct string_table *strings;
};
```

#### 1.3 String Table (`src/string_table.c`)
```c
// Interned strings for cache efficiency
struct string_table {
    char *data;
    uint32_t capacity;
    uint32_t used;
};

uint32_t string_intern(struct string_table *st, const char *str);
const char *string_get(struct string_table *st, uint32_t offset);
```

#### 1.4 Simple FUSE Interface (`fuse/razorfs_simple.c`)
```c
// Minimal FUSE operations - NO OPTIMIZATION FLAGS
// Compile with: -O0 -g for debugging
// Maximum 500 lines total

static int razorfs_getattr(const char *path, struct stat *stbuf,
                           struct fuse_file_info *fi);
static int razorfs_readdir(const char *path, void *buf,
                           fuse_fill_dir_t filler, off_t offset,
                           struct fuse_file_info *fi,
                           enum fuse_readdir_flags flags);
static int razorfs_mkdir(const char *path, mode_t mode);
static int razorfs_create(const char *path, mode_t mode,
                          struct fuse_file_info *fi);
static int razorfs_unlink(const char *path);
static int razorfs_rmdir(const char *path);
static int razorfs_read(const char *path, char *buf, size_t size,
                        off_t offset, struct fuse_file_info *fi);
static int razorfs_write(const char *path, const char *buf, size_t size,
                         off_t offset, struct fuse_file_info *fi);
```

### Testing Requirements
- Create/read/delete 10,000 files successfully
- Verify O(log n) scaling with performance measurements
- No memory leaks (valgrind clean)
- Tree structure validation after operations

### Success Criteria
- [ ] All operations complete in O(log n) time
- [ ] Tree maintains balance (max depth ≤ log₁₆(n) + 2)
- [ ] Memory layout is contiguous (verify with gdb)
- [ ] FUSE filesystem mounts and passes basic tests
- [ ] Code under 400 lines for tree, 500 lines for FUSE

### Files to Create
```
src/nary_node.h         (50 lines)
src/nary_tree.h         (80 lines)
src/nary_tree.c         (300 lines)
src/string_table.h      (30 lines)
src/string_table.c      (100 lines)
fuse/razorfs_simple.c   (450 lines)
fuse/Makefile.simple    (30 lines)
tests/test_nary_tree.c  (200 lines)
```

### Files to Delete
```
fuse/razorfs_fuse.cpp
fuse/razorfs_fuse_original.cpp
fuse/razorfs_fuse_cache_optimized.cpp
fuse/razorfs_fuse_st.cpp
fuse/razorfs_fuse_simple.cpp
src/cache_optimized_filesystem.hpp
src/razor_optimized_v2.hpp
src/razor_cache_optimized_nary_tree.hpp
```

---

## 📋 PHASE 2: NUMA & Cache Optimization (FUSE)
**Duration**: 1.5 weeks
**Status**: BLOCKED (requires Phase 1)
**Dependencies**: Phase 1 complete

### Objectives
- ✓ NUMA-aware memory allocation
- ✓ CPU affinity for reduced latency
- ✓ Prefetching hints for tree traversal
- ✓ False-sharing prevention
- ✓ Memory barriers for correctness

### Deliverables

#### 2.1 NUMA Allocator (`src/numa_alloc.c`)
```c
#include <numa.h>
#include <numaif.h>

// Allocate nodes on local NUMA node
void *numa_alloc_nodes(size_t count) {
    int cpu = sched_getcpu();
    int node = numa_node_of_cpu(cpu);
    size_t size = count * sizeof(struct nary_node);
    void *mem = numa_alloc_onnode(size, node);
    return mem;
}

// Set CPU affinity to reduce migrations
int set_cpu_affinity(int cpu) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(cpu, &cpuset);
    return sched_setaffinity(0, sizeof(cpuset), &cpuset);
}
```

#### 2.2 Prefetch Optimization (`src/nary_tree.c` enhancement)
```c
// Add prefetching to tree descent
uint32_t nary_find_child_prefetch(struct nary_tree *tree,
                                  uint32_t parent_idx,
                                  const char *name) {
    struct nary_node *parent = &tree->nodes[parent_idx];

    // Prefetch children nodes
    for (int i = 0; i < parent->num_children; i++) {
        uint32_t child_idx = parent->children[i];
        __builtin_prefetch(&tree->nodes[child_idx], 0, 3);
    }

    // Now do actual lookup
    // ...
}
```

#### 2.3 Memory Barriers (`src/nary_tree.h`)
```c
#define smp_wmb() __asm__ __volatile__("sfence":::"memory")
#define smp_rmb() __asm__ __volatile__("lfence":::"memory")
#define smp_mb()  __asm__ __volatile__("mfence":::"memory")
```

### Testing Requirements
- NUMA locality measurements (numastat)
- Cache hit rate profiling (perf stat)
- Verify prefetch improves lookup time
- Test on multi-socket systems

### Success Criteria
- [ ] Nodes allocated on local NUMA node (>95%)
- [ ] L1 cache hit rate >85%
- [ ] L2 cache hit rate >75%
- [ ] Prefetching reduces lookup latency by 10%+
- [ ] No remote NUMA access during hot paths

### Files to Create/Modify
```
src/numa_alloc.h        (40 lines)
src/numa_alloc.c        (150 lines)
src/nary_tree.c         (modify: add prefetch)
tests/test_numa.c       (150 lines)
```

---

## 📋 PHASE 3: Proper Multithreading (FUSE)
**Duration**: 2 weeks
**Status**: BLOCKED (requires Phase 1)
**Dependencies**: Phase 1 complete

### Objectives
- ✓ Per-inode reader-writer locks (ext4-style)
- ✓ Lock ordering: always parent before child
- ✓ RCU for lock-free reads where possible
- ✓ Zero global locks on hot paths
- ✓ Deadlock prevention

### Deliverables

#### 3.1 Per-Node Locking (`src/nary_node.h` enhancement)
```c
struct __attribute__((aligned(128))) nary_node_mt {
    // Original 64-byte node data
    struct nary_node node;

    // Lock on separate cache line (prevent false sharing)
    pthread_rwlock_t lock;
    uint32_t padding[14];  // Pad to 128 bytes total
};
```

#### 3.2 Lock Ordering Discipline (`src/nary_tree_mt.c`)
```c
// LOCK ORDERING RULE: Always lock parent before child

int nary_insert_mt(struct nary_tree_mt *tree, uint32_t parent_idx,
                   const char *name, uint16_t mode) {
    struct nary_node_mt *parent = &tree->nodes[parent_idx];

    // Lock parent FIRST
    pthread_rwlock_wrlock(&parent->lock);

    // Allocate child
    uint32_t child_idx = allocate_node(tree);
    struct nary_node_mt *child = &tree->nodes[child_idx];

    // Lock child SECOND
    pthread_rwlock_wrlock(&child->lock);

    // Do insertion work
    insert_child_unlocked(parent, child, name);

    // Unlock in REVERSE order
    pthread_rwlock_unlock(&child->lock);
    pthread_rwlock_unlock(&parent->lock);

    return 0;
}

// Read operations use shared locks
int nary_lookup_mt(struct nary_tree_mt *tree, uint32_t parent_idx,
                   const char *name) {
    struct nary_node_mt *parent = &tree->nodes[parent_idx];

    pthread_rwlock_rdlock(&parent->lock);
    uint32_t result = find_child_unlocked(parent, name);
    pthread_rwlock_unlock(&parent->lock);

    return result;
}
```

#### 3.3 RCU for Hot Paths (`src/rcu.c`)
```c
// Read-Copy-Update for lock-free reads
// Follow Linux kernel RCU patterns

#define rcu_read_lock()   /* compiler barrier */
#define rcu_read_unlock() /* compiler barrier */
#define rcu_dereference(p) \
    ({ typeof(p) _p = (p); smp_rmb(); _p; })
#define rcu_assign_pointer(p, v) \
    ({ smp_wmb(); (p) = (v); })
```

### Testing Requirements
- Parallel create/delete stress test (1000 threads)
- Deadlock detection (run under lock validator)
- Race condition testing (ThreadSanitizer)
- Performance under contention

### Success Criteria
- [ ] Zero deadlocks under 1000 concurrent threads
- [ ] No data races (ThreadSanitizer clean)
- [ ] Read operations scale linearly with cores
- [ ] Write operations maintain consistency
- [ ] Lock contention <5% on benchmarks

### Files to Create/Modify
```
src/nary_tree_mt.h      (60 lines)
src/nary_tree_mt.c      (400 lines)
src/rcu.h               (80 lines)
fuse/razorfs_mt.c       (500 lines)
tests/test_mt.c         (300 lines)
```

---

## 📋 PHASE 4: POSIX Compliance (FUSE)
**Duration**: 1.5 weeks
**Status**: BLOCKED (requires Phase 1, 3)
**Dependencies**: Phase 1 and Phase 3 complete

### Objectives
- ✓ Complete delete/rmdir with parent updates
- ✓ Atomic rename operation
- ✓ Proper mtime/ctime/atime tracking
- ✓ Correct POSIX error codes
- ✓ Extended attributes support

### Deliverables

#### 4.1 Enhanced Operations (`fuse/razorfs_posix.c`)
```c
// Delete with proper parent update
static int razorfs_unlink(const char *path) {
    uint32_t parent_idx, child_idx;

    // Lock parent AND child (in order)
    pthread_rwlock_wrlock(&nodes[parent_idx].lock);
    pthread_rwlock_wrlock(&nodes[child_idx].lock);

    // Remove child
    nary_delete(tree, child_idx);

    // Update parent mtime
    nodes[parent_idx].node.mtime = time(NULL);

    pthread_rwlock_unlock(&nodes[child_idx].lock);
    pthread_rwlock_unlock(&nodes[parent_idx].lock);

    return 0;
}

// Atomic rename
static int razorfs_rename(const char *from, const char *to,
                          unsigned int flags) {
    uint32_t old_parent, new_parent, node_idx;

    // Lock both parents AND node (complex ordering)
    lock_three_nodes(old_parent, new_parent, node_idx);

    // Move node atomically
    remove_from_parent(old_parent, node_idx);
    add_to_parent(new_parent, node_idx);

    unlock_three_nodes(old_parent, new_parent, node_idx);

    return 0;
}

// Timestamp tracking
static void update_timestamps(struct nary_node *node, int flags) {
    time_t now = time(NULL);

    if (flags & UPDATE_ATIME) node->atime = now;
    if (flags & UPDATE_MTIME) node->mtime = now;
    if (flags & UPDATE_CTIME) node->ctime = now;
}
```

#### 4.2 Error Handling (`fuse/razorfs_posix.c`)
```c
// Proper POSIX error mapping
static int map_error(int internal_error) {
    switch (internal_error) {
        case NARY_NOT_FOUND:    return -ENOENT;
        case NARY_IS_DIR:       return -EISDIR;
        case NARY_NOT_DIR:      return -ENOTDIR;
        case NARY_NOT_EMPTY:    return -ENOTEMPTY;
        case NARY_EXISTS:       return -EEXIST;
        case NARY_NO_MEM:       return -ENOMEM;
        case NARY_INVALID:      return -EINVAL;
        default:                return -EIO;
    }
}
```

### Testing Requirements
- POSIX compliance test suite (pjdfstest)
- Timestamp verification
- Rename atomicity testing
- Error code validation

### Success Criteria
- [ ] Pass pjdfstest suite (>95% tests)
- [ ] Timestamps update correctly on all operations
- [ ] Rename is truly atomic (no race windows)
- [ ] All operations return correct POSIX errors
- [ ] Extended attributes work (if implemented)

### Files to Create/Modify
```
fuse/razorfs_posix.c    (600 lines)
tests/test_posix.c      (250 lines)
```

---

## 📋 PHASE 5: Simplification & Cleanup (FUSE)
**Duration**: 1 week
**Status**: BLOCKED (requires Phase 1-4)
**Dependencies**: All previous phases complete

### Objectives
- ✓ Single Makefile
- ✓ Remove all unnecessary files
- ✓ Consolidate documentation
- ✓ Code review and refactoring
- ✓ Performance validation

### Deliverables

#### 5.1 Single Makefile (`Makefile`)
```makefile
CC = gcc
CFLAGS = -std=c11 -O0 -g -Wall -Wextra -pthread
CFLAGS += $(shell pkg-config fuse3 --cflags)
LIBS = $(shell pkg-config fuse3 --libs) -lpthread -lnuma

SOURCES = src/nary_tree.c src/string_table.c src/numa_alloc.c \
          src/nary_tree_mt.c fuse/razorfs_posix.c

OBJECTS = $(SOURCES:.c=.o)

all: razorfs

razorfs: $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

clean:
	rm -f $(OBJECTS) razorfs

test: razorfs
	./tests/run_all_tests.sh

.PHONY: all clean test
```

#### 5.2 File Cleanup
```bash
# Keep only:
README.md
PHASES_IMPLEMENTATION.md
Makefile
src/nary_node.h
src/nary_tree.h
src/nary_tree.c
src/nary_tree_mt.c
src/string_table.h
src/string_table.c
src/numa_alloc.h
src/numa_alloc.c
fuse/razorfs_posix.c
tests/run_all_tests.sh
tests/test_nary_tree.c
tests/test_mt.c
tests/test_posix.c

# Delete everything else in:
fuse/*.cpp
src/*.hpp
tests/integration/*
benchmarks/*
*.py
*.sh (except tests/run_all_tests.sh)
```

#### 5.3 Documentation Update
- Update README with accurate architecture
- Document build process
- Add usage examples
- Include performance benchmarks (honest)

### Testing Requirements
- Full regression test suite
- Performance benchmarks
- Memory leak detection
- Code coverage analysis

### Success Criteria
- [ ] Single command build: `make`
- [ ] Single command test: `make test`
- [ ] <2000 lines of C code total
- [ ] Documentation matches implementation
- [ ] No memory leaks (valgrind)
- [ ] Performance meets O(log n) guarantees

---

## 📋 PHASE 6: Kernel Module (Future)
**Duration**: 4-6 weeks
**Status**: NOT STARTED
**Dependencies**: All FUSE phases complete and stable

### Objectives
- Port proven FUSE design to kernel module
- Use Linux kernel APIs (kmalloc, RCU, etc.)
- Integrate with VFS layer
- Optimize for zero-copy operations

### High-Level Plan
1. **VFS Integration** (1 week)
   - Implement inode operations
   - Implement file operations
   - Implement super operations

2. **Kernel Memory Management** (1 week)
   - Replace malloc with kmalloc
   - Use kernel slab allocator
   - Implement kernel RCU

3. **Performance Optimization** (2 weeks)
   - Zero-copy I/O
   - Kernel page cache integration
   - Direct I/O support

4. **Testing & Stabilization** (1-2 weeks)
   - Kernel stress testing
   - Crash recovery testing
   - Performance validation

**Note**: Kernel module development requires FUSE implementation to be stable and thoroughly tested first.

---

## 🎯 Current Status

### Completed
- [x] Architectural review
- [x] Phase plan created
- [x] Disclaimer added to README

### In Progress
- [ ] Phase 1: Core N-ary Tree Implementation

### Blocked
- [ ] Phase 2: NUMA & Cache Optimization (needs Phase 1)
- [ ] Phase 3: Proper Multithreading (needs Phase 1)
- [ ] Phase 4: POSIX Compliance (needs Phase 1, 3)
- [ ] Phase 5: Simplification & Cleanup (needs Phase 1-4)
- [ ] Phase 6: Kernel Module (needs all FUSE phases)

---

## 📝 Notes for Resuming Agent

### Starting Phase 1
1. Create directory structure: `mkdir -p src fuse tests`
2. Start with `src/nary_node.h` - define the 64-byte node structure
3. Implement `src/nary_tree.c` - core tree operations
4. Verify with `tests/test_nary_tree.c` before moving to FUSE
5. Keep code simple - no optimization flags initially
6. Use `valgrind` to check for memory leaks continuously

### Key Principles
- **Simplicity over cleverness**: Plain C, no templates
- **Test-driven**: Write test before implementation
- **Incremental**: One function at a time, verify it works
- **Documentation**: Comment complex algorithms
- **Performance**: Measure, don't guess

### Resources
- Reference: https://github.com/ncandio/n-ary_python_package
- FUSE docs: https://libfuse.github.io/doxygen/
- Linux kernel RCU: https://www.kernel.org/doc/Documentation/RCU/
- ext4 source: fs/ext4/ in Linux kernel tree

### Contact
- For questions about design decisions, refer to this document
- For ambiguities, prefer simpler solution
- For performance trade-offs, prioritize correctness first

---

**Last Updated**: 2025-10-02
**Document Version**: 1.0
**Status**: Ready for Phase 1 Implementation
