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
_Static_assert(sizeof(struct string_table) == 16,
               "string_table must be exactly 16 bytes");

/**
 * Initialize string table
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

    /* Grow buffer if needed */
    if (st->used + needed > st->capacity) {
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
 */
void string_table_destroy(struct string_table *st) {
    if (!st) return;

    if (st->data) {
        free(st->data);
        st->data = NULL;
    }

    st->capacity = 0;
    st->used = 0;
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