/**
 * Shared Memory Persistence Implementation - RAZORFS Phase 6
 */

#define _GNU_SOURCE
#include "shm_persist.h"
#include "numa_support.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>

/* Calculate total shared memory size needed */
static size_t calculate_shm_size(uint32_t capacity) {
    return sizeof(struct shm_tree_header) +
           (capacity * sizeof(struct nary_node_mt)) +
           (capacity * sizeof(uint16_t));  /* free_list */
}

/* String table size for shared memory */
#define STRING_TABLE_SHM_SIZE (1024 * 1024)  /* 1MB for strings */

int shm_tree_exists(void) {
    int fd = shm_open(SHM_TREE_NODES, O_RDONLY, 0);
    if (fd < 0) {
        return 0;  /* Doesn't exist */
    }
    close(fd);
    return 1;  /* Exists */
}

int shm_tree_init(struct nary_tree_mt *tree) {
    if (!tree) return -1;

    /* Initialize NUMA support */
    numa_init();
    int numa_node = numa_get_current_node();

    int is_new = !shm_tree_exists();
    int flags = O_RDWR | (is_new ? O_CREAT : 0);
    size_t shm_size = calculate_shm_size(NARY_MT_INITIAL_CAPACITY);

    /* Open/create shared memory */
    int fd = shm_open(SHM_TREE_NODES, flags, 0600);
    if (fd < 0) {
        perror("shm_open");
        return -1;
    }

    /* Set size for new region */
    if (is_new) {
        if (ftruncate(fd, shm_size) < 0) {
            perror("ftruncate");
            close(fd);
            shm_unlink(SHM_TREE_NODES);
            return -1;
        }
    }

    /* Map shared memory */
    void *addr = mmap(NULL, shm_size, PROT_READ | PROT_WRITE,
                      MAP_SHARED, fd, 0);
    close(fd);  /* Can close fd after mmap */

    if (addr == MAP_FAILED) {
        perror("mmap");
        if (is_new) shm_unlink(SHM_TREE_NODES);
        return -1;
    }

    /* Bind to NUMA node if available */
    if (numa_available()) {
        if (numa_bind_memory(addr, shm_size, numa_node) == 0) {
            printf("📍 NUMA: Bound shared memory to node %d\n", numa_node);
        }
    }

    /* Setup tree structure */
    struct shm_tree_header *hdr = (struct shm_tree_header *)addr;

    if (is_new) {
        /* Initialize new shared memory */
        printf("🆕 Creating new persistent filesystem\n");

        hdr->magic = SHM_MAGIC;
        hdr->version = SHM_VERSION;
        hdr->capacity = NARY_MT_INITIAL_CAPACITY;
        hdr->used = 0;
        hdr->next_inode = 1;
        hdr->free_count = 0;

        /* Setup pointers */
        tree->nodes = (struct nary_node_mt *)(hdr + 1);
        tree->free_list = (uint16_t *)(tree->nodes + hdr->capacity);

        tree->capacity = hdr->capacity;
        tree->used = 0;
        tree->next_inode = 1;
        tree->op_count = 0;
        tree->free_count = 0;

        /* Create string table shared memory */
        int str_fd = shm_open(SHM_STRING_TABLE, O_RDWR | O_CREAT, 0600);
        if (str_fd < 0) {
            perror("shm_open (strings)");
            munmap(addr, shm_size);
            shm_unlink(SHM_TREE_NODES);
            return -1;
        }

        if (ftruncate(str_fd, STRING_TABLE_SHM_SIZE) < 0) {
            perror("ftruncate (strings)");
            close(str_fd);
            munmap(addr, shm_size);
            shm_unlink(SHM_TREE_NODES);
            shm_unlink(SHM_STRING_TABLE);
            return -1;
        }

        void *str_buf = mmap(NULL, STRING_TABLE_SHM_SIZE, PROT_READ | PROT_WRITE,
                             MAP_SHARED, str_fd, 0);
        close(str_fd);

        if (str_buf == MAP_FAILED) {
            perror("mmap (strings)");
            munmap(addr, shm_size);
            shm_unlink(SHM_TREE_NODES);
            shm_unlink(SHM_STRING_TABLE);
            return -1;
        }

        /* Initialize string table in shared memory */
        if (string_table_init_shm(&tree->strings, str_buf, STRING_TABLE_SHM_SIZE, 0) != 0) {
            munmap(str_buf, STRING_TABLE_SHM_SIZE);
            munmap(addr, shm_size);
            shm_unlink(SHM_TREE_NODES);
            shm_unlink(SHM_STRING_TABLE);
            return -1;
        }

        /* Initialize tree lock */
        pthread_rwlockattr_t attr;
        pthread_rwlockattr_init(&attr);
        pthread_rwlockattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
        pthread_rwlock_init(&tree->tree_lock, &attr);
        pthread_rwlockattr_destroy(&attr);

        /* Create root directory */
        memset(&tree->nodes[0], 0, sizeof(struct nary_node_mt));

        pthread_rwlockattr_init(&attr);
        pthread_rwlockattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
        pthread_rwlock_init(&tree->nodes[0].lock, &attr);
        pthread_rwlockattr_destroy(&attr);

        tree->nodes[0].node.inode = tree->next_inode++;
        tree->nodes[0].node.parent_idx = NARY_INVALID_IDX;
        tree->nodes[0].node.num_children = 0;
        tree->nodes[0].node.mode = S_IFDIR | 0755;
        tree->nodes[0].node.name_offset = string_table_intern(&tree->strings, "/");
        tree->nodes[0].node.size = 0;
        tree->nodes[0].node.mtime = time(NULL);

        for (int i = 0; i < NARY_BRANCHING_FACTOR; i++) {
            tree->nodes[0].node.children[i] = NARY_INVALID_IDX;
        }

        tree->used = 1;
        hdr->used = 1;
        hdr->next_inode = tree->next_inode;

    } else {
        /* Attach to existing shared memory */
        printf("♻️  Attaching to existing persistent filesystem\n");

        /* Validate magic */
        if (hdr->magic != SHM_MAGIC) {
            fprintf(stderr, "Invalid shared memory magic: 0x%x\n", hdr->magic);
            munmap(addr, shm_size);
            return -1;
        }

        /* Setup pointers */
        tree->nodes = (struct nary_node_mt *)(hdr + 1);
        tree->free_list = (uint16_t *)(tree->nodes + hdr->capacity);

        tree->capacity = hdr->capacity;
        tree->used = hdr->used;
        tree->next_inode = hdr->next_inode;
        tree->op_count = 0;
        tree->free_count = hdr->free_count;

        /* Attach to existing string table shared memory */
        int str_fd = shm_open(SHM_STRING_TABLE, O_RDWR, 0600);
        if (str_fd < 0) {
            perror("shm_open (strings)");
            munmap(addr, shm_size);
            return -1;
        }

        void *str_buf = mmap(NULL, STRING_TABLE_SHM_SIZE, PROT_READ | PROT_WRITE,
                             MAP_SHARED, str_fd, 0);
        close(str_fd);

        if (str_buf == MAP_FAILED) {
            perror("mmap (strings)");
            munmap(addr, shm_size);
            return -1;
        }

        /* Attach to existing string table in shared memory */
        if (string_table_init_shm(&tree->strings, str_buf, STRING_TABLE_SHM_SIZE, 1) != 0) {
            munmap(str_buf, STRING_TABLE_SHM_SIZE);
            munmap(addr, shm_size);
            return -1;
        }

        /* Tree lock is already initialized in shared memory */
        printf("📊 Restored %u nodes, next inode: %u\n", tree->used, tree->next_inode);
    }

    memset(&tree->stats, 0, sizeof(tree->stats));
    tree->stats.total_nodes = tree->used;

    return 0;
}

