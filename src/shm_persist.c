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
    int numa_nodes = numa_init();
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

    /* Sync string table to shared memory */
    if (tree->strings.is_shm && tree->strings.data) {
        msync(tree->strings.data, STRING_TABLE_SHM_SIZE, MS_SYNC);
        munmap(tree->strings.data, STRING_TABLE_SHM_SIZE);
    }

    /* Unmap but don't destroy */
    munmap(hdr, shm_size);

    /* Clean up string table structure */
    string_table_destroy(&tree->strings);

    printf("💾 Filesystem detached - data persists in shared memory\n");
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
