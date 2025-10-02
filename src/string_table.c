/**
 * String Table Implementation - String Interning for RAZORFS
 *
 * Simple contiguous buffer design for cache efficiency.
 */

#include "string_table.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

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

uint32_t string_table_intern(struct string_table *st, const char *str) {
    if (!st || !str) return UINT32_MAX;

    size_t len = strlen(str);
    if (len > MAX_FILENAME_LENGTH) {
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

const char *string_table_get(const struct string_table *st, uint32_t offset) {
    if (!st || offset >= st->used) {
        return NULL;
    }
    return st->data + offset;
}

void string_table_destroy(struct string_table *st) {
    if (!st) return;

    if (st->data) {
        free(st->data);
        st->data = NULL;
    }

    st->capacity = 0;
    st->used = 0;
}

void string_table_stats(const struct string_table *st,
                        uint32_t *total_size,
                        uint32_t *used_size) {
    if (!st) return;

    if (total_size) *total_size = st->capacity;
    if (used_size) *used_size = st->used;
}