void shm_tree_detach(struct nary_tree_mt *tree) {
    if (!tree || !tree->nodes) return;

    /* Sync header before detaching */
    struct shm_tree_header *hdr = ((struct shm_tree_header *)tree->nodes) - 1;
    hdr->used = tree->used;
    hdr->next_inode = tree->next_inode;
    hdr->free_count = tree->free_count;

    /* Ensure all changes written to shared memory */
    size_t shm_size = calculate_shm_size(tree->capacity);
    msync(hdr, shm_size, MS_SYNC);

    /* For disk-backed storage, persist string table */
    if (!tree->strings.is_shm) {
        // Try to save string table to disk if using file-backed persistence
        if (disk_tree_exists()) {
            disk_string_table_save(&tree->strings, DISK_STRING_TABLE);
        }
    } else if (tree->strings.data) {
        /* For shared memory mode, sync string table */
        msync(tree->strings.data, STRING_TABLE_SHM_SIZE, MS_SYNC);
        munmap(tree->strings.data, STRING_TABLE_SHM_SIZE);
    }

    /* Unmap but don't destroy */
    munmap(hdr, shm_size);

    /* Clean up string table structure */
    string_table_destroy(&tree->strings);

    printf("💾 Filesystem detached - data persists in disk storage\n");
}

