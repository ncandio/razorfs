/**
 * String Table Implementation - RAZORFS Phase 5
 *
 * C implementation of string interning for filesystem nodes.
 * Eliminates dynamic string allocations that fragment cache.
 * All filenames stored in contiguous buffer, nodes store offsets only.
 */

#include "string_table.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

/* Configuration */
#define STRING_TABLE_INITIAL_SIZE (64 * 1024)      /* 64KB initial */
#define STRING_TABLE_MAX_SIZE (16 * 1024 * 1024)   /* 16MB maximum */
#define MAX_FILENAME_LENGTH 255                     /* POSIX limit */

/* Static assertions */
_Static_assert(sizeof(struct string_table) == 24,
               "string_table must be exactly 24 bytes (with is_shm field and padding)");

/**
 * Initialize string table (heap mode)
 * Returns 0 on success, -1 on failure
 */
int string_table_init(struct string_table *st) {
    if (!st) return -1;

    st->data = malloc(STRING_TABLE_INITIAL_SIZE);
    if (!st->data) {
        return -1;
    }

    st->capacity = STRING_TABLE_INITIAL_SIZE;
    st->used = 0;
    st->is_shm = 0;  /* Heap mode */

    return 0;
}

/**
 * Initialize string table with shared memory buffer
 * buf: pre-allocated shared memory buffer
 * size: buffer size
 * existing: 1 if attaching to existing data, 0 if initializing new
 * Returns 0 on success, -1 on failure
 */
int string_table_init_shm(struct string_table *st, void *buf, size_t size, int existing) {
    if (!st || !buf || size == 0) return -1;

    st->data = (char *)buf;
    st->capacity = size;
    st->is_shm = 1;  /* Shared memory mode */

    if (existing) {
        /* Attaching to existing - read used bytes from first uint32_t */
        memcpy(&st->used, buf, sizeof(uint32_t));
    } else {
        /* New buffer - initialize with used = sizeof(uint32_t) */
        st->used = sizeof(uint32_t);
        memcpy(buf, &st->used, sizeof(uint32_t));
    }

    return 0;
}

/**
 * Intern a string - store and return offset
 * If string already exists, returns existing offset.
 *
 * Returns: offset on success, UINT32_MAX on error
 */
uint32_t string_table_intern(struct string_table *st, const char *str) {
    if (!st || !str) return UINT32_MAX;
    
    size_t len = strlen(str);
    if (len == 0 || len > MAX_FILENAME_LENGTH) {
        return UINT32_MAX;
    }

    /* Check if string already exists (linear scan for now) */
    uint32_t offset = 0;
    while (offset < st->used) {
        const char *existing = st->data + offset;
        if (strcmp(existing, str) == 0) {
            return offset;  /* Found duplicate */
        }
        offset += strlen(existing) + 1;  /* Move to next string */
    }

    /* String not found - add it */
    size_t needed = len + 1;  /* Include null terminator */

    /* Grow buffer if needed (heap mode only) */
    if (st->used + needed > st->capacity) {
        if (st->is_shm) {
            /* Shared memory mode - cannot grow, table full */
            return UINT32_MAX;
        }

        /* Heap mode - can realloc */
        uint32_t new_capacity = st->capacity * 2;
        if (new_capacity > STRING_TABLE_MAX_SIZE) {
            return UINT32_MAX;  /* Table full */
        }

        char *new_data = realloc(st->data, new_capacity);
        if (!new_data) {
            return UINT32_MAX;
        }

        st->data = new_data;
        st->capacity = new_capacity;
    }

    /* Copy string to buffer */
    uint32_t new_offset = st->used;
    memcpy(st->data + new_offset, str, needed);
    st->used += needed;

    /* Update shared memory header if in shm mode */
    if (st->is_shm) {
        memcpy(st->data, &st->used, sizeof(uint32_t));
    }

    return new_offset;
}

/**
 * Get string by offset
 * Returns: pointer to string, or NULL if invalid offset
 */
const char *string_table_get(const struct string_table *st, uint32_t offset) {
    if (!st || offset >= st->used) {
        return NULL;
    }
    return st->data + offset;
}

/**
 * Free string table resources
 * Note: In shared memory mode, does NOT free the buffer
 */
void string_table_destroy(struct string_table *st) {
    if (!st) return;

    if (st->data && !st->is_shm) {
        /* Only free if heap mode */
        free(st->data);
    }

    st->data = NULL;
    st->capacity = 0;
    st->used = 0;
    st->is_shm = 0;
}

/**
 * Get statistics
 */
void string_table_stats(const struct string_table *st,
                        uint32_t *total_size,
                        uint32_t *used_size) {
    if (!st) return;

    if (total_size) *total_size = st->capacity;
    if (used_size) *used_size = st->used;
}