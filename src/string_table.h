/**
 * String Table - Interned String Storage for RAZORFS
 *
 * Purpose: Eliminate dynamic string allocations that fragment cache.
 * All filenames stored in contiguous buffer, nodes store offsets only.
 *
 * Benefits:
 * - Cache-friendly: strings stored contiguously
 * - Space-efficient: duplicate names stored once
 * - Fast comparison: offset equality check
 */

#ifndef RAZORFS_STRING_TABLE_H
#define RAZORFS_STRING_TABLE_H

#include <stdint.h>
#include <stddef.h>

/* Configuration */
#define STRING_TABLE_INITIAL_SIZE (64 * 1024)      /* 64KB initial */
#define STRING_TABLE_MAX_SIZE (16 * 1024 * 1024)   /* 16MB maximum */
#define MAX_FILENAME_LENGTH 255                     /* POSIX limit */

/**
 * String Table Structure
 *
 * Simple design: contiguous buffer of null-terminated strings.
 * No hash table - linear scan for duplicates (acceptable for filesystem scale).
 *
 * Two modes:
 * 1. Heap mode (is_shm = 0): data allocated with malloc/realloc
 * 2. Shared memory mode (is_shm = 1): data points to fixed-size shared memory
 */
struct string_table {
    char *data;              /* Contiguous string buffer */
    uint32_t capacity;       /* Total allocated size */
    uint32_t used;           /* Bytes currently used */
    int is_shm;              /* 1 if backed by shared memory, 0 if heap */
};

/**
 * Initialize string table (heap mode)
 * Returns 0 on success, -1 on failure
 */
int string_table_init(struct string_table *st);

/**
 * Initialize string table with shared memory buffer
 * buf: pre-allocated shared memory buffer
 * size: buffer size
 * existing: 1 if attaching to existing data, 0 if initializing new
 * Returns 0 on success, -1 on failure
 */
int string_table_init_shm(struct string_table *st, void *buf, size_t size, int existing);

/**
 * Intern a string - store and return offset
 * If string already exists, returns existing offset.
 *
 * Returns: offset on success, UINT32_MAX on error
 */
uint32_t string_table_intern(struct string_table *st, const char *str);

/**
 * Get string by offset
 * Returns: pointer to string, or NULL if invalid offset
 */
const char *string_table_get(const struct string_table *st, uint32_t offset);

/**
 * Free string table resources
 */
void string_table_destroy(struct string_table *st);

/**
 * Get statistics
 */
void string_table_stats(const struct string_table *st,
                        uint32_t *total_size,
                        uint32_t *used_size);

#endif /* RAZORFS_STRING_TABLE_H */