void shm_tree_destroy(struct nary_tree_mt *tree) {
    if (!tree || !tree->nodes) return;

    /* Unmap string table shared memory */
    if (tree->strings.is_shm && tree->strings.data) {
        munmap(tree->strings.data, STRING_TABLE_SHM_SIZE);
    }

    /* Unmap shared memory */
    struct shm_tree_header *hdr = ((struct shm_tree_header *)tree->nodes) - 1;
    size_t shm_size = calculate_shm_size(tree->capacity);
    munmap(hdr, shm_size);

    /* Destroy locks */
    pthread_rwlock_destroy(&tree->tree_lock);
    for (uint32_t i = 0; i < tree->used; i++) {
        pthread_rwlock_destroy(&tree->nodes[i].lock);
    }

    /* Remove shared memory regions */
    shm_unlink(SHM_TREE_NODES);
    shm_unlink(SHM_STRING_TABLE);

    /* Clean up string table structure */
    string_table_destroy(&tree->strings);

    printf("🗑️  Persistent filesystem destroyed\n");
}

/* === File Data Persistence === */

/**
 * File data header in shared memory
 */
struct shm_file_header {
    uint32_t magic;              /* Magic number for validation */
    uint32_t inode;              /* Inode number */
    size_t size;                 /* Uncompressed file size */
    size_t data_size;            /* Actual data size (may be compressed) */
    int is_compressed;           /* 1 if data is compressed */
};

#define SHM_FILE_MAGIC 0x46494C45  /* "FILE" */

/**
 * Save file data to shared memory
 * Creates/updates /dev/shm/razorfs_file_<inode>
 *
 * @param inode Inode number
 * @param data File data (may be compressed)
 * @param size Original (uncompressed) size
 * @param data_size Actual data size (compressed size if compressed)
 * @param is_compressed 1 if data is compressed, 0 otherwise
 * @return 0 on success, -1 on failure
 */
int shm_file_data_save(uint32_t inode, const void *data, size_t size,
                        size_t data_size, int is_compressed) {
    if (!data || data_size == 0) return -1;

    /* Create shared memory name */
    char shm_name[64];
    snprintf(shm_name, sizeof(shm_name), "%s%u", SHM_FILE_PREFIX, inode);

    /* Calculate total size needed */
    size_t total_size = sizeof(struct shm_file_header) + data_size;

    /* Create/open shared memory */
    int fd = shm_open(shm_name, O_RDWR | O_CREAT, 0600);
    if (fd < 0) {
        perror("shm_open (file data)");
        return -1;
    }

    /* Set size */
    if (ftruncate(fd, total_size) < 0) {
        perror("ftruncate (file data)");
        close(fd);
        shm_unlink(shm_name);
        return -1;
    }

    /* Map memory */
    void *addr = mmap(NULL, total_size, PROT_READ | PROT_WRITE,
                      MAP_SHARED, fd, 0);
    close(fd);

    if (addr == MAP_FAILED) {
        perror("mmap (file data)");
        shm_unlink(shm_name);
        return -1;
    }

    /* Write header */
    struct shm_file_header *hdr = (struct shm_file_header *)addr;
    hdr->magic = SHM_FILE_MAGIC;
    hdr->inode = inode;
    hdr->size = size;
    hdr->data_size = data_size;
    hdr->is_compressed = is_compressed;

    /* Write data */
    memcpy(hdr + 1, data, data_size);

    /* Sync to disk */
    msync(addr, total_size, MS_SYNC);

    /* Unmap */
    munmap(addr, total_size);

    return 0;
}

