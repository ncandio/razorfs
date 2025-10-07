/**
 * Inode Table Implementation
 */

#include "inode_table.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>

/* Hash function for inode numbers */
static uint32_t hash_inode(uint32_t inode_num, uint32_t capacity) {
    /* Simple multiplicative hash */
    return (inode_num * 2654435761U) % capacity;
}


/* Destroy inode table */
void inode_table_destroy(struct inode_table *table) {
    if (!table) return;

    pthread_rwlock_destroy(&table->lock);

    /* Free hash table chains */
    if (table->hash_table) {
        for (uint32_t i = 0; i < table->hash_capacity; i++) {
            struct inode_hash_entry *entry = table->hash_table[i];
            while (entry) {
                struct inode_hash_entry *next = entry->next;
                free(entry);
                entry = next;
            }
        }
        free(table->hash_table);
    }

    if (table->inodes) {
        free(table->inodes);
    }

    memset(table, 0, sizeof(*table));
}

/* Add inode to hash table */
static int hash_insert(struct inode_table *table, uint32_t inode_num, uint32_t index) {
    uint32_t hash = hash_inode(inode_num, table->hash_capacity);

    struct inode_hash_entry *entry = malloc(sizeof(struct inode_hash_entry));
    if (!entry) return -1;

    entry->inode_num = inode_num;
    entry->index = index;
    entry->next = table->hash_table[hash];
    table->hash_table[hash] = entry;

    return 0;
}

/* Remove inode from hash table */
static void hash_remove(struct inode_table *table, uint32_t inode_num) {
    uint32_t hash = hash_inode(inode_num, table->hash_capacity);

    struct inode_hash_entry **prev = &table->hash_table[hash];
    struct inode_hash_entry *entry = table->hash_table[hash];

    while (entry) {
        if (entry->inode_num == inode_num) {
            *prev = entry->next;
            free(entry);
            return;
        }
        prev = &entry->next;
        entry = entry->next;
    }
}

/* Lookup inode index by number */
static uint32_t hash_lookup(struct inode_table *table, uint32_t inode_num) {
    uint32_t hash = hash_inode(inode_num, table->hash_capacity);
    struct inode_hash_entry *entry = table->hash_table[hash];

    while (entry) {
        if (entry->inode_num == inode_num) {
            return entry->index;
        }
        entry = entry->next;
    }

    return UINT32_MAX;  /* Not found */
}

/* Allocate a new inode */
uint32_t inode_alloc(struct inode_table *table, mode_t mode) {
    if (!table) return 0;

    pthread_rwlock_wrlock(&table->lock);

    /* Check if table is full */
    if (table->used >= table->capacity) {
        pthread_rwlock_unlock(&table->lock);
        return 0;
    }

    /* Find free slot */
    uint32_t index = table->used++;
    uint32_t inode_num = table->next_inode++;

    /* Initialize inode */
    struct razorfs_inode *inode = &table->inodes[index];
    memset(inode, 0, sizeof(*inode));

    inode->inode_num = inode_num;
    inode->nlink = 1;  /* Start with 1 link */
    inode->mode = mode;

    /* Set timestamps */
    uint32_t now = (uint32_t)time(NULL);
    inode->atime = now;
    inode->mtime = now;
    inode->ctime = now;

    inode->size = 0;
    inode->xattr_head = 0;

    /* Add to hash table */
    if (hash_insert(table, inode_num, index) != 0) {
        table->used--;
        table->next_inode--;
        pthread_rwlock_unlock(&table->lock);
        return 0;
    }

    pthread_rwlock_unlock(&table->lock);
    return inode_num;
}

/* Lookup inode by number */
struct razorfs_inode* inode_lookup(struct inode_table *table, uint32_t inode_num) {
    if (!table || inode_num == 0) return NULL;

    /* Caller should hold at least read lock */
    uint32_t index = hash_lookup(table, inode_num);
    if (index == UINT32_MAX) {
        return NULL;
    }

    if (index >= table->used) {
        return NULL;
    }

    return &table->inodes[index];
}

/* Increment link count */
int inode_link(struct inode_table *table, uint32_t inode_num) {
    if (!table || inode_num == 0) return -EINVAL;

    pthread_rwlock_wrlock(&table->lock);

    struct razorfs_inode *inode = inode_lookup(table, inode_num);
    if (!inode) {
        pthread_rwlock_unlock(&table->lock);
        return -ENOENT;
    }

    /* Check for overflow */
    if (inode->nlink >= INODE_MAX_LINKS) {
        pthread_rwlock_unlock(&table->lock);
        return -EMLINK;
    }

    inode->nlink++;
    inode->ctime = (uint32_t)time(NULL);

    pthread_rwlock_unlock(&table->lock);
    return 0;
}

/* Decrement link count */
int inode_unlink(struct inode_table *table, uint32_t inode_num) {
    if (!table || inode_num == 0) return -EINVAL;

    pthread_rwlock_wrlock(&table->lock);

    struct razorfs_inode *inode = inode_lookup(table, inode_num);
    if (!inode) {
        pthread_rwlock_unlock(&table->lock);
        return -ENOENT;
    }

    /* Decrement link count */
    if (inode->nlink > 0) {
        inode->nlink--;
        inode->ctime = (uint32_t)time(NULL);
    }

    /* If no more links, free the inode */
    if (inode->nlink == 0) {
        /* Remove from hash table */
        hash_remove(table, inode_num);

        /* Mark as free (simple approach: just zero it) */
        /* TODO: Add to free list for reuse */
        memset(inode, 0, sizeof(*inode));
    }

    pthread_rwlock_unlock(&table->lock);
    return 0;
}

/* Update inode metadata */
int inode_update(struct inode_table *table, uint32_t inode_num,
                 uint64_t size, uint32_t mtime) {
    if (!table || inode_num == 0) return -EINVAL;

    pthread_rwlock_wrlock(&table->lock);

    struct razorfs_inode *inode = inode_lookup(table, inode_num);
    if (!inode) {
        pthread_rwlock_unlock(&table->lock);
        return -ENOENT;
    }

    inode->size = size;
    inode->mtime = mtime;
    inode->ctime = (uint32_t)time(NULL);

    pthread_rwlock_unlock(&table->lock);
    return 0;
}

/* Get inode table statistics */
void inode_table_stats(struct inode_table *table,
                       uint32_t *total_out,
                       uint32_t *used_out,
                       uint32_t *free_out) {
    if (!table) return;

    pthread_rwlock_rdlock(&table->lock);

    if (total_out) *total_out = table->capacity;
    if (used_out) *used_out = table->used;
    if (free_out) *free_out = table->capacity - table->used;

    pthread_rwlock_unlock(&table->lock);
}
