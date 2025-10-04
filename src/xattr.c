/**
 * Extended Attributes (xattr) Implementation
 */

#include "xattr.h"
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>

/* Initialize xattr subsystem */
int xattr_init(struct xattr_pool *pool,
               struct xattr_value_pool *values,
               struct string_table *names,
               uint32_t max_entries,
               uint32_t value_pool_size) {
    if (!pool || !values || !names) return -1;

    /* Initialize entry pool */
    memset(pool, 0, sizeof(*pool));
    pool->entries = calloc(max_entries, sizeof(struct xattr_entry));
    if (!pool->entries) {
        return -1;
    }
    pool->capacity = max_entries;
    pool->used = 1;  /* Reserve index 0 as invalid */
    pool->free_head = 0;

    if (pthread_rwlock_init(&pool->lock, NULL) != 0) {
        free(pool->entries);
        return -1;
    }

    /* Initialize value pool */
    memset(values, 0, sizeof(*values));
    values->buffer = malloc(value_pool_size);
    if (!values->buffer) {
        pthread_rwlock_destroy(&pool->lock);
        free(pool->entries);
        return -1;
    }
    values->capacity = value_pool_size;
    values->used = 0;
    values->free_head = 0;

    if (pthread_rwlock_init(&values->lock, NULL) != 0) {
        free(values->buffer);
        pthread_rwlock_destroy(&pool->lock);
        free(pool->entries);
        return -1;
    }

    return 0;
}

/* Cleanup xattr subsystem */
void xattr_destroy(struct xattr_pool *pool,
                   struct xattr_value_pool *values) {
    if (!pool || !values) return;

    if (pool->entries) {
        pthread_rwlock_destroy(&pool->lock);
        free(pool->entries);
        pool->entries = NULL;
    }

    if (values->buffer) {
        pthread_rwlock_destroy(&values->lock);
        free(values->buffer);
        values->buffer = NULL;
    }
}

/* Validate xattr name and extract namespace */
int xattr_validate_name(const char *name, uint8_t *flags_out) {
    if (!name || !flags_out) return -EINVAL;

    size_t len = strlen(name);
    if (len == 0 || len > XATTR_NAME_MAX) {
        return -ENAMETOOLONG;
    }

    /* Check namespace prefix */
    if (strncmp(name, "security.", 9) == 0) {
        *flags_out = XATTR_NS_SECURITY;
        return 0;
    }
    if (strncmp(name, "system.", 7) == 0) {
        *flags_out = XATTR_NS_SYSTEM;
        return 0;
    }
    if (strncmp(name, "user.", 5) == 0) {
        *flags_out = XATTR_NS_USER;
        return 0;
    }
    if (strncmp(name, "trusted.", 8) == 0) {
        *flags_out = XATTR_NS_TRUSTED;
        return 0;
    }

    return -EOPNOTSUPP;  /* Invalid namespace */
}

/* Check if value size is valid */
int xattr_validate_size(size_t size) {
    if (size > XATTR_SIZE_MAX) {
        return -E2BIG;
    }
    return 0;
}

/* Allocate value storage */
static uint32_t allocate_value(struct xattr_value_pool *values,
                               const void *value, size_t size) {
    if (size == 0) return 0;

    /* Simple bump allocator for now */
    /* TODO: Implement free list for reclamation */

    /* Start at offset 1 (reserve 0 for "no value") */
    if (values->used == 0) {
        values->used = 1;
    }

    if (values->used + size > values->capacity) {
        return 0;  /* No space */
    }

    uint32_t offset = values->used;
    memcpy(values->buffer + offset, value, size);
    values->used += size;

    return offset;
}

/* Free value storage */
static void free_value(struct xattr_value_pool *values,
                      uint32_t offset, uint32_t size) {
    /* Simple implementation: just mark as unused */
    /* TODO: Add to free list for reuse */
    (void)values;
    (void)offset;
    (void)size;
}

/* Allocate xattr entry */
static uint32_t allocate_entry(struct xattr_pool *pool) {
    /* Check free list first */
    if (pool->free_head != 0) {
        uint32_t offset = pool->free_head;
        pool->free_head = pool->entries[offset].next_offset;
        memset(&pool->entries[offset], 0, sizeof(struct xattr_entry));
        return offset;
    }

    /* Allocate new entry */
    if (pool->used >= pool->capacity) {
        return 0;  /* No space */
    }

    uint32_t offset = pool->used++;
    memset(&pool->entries[offset], 0, sizeof(struct xattr_entry));
    return offset;
}

/* Free xattr entry */
static void free_entry(struct xattr_pool *pool, uint32_t offset) {
    if (offset == 0 || offset >= pool->used) return;

    /* Add to free list */
    pool->entries[offset].next_offset = pool->free_head;
    pool->free_head = offset;
}