/**
 * Restore file data from shared memory
 * Reads from /dev/shm/razorfs_file_<inode>
 *
 * @param inode Inode number
 * @param data_out Output pointer to receive allocated data (caller must free)
 * @param size_out Output pointer for original (uncompressed) size
 * @param data_size_out Output pointer for actual data size
 * @param is_compressed_out Output pointer for compression flag
 * @return 0 on success, -1 if not found or error
 */
int shm_file_data_restore(uint32_t inode, void **data_out, size_t *size_out,
                           size_t *data_size_out, int *is_compressed_out) {
    if (!data_out || !size_out) return -1;

    /* Create shared memory name */
    char shm_name[64];
    snprintf(shm_name, sizeof(shm_name), "%s%u", SHM_FILE_PREFIX, inode);

    /* Try to open existing shared memory */
    int fd = shm_open(shm_name, O_RDONLY, 0);
    if (fd < 0) {
        /* File has no persisted data (not an error) */
        return -1;
    }

    /* Get size */
    struct stat st;
    if (fstat(fd, &st) < 0) {
        perror("fstat (file data)");
        close(fd);
        return -1;
    }

    if (st.st_size < (off_t)sizeof(struct shm_file_header)) {
        fprintf(stderr, "Invalid file data size for inode %u\n", inode);
        close(fd);
        return -1;
    }

    /* Map memory */
    void *addr = mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
    close(fd);

    if (addr == MAP_FAILED) {
        perror("mmap (file data restore)");
        return -1;
    }

    /* Read header */
    const struct shm_file_header *hdr = (const struct shm_file_header *)addr;

    /* Validate magic */
    if (hdr->magic != SHM_FILE_MAGIC) {
        fprintf(stderr, "Invalid file data magic for inode %u: 0x%x\n",
                inode, hdr->magic);
        munmap(addr, st.st_size);
        return -1;
    }

    /* Allocate memory for data */
    *data_out = malloc(hdr->data_size);
    if (!*data_out) {
        munmap(addr, st.st_size);
        return -1;
    }

    /* Copy data */
    memcpy(*data_out, hdr + 1, hdr->data_size);
    *size_out = hdr->size;
    if (data_size_out) *data_size_out = hdr->data_size;
    if (is_compressed_out) *is_compressed_out = hdr->is_compressed;

    /* Unmap */
    munmap(addr, st.st_size);

    return 0;
}

/**
 * Remove file data from shared memory
 * Called when a file is deleted
 *
 * @param inode Inode number
 */
void shm_file_data_remove(uint32_t inode) {
    char shm_name[64];
    snprintf(shm_name, sizeof(shm_name), "%s%u", SHM_FILE_PREFIX, inode);
    shm_unlink(shm_name);  /* Ignore errors - file may not have persisted data */
}

/* === DISK-BACKED PERSISTENCE (Survives Reboot) === */

int disk_tree_exists(void) {
    struct stat st;
    return (stat(DISK_TREE_NODES, &st) == 0);
}

/**
 * Create data directory if it doesn't exist
 */
static int ensure_data_dir(void) {
    struct stat st;
    if (stat(DISK_DATA_DIR, &st) == 0) {
        return 0;  /* Already exists */
    }

    if (mkdir(DISK_DATA_DIR, 0700) < 0) {
        if (errno != EEXIST) {
            perror("mkdir (data dir)");
            return -1;
        }
    }
    return 0;
}

