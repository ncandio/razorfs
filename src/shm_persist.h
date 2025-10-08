/**
 * Shared Memory Persistence - RAZORFS Phase 6
 *
 * Simple persistence via shared memory:
 * - Tree nodes in one shared memory region
 * - String table in another region
 * - File data in individual regions
 * - No daemon needed - survives mount/unmount
 */

#ifndef RAZORFS_SHM_PERSIST_H
#define RAZORFS_SHM_PERSIST_H

#include "nary_tree_mt.h"
#include <stdint.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Shared memory region names (volatile - cleared on reboot) */
#define SHM_TREE_NODES   "/razorfs_nodes"
#define SHM_STRING_TABLE "/razorfs_strings"
#define SHM_FILE_PREFIX  "/razorfs_file_"

/* Disk-backed storage paths (persistent - survives reboot) */
#define DISK_DATA_DIR     "/tmp/razorfs_data"
#define DISK_TREE_NODES   "/tmp/razorfs_data/nodes.dat"
#define DISK_STRING_TABLE "/tmp/razorfs_data/strings.dat"
#define DISK_FILE_PREFIX  "/tmp/razorfs_data/file_"

/* Shared memory tree structure header */
struct shm_tree_header {
    uint32_t magic;            /* Magic number for validation */
    uint32_t version;          /* Format version */
    uint32_t capacity;         /* Total node capacity */
    uint32_t used;             /* Nodes in use */
    uint32_t next_inode;       /* Next inode number */
    uint32_t free_count;       /* Free list count */
};

#define SHM_MAGIC 0x52415A4F    /* "RAZO" */
#define SHM_VERSION 1

/**
 * Initialize tree from shared memory (attach if exists, create if not)
 * VOLATILE: Data cleared on reboot
 * Returns 0 on success, -1 on failure
 */
int shm_tree_init(struct nary_tree_mt *tree);

/**
 * Initialize tree from disk-backed files (attach if exists, create if not)
 * PERSISTENT: Data survives reboot
 * Returns 0 on success, -1 on failure
 */
int disk_tree_init(struct nary_tree_mt *tree);

/**
 * Detach from shared memory (data persists)
 */
void shm_tree_detach(struct nary_tree_mt *tree);

/**
 * Destroy shared memory (permanently delete)
 */
void shm_tree_destroy(struct nary_tree_mt *tree) __attribute__((unused));

/**
 * Check if shared memory already exists
 * Returns 1 if exists, 0 if not
 */
int shm_tree_exists(void);

/**
 * Check if disk-backed storage already exists
 * Returns 1 if exists, 0 if not
 */
int disk_tree_exists(void);

/**
 * Initialize string table persistence to disk
 * Creates/persists string table to disk file
 *
 * @param st String table to persist
 * @param filepath Path to persistence file
 * @return 0 on success, -1 on failure
 */
int disk_string_table_save(const struct string_table *st, const char *filepath);

/**
 * Load string table from disk persistence
 * Loads string table from disk file
 *
 * @param st String table to load into
 * @param filepath Path to persistence file
 * @return 0 on success, -1 on failure
 */
int disk_string_table_load(struct string_table *st, const char *filepath);

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
                        size_t data_size, int is_compressed);

/**
 * Restore file data from shared memory
 * Reads from /dev/shm/razorfs_file_<inode>
 *
 * @param inode Inode number
 * @param data_out Output pointer to receive allocated data (caller must free)
 * @param size_out Output pointer for original (uncompressed) size
 * @param data_size_out Output pointer for actual data size (may be NULL)
 * @param is_compressed_out Output pointer for compression flag (may be NULL)
 * @return 0 on success, -1 if not found or error
 */
int shm_file_data_restore(uint32_t inode, void **data_out, size_t *size_out,
                           size_t *data_size_out, int *is_compressed_out);

/**
 * Remove file data from shared memory
 * Called when a file is deleted
 *
 * @param inode Inode number
 */
void shm_file_data_remove(uint32_t inode);

/**
 * Disk-backed file data operations (PERSISTENT)
 * Same interface as shm_* but uses disk files instead
 */
int disk_file_data_save(uint32_t inode, const void *data, size_t size,
                        size_t data_size, int is_compressed);
int disk_file_data_restore(uint32_t inode, void **data_out, size_t *size_out,
                           size_t *data_size_out, int *is_compressed_out);
void disk_file_data_remove(uint32_t inode);

#ifdef __cplusplus
}
#endif

#endif /* RAZORFS_SHM_PERSIST_H */