/* Get xattr value */
int xattr_get(struct xattr_pool *pool,
              struct xattr_value_pool *values,
              struct string_table *names,
              uint32_t xattr_head,
              const char *name,
              void *value,
              size_t size) {
    if (!pool || !values || !names || !name) return -EINVAL;

    /* Validate name */
    uint8_t flags;
    int ret = xattr_validate_name(name, &flags);
    if (ret < 0) return ret;

    /* Acquire read lock */
    pthread_rwlock_rdlock(&pool->lock);

    /* Traverse linked list */
    uint32_t offset = xattr_head;
    while (offset != 0) {
        if (offset >= pool->used) {
            pthread_rwlock_unlock(&pool->lock);
            return -EINVAL;
        }

        struct xattr_entry *entry = &pool->entries[offset];

        /* Get name from string table */
        const char *entry_name = string_table_get(names, entry->name_offset);
        if (entry_name && strcmp(entry_name, name) == 0) {
            /* Found it */
            uint32_t value_len = entry->value_len;

            /* If size is 0, just return the size needed */
            if (size == 0) {
                pthread_rwlock_unlock(&pool->lock);
                return value_len;
            }

            /* Check if buffer is large enough */
            if (size < value_len) {
                pthread_rwlock_unlock(&pool->lock);
                return -ERANGE;
            }

            /* Copy value */
            pthread_rwlock_rdlock(&values->lock);
            if (value_len > 0 && entry->value_offset < values->capacity) {
                memcpy(value, values->buffer + entry->value_offset, value_len);
            }
            pthread_rwlock_unlock(&values->lock);
            pthread_rwlock_unlock(&pool->lock);

            return value_len;
        }

        offset = entry->next_offset;
    }

    pthread_rwlock_unlock(&pool->lock);
    return -ENODATA;  /* Not found */
}

/* Set xattr value */
int xattr_set(struct xattr_pool *pool,
              struct xattr_value_pool *values,
              struct string_table *names,
              uint32_t *xattr_head,
              uint16_t *xattr_count,
              const char *name,
              const void *value,
              size_t size,
              int flags) {
    if (!pool || !values || !names || !xattr_head || !name) {
        return -EINVAL;
    }

    /* Validate name */
    uint8_t ns_flags;
    int ret = xattr_validate_name(name, &ns_flags);
    if (ret < 0) return ret;

    /* Validate size */
    ret = xattr_validate_size(size);
    if (ret < 0) return ret;

    /* Acquire write locks */
    pthread_rwlock_wrlock(&pool->lock);
    pthread_rwlock_wrlock(&values->lock);

    /* Check if xattr already exists */
    uint32_t offset = *xattr_head;
    uint32_t prev_offset = 0;

    while (offset != 0) {
        if (offset >= pool->used) {
            pthread_rwlock_unlock(&values->lock);
            pthread_rwlock_unlock(&pool->lock);
            return -EINVAL;
        }

        struct xattr_entry *entry = &pool->entries[offset];
        const char *entry_name = string_table_get(names, entry->name_offset);

        if (entry_name && strcmp(entry_name, name) == 0) {
            /* Check flags */
            if (flags & XATTR_CREATE) {
                pthread_rwlock_unlock(&values->lock);
                pthread_rwlock_unlock(&pool->lock);
                return -EEXIST;
            }

            /* Update existing entry */
            /* Free old value */
            free_value(values, entry->value_offset, entry->value_len);

            /* Allocate new value */
            uint32_t value_offset = allocate_value(values, value, size);
            if (value_offset == 0 && size > 0) {
                pthread_rwlock_unlock(&values->lock);
                pthread_rwlock_unlock(&pool->lock);
                return -ENOSPC;
            }

            entry->value_offset = value_offset;
            entry->value_len = size;

            pthread_rwlock_unlock(&values->lock);
            pthread_rwlock_unlock(&pool->lock);
            return 0;
        }

        prev_offset = offset;
        offset = entry->next_offset;
    }

    /* Not found */
    if (flags & XATTR_REPLACE) {
        pthread_rwlock_unlock(&values->lock);
        pthread_rwlock_unlock(&pool->lock);
        return -ENODATA;
    }

    /* Create new entry */
    uint32_t name_offset = string_table_intern(names, name);
    if (name_offset == UINT32_MAX) {
        pthread_rwlock_unlock(&values->lock);
        pthread_rwlock_unlock(&pool->lock);
        return -ENOSPC;
    }

    uint32_t value_offset = 0;
    if (size > 0) {
        value_offset = allocate_value(values, value, size);
        if (value_offset == 0) {
            pthread_rwlock_unlock(&values->lock);
            pthread_rwlock_unlock(&pool->lock);
            return -ENOSPC;
        }
    }

    uint32_t new_offset = allocate_entry(pool);
    if (new_offset == 0) {
        free_value(values, value_offset, size);
        pthread_rwlock_unlock(&values->lock);
        pthread_rwlock_unlock(&pool->lock);
        return -ENOSPC;
    }

    struct xattr_entry *new_entry = &pool->entries[new_offset];
    new_entry->name_offset = name_offset;
    new_entry->value_offset = value_offset;
    new_entry->value_len = size;
    new_entry->flags = ns_flags;
    new_entry->next_offset = 0;

    /* Link into list */
    if (*xattr_head == 0) {
        *xattr_head = new_offset;
    } else {
        pool->entries[prev_offset].next_offset = new_offset;
    }

    if (xattr_count) {
        (*xattr_count)++;
    }

    pthread_rwlock_unlock(&values->lock);
    pthread_rwlock_unlock(&pool->lock);
    return 0;
}