int disk_tree_init(struct nary_tree_mt *tree) {
    if (!tree) return -1;

    /* Ensure data directory exists */
    if (ensure_data_dir() < 0) {
        return -1;
    }

    /* Initialize NUMA support */
    numa_init();
    int numa_node = numa_get_current_node();

    int is_new = !disk_tree_exists();
    int flags = O_RDWR | (is_new ? O_CREAT : 0);
    size_t shm_size = calculate_shm_size(NARY_MT_INITIAL_CAPACITY);

    /* Open/create disk-backed file for tree nodes */
    int fd = open(DISK_TREE_NODES, flags, 0600);
    if (fd < 0) {
        perror("open (tree nodes)");
        return -1;
    }

    /* Set size for new file */
    if (is_new) {
        if (ftruncate(fd, shm_size) < 0) {
            perror("ftruncate (tree nodes)");
            close(fd);
            unlink(DISK_TREE_NODES);
            return -1;
        }
    }

    /* Map file to memory */
    void *addr = mmap(NULL, shm_size, PROT_READ | PROT_WRITE,
                      MAP_SHARED, fd, 0);
    close(fd);  /* Can close fd after mmap */

    if (addr == MAP_FAILED) {
        perror("mmap (tree nodes)");
        if (is_new) unlink(DISK_TREE_NODES);
        return -1;
    }

    /* Bind to NUMA node if available */
    if (numa_available()) {
        if (numa_bind_memory(addr, shm_size, numa_node) == 0) {
            printf("📍 NUMA: Bound disk-backed memory to node %d\n", numa_node);
        }
    }

    /* Setup tree structure */
    struct shm_tree_header *hdr = (struct shm_tree_header *)addr;

    if (is_new) {
        /* Initialize new disk-backed storage */
        printf("🆕 Creating new PERSISTENT filesystem (disk-backed)\n");

        hdr->magic = SHM_MAGIC;
        hdr->version = SHM_VERSION;
        hdr->capacity = NARY_MT_INITIAL_CAPACITY;
        hdr->used = 0;
        hdr->next_inode = 1;
        hdr->free_count = 0;

        /* Setup pointers */
        tree->nodes = (struct nary_node_mt *)(hdr + 1);
        tree->free_list = (uint16_t *)(tree->nodes + hdr->capacity);

        tree->capacity = hdr->capacity;
        tree->used = 0;
        tree->next_inode = 1;
        tree->op_count = 0;
        tree->free_count = 0;

        /* Initialize string table in heap mode (we'll persist separately) */
        if (string_table_init(&tree->strings) != 0) {
            munmap(addr, shm_size);
            unlink(DISK_TREE_NODES);
            return -1;
        }

        /* Initialize tree lock */
        pthread_rwlockattr_t attr;
        pthread_rwlockattr_init(&attr);
        pthread_rwlockattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
        pthread_rwlock_init(&tree->tree_lock, &attr);
        pthread_rwlockattr_destroy(&attr);

        /* Create root directory */
        memset(&tree->nodes[0], 0, sizeof(struct nary_node_mt));

        pthread_rwlockattr_init(&attr);
        pthread_rwlockattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
        pthread_rwlock_init(&tree->nodes[0].lock, &attr);
        pthread_rwlockattr_destroy(&attr);

        tree->nodes[0].node.inode = tree->next_inode++;
        tree->nodes[0].node.parent_idx = NARY_INVALID_IDX;
        tree->nodes[0].node.num_children = 0;
        tree->nodes[0].node.mode = S_IFDIR | 0755;
        tree->nodes[0].node.name_offset = string_table_intern(&tree->strings, "/");
        tree->nodes[0].node.size = 0;
        tree->nodes[0].node.mtime = time(NULL);

        for (int i = 0; i < NARY_BRANCHING_FACTOR; i++) {
            tree->nodes[0].node.children[i] = NARY_INVALID_IDX;
        }

        tree->used = 1;
        hdr->used = 1;
        hdr->next_inode = tree->next_inode;

        /* Persist string table to disk */
        if (disk_string_table_save(&tree->strings, DISK_STRING_TABLE) != 0) {
            fprintf(stderr, "Failed to save string table to disk\n");
        }

        printf("💾 Disk-backed storage: %s, %s\n", DISK_TREE_NODES, DISK_STRING_TABLE);

    } else {
        /* Attach to existing disk-backed storage */
        printf("♻️  Attaching to existing PERSISTENT filesystem (disk-backed)\n");

        /* Validate magic */
        if (hdr->magic != SHM_MAGIC) {
            fprintf(stderr, "Invalid disk storage magic: 0x%x\n", hdr->magic);
            munmap(addr, shm_size);
            return -1;
        }

        /* Setup pointers */
        tree->nodes = (struct nary_node_mt *)(hdr + 1);
        tree->free_list = (uint16_t *)(tree->nodes + hdr->capacity);

        tree->capacity = hdr->capacity;
        tree->used = hdr->used;
        tree->next_inode = hdr->next_inode;
        tree->op_count = 0;
        tree->free_count = hdr->free_count;

        /* Initialize string table in heap mode and load from disk */
        if (string_table_init(&tree->strings) != 0) {
            munmap(addr, shm_size);
            return -1;
        }

        /* Load string table from disk */
        if (disk_string_table_load(&tree->strings, DISK_STRING_TABLE) != 0) {
            fprintf(stderr, "Failed to load string table from disk\n");
            string_table_destroy(&tree->strings);
            munmap(addr, shm_size);
            return -1;
        }

        /* Tree lock is already initialized in disk storage */
        printf("📊 Restored %u nodes, next inode: %u (from disk)\n", tree->used, tree->next_inode);
    }

    memset(&tree->stats, 0, sizeof(tree->stats));
    tree->stats.total_nodes = tree->used;

    return 0;
}

/* Disk-backed file data operations */

int disk_file_data_save(uint32_t inode, const void *data, size_t size,
                        size_t data_size, int is_compressed) {
    if (!data || data_size == 0) return -1;

    /* Ensure data directory exists */
    if (ensure_data_dir() < 0) {
        return -1;
    }

    /* Create file path */
    char filepath[256];
    snprintf(filepath, sizeof(filepath), "%s%u", DISK_FILE_PREFIX, inode);

    /* Calculate total size needed */
    size_t total_size = sizeof(struct shm_file_header) + data_size;

    /* Create/open file */
    int fd = open(filepath, O_RDWR | O_CREAT, 0600);
    if (fd < 0) {
        perror("open (file data)");
        return -1;
    }

    /* Set size */
    if (ftruncate(fd, total_size) < 0) {
        perror("ftruncate (file data)");
        close(fd);
        unlink(filepath);
        return -1;
    }

    /* Map memory */
    void *addr = mmap(NULL, total_size, PROT_READ | PROT_WRITE,
                      MAP_SHARED, fd, 0);
    close(fd);

    if (addr == MAP_FAILED) {
        perror("mmap (file data)");
        unlink(filepath);
        return -1;
    }

    /* Write header */
    struct shm_file_header *hdr = (struct shm_file_header *)addr;
    hdr->magic = SHM_FILE_MAGIC;
    hdr->inode = inode;
    hdr->size = size;
    hdr->data_size = data_size;
    hdr->is_compressed = is_compressed;

    /* Write data */
    memcpy(hdr + 1, data, data_size);

    /* Sync to disk */
    msync(addr, total_size, MS_SYNC);

    /* Unmap */
    munmap(addr, total_size);

    return 0;
}

int disk_file_data_restore(uint32_t inode, void **data_out, size_t *size_out,
                           size_t *data_size_out, int *is_compressed_out) {
    if (!data_out || !size_out) return -1;

    /* Create file path */
    char filepath[256];
    snprintf(filepath, sizeof(filepath), "%s%u", DISK_FILE_PREFIX, inode);

    /* Try to open existing file */
    int fd = open(filepath, O_RDONLY, 0);
    if (fd < 0) {
        /* File has no persisted data (not an error) */
        return -1;
    }

    /* Get size */
    struct stat st;
    if (fstat(fd, &st) < 0) {
        perror("fstat (file data)");
        close(fd);
        return -1;
    }

    if (st.st_size < (off_t)sizeof(struct shm_file_header)) {
        fprintf(stderr, "Invalid file data size for inode %u\n", inode);
        close(fd);
        return -1;
    }

    /* Map memory */
    void *addr = mmap(NULL, st.st_size, PROT_READ, MAP_SHARED, fd, 0);
    close(fd);

    if (addr == MAP_FAILED) {
        perror("mmap (file data restore)");
        return -1;
    }

    /* Read header */
    const struct shm_file_header *hdr = (const struct shm_file_header *)addr;

    /* Validate magic */
    if (hdr->magic != SHM_FILE_MAGIC) {
        fprintf(stderr, "Invalid file data magic for inode %u: 0x%x\n",
                inode, hdr->magic);
        munmap(addr, st.st_size);
        return -1;
    }

    /* Allocate memory for data */
    *data_out = malloc(hdr->data_size);
    if (!*data_out) {
        munmap(addr, st.st_size);
        return -1;
    }

    /* Copy data */
    memcpy(*data_out, hdr + 1, hdr->data_size);
    *size_out = hdr->size;
    if (data_size_out) *data_size_out = hdr->data_size;
    if (is_compressed_out) *is_compressed_out = hdr->is_compressed;

    /* Unmap */
    munmap(addr, st.st_size);

    return 0;
}