/* List xattr names */
ssize_t xattr_list(struct xattr_pool *pool,
                   struct string_table *names,
                   uint32_t xattr_head,
                   char *list,
                   size_t size) {
    if (!pool || !names) return -EINVAL;

    pthread_rwlock_rdlock(&pool->lock);

    /* Calculate total size needed */
    size_t total = 0;
    uint32_t offset = xattr_head;

    while (offset != 0) {
        if (offset >= pool->used) {
            pthread_rwlock_unlock(&pool->lock);
            return -EINVAL;
        }

        struct xattr_entry *entry = &pool->entries[offset];
        const char *entry_name = string_table_get(names, entry->name_offset);

        if (entry_name) {
            size_t name_len = strlen(entry_name) + 1;  /* Include null */
            total += name_len;
        }

        offset = entry->next_offset;
    }

    /* If size is 0, just return total size */
    if (size == 0) {
        pthread_rwlock_unlock(&pool->lock);
        return total;
    }

    /* Fill buffer */
    size_t written = 0;
    offset = xattr_head;

    while (offset != 0 && written < size) {
        if (offset >= pool->used) {
            pthread_rwlock_unlock(&pool->lock);
            return -EINVAL;
        }

        struct xattr_entry *entry = &pool->entries[offset];
        const char *entry_name = string_table_get(names, entry->name_offset);

        if (entry_name) {
            size_t name_len = strlen(entry_name) + 1;
            if (written + name_len <= size) {
                memcpy(list + written, entry_name, name_len);
                written += name_len;
            }
        }

        offset = entry->next_offset;
    }

    pthread_rwlock_unlock(&pool->lock);
    return total;
}

/* Remove xattr */
int xattr_remove(struct xattr_pool *pool,
                 struct xattr_value_pool *values,
                 struct string_table *names,
                 uint32_t *xattr_head,
                 uint16_t *xattr_count,
                 const char *name) {
    if (!pool || !values || !names || !xattr_head || !name) {
        return -EINVAL;
    }

    /* Validate name */
    uint8_t flags;
    int ret = xattr_validate_name(name, &flags);
    if (ret < 0) return ret;

    pthread_rwlock_wrlock(&pool->lock);
    pthread_rwlock_wrlock(&values->lock);

    /* Find entry */
    uint32_t offset = *xattr_head;
    uint32_t prev_offset = 0;

    while (offset != 0) {
        if (offset >= pool->used) {
            pthread_rwlock_unlock(&values->lock);
            pthread_rwlock_unlock(&pool->lock);
            return -EINVAL;
        }

        struct xattr_entry *entry = &pool->entries[offset];
        const char *entry_name = string_table_get(names, entry->name_offset);

        if (entry_name && strcmp(entry_name, name) == 0) {
            /* Found it - unlink from list */
            if (prev_offset == 0) {
                *xattr_head = entry->next_offset;
            } else {
                pool->entries[prev_offset].next_offset = entry->next_offset;
            }

            /* Free value */
            free_value(values, entry->value_offset, entry->value_len);

            /* Free entry */
            free_entry(pool, offset);

            if (xattr_count) {
                (*xattr_count)--;
            }

            pthread_rwlock_unlock(&values->lock);
            pthread_rwlock_unlock(&pool->lock);
            return 0;
        }

        prev_offset = offset;
        offset = entry->next_offset;
    }

    pthread_rwlock_unlock(&values->lock);
    pthread_rwlock_unlock(&pool->lock);
    return -ENODATA;
}

/* Free all xattrs for an inode */
void xattr_free_all(struct xattr_pool *pool,
                    struct xattr_value_pool *values,
                    uint32_t xattr_head,
                    uint16_t xattr_count) {
    if (!pool || !values || xattr_head == 0) return;

    pthread_rwlock_wrlock(&pool->lock);
    pthread_rwlock_wrlock(&values->lock);

    uint32_t offset = xattr_head;
    uint32_t count = 0;

    while (offset != 0 && count < xattr_count) {
        if (offset >= pool->used) break;

        struct xattr_entry *entry = &pool->entries[offset];
        uint32_t next = entry->next_offset;

        /* Free value */
        free_value(values, entry->value_offset, entry->value_len);

        /* Free entry */
        free_entry(pool, offset);

        offset = next;
        count++;
    }

    pthread_rwlock_unlock(&values->lock);
    pthread_rwlock_unlock(&pool->lock);
}