void disk_file_data_remove(uint32_t inode) {
    char filepath[256];
    snprintf(filepath, sizeof(filepath), "%s%u", DISK_FILE_PREFIX, inode);
    unlink(filepath);  /* Ignore errors - file may not have persisted data */
}

/* === Disk-Backed String Table Persistence === */

/**
 * Save string table to disk
 * Creates/persists string table to disk file
 */
int disk_string_table_save(const struct string_table *st, const char *filepath) {
    if (!st || !filepath) return -1;
    if (!st->data) return -1;

    /* Ensure data directory exists */
    if (ensure_data_dir() < 0) {
        return -1;
    }

    int fd = open(filepath, O_RDWR | O_CREAT, 0600);
    if (fd < 0) {
        perror("open (string table)");
        return -1;
    }

    /* Set file size */
    size_t total_size = sizeof(uint32_t) + st->used;  /* used size stored at beginning */
    if (ftruncate(fd, total_size) < 0) {
        perror("ftruncate (string table)");
        close(fd);
        return -1;
    }

    /* Map file for writing */
    void *addr = mmap(NULL, total_size, PROT_READ | PROT_WRITE,
                      MAP_SHARED, fd, 0);
    close(fd);

    if (addr == MAP_FAILED) {
        perror("mmap (string table save)");
        return -1;
    }

    /* Write used size first, then string data */
    memcpy(addr, &st->used, sizeof(uint32_t));
    memcpy((char*)addr + sizeof(uint32_t), st->data, st->used);

    /* Sync to disk */
    if (msync(addr, total_size, MS_SYNC) != 0) {
        perror("msync (string table)");
        munmap(addr, total_size);
        return -1;
    }

    /* Unmap */
    munmap(addr, total_size);

    return 0;
}

/**
 * Load string table from disk
 * Loads string table from disk file
 */
int disk_string_table_load(struct string_table *st, const char *filepath) {
    if (!st || !filepath) return -1;

    int fd = open(filepath, O_RDONLY, 0);
    if (fd < 0) {
        /* File may not exist yet, which is OK */
        return -1;
    }

    /* Get file size */
    struct stat st_info;
    if (fstat(fd, &st_info) < 0) {
        perror("fstat (string table)");
        close(fd);
        return -1;
    }

    if (st_info.st_size < (off_t)sizeof(uint32_t)) {
        fprintf(stderr, "Invalid string table file size\n");
        close(fd);
        return -1;
    }

    /* Map file for reading */
    void *addr = mmap(NULL, st_info.st_size, PROT_READ, MAP_SHARED, fd, 0);
    close(fd);

    if (addr == MAP_FAILED) {
        perror("mmap (string table load)");
        return -1;
    }

    /* Read used size first */
    memcpy(&st->used, addr, sizeof(uint32_t));
    
    /* Calculate actual data size */
    size_t data_size = st_info.st_size - sizeof(uint32_t);
    
    /* Allocate space for data */
    st->data = malloc(data_size);
    if (!st->data) {
        munmap(addr, st_info.st_size);
        return -1;
    }

    /* Copy string data */
    memcpy(st->data, (char*)addr + sizeof(uint32_t), data_size);
    munmap(addr, st_info.st_size);

    /* Set up capacity based on data read */
    st->capacity = data_size;
    st->is_shm = 0;  /* Not in shared memory mode for loaded string table */

    /* Rebuild hash table from loaded strings (basic implementation) */
    /* In a full implementation, we'd need to reconstruct the hash table */
    /* For now, we'll just mark that we need to rebuild it elsewhere if needed */

    return 0;
}
