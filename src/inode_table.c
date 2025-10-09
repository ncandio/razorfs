/**
 * Inode Table Implementation
 */

#include "inode_table.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>

/* Find next prime for hash table size */
static uint32_t next_prime(uint32_t n) {
    if (n <= 2) return 2;
    if (n % 2 == 0) n++;
    while (1) {
        int is_prime = 1;
        for (uint32_t i = 3; i * i <= n; i += 2) {
            if (n % i == 0) {
                is_prime = 0;
                break;
            }
        }
        if (is_prime) return n;
        n += 2;
    }
}

int inode_table_init(struct inode_table *table, uint32_t capacity) __attribute__((unused));
int inode_table_init(struct inode_table *table, uint32_t capacity) {
    if (!table || capacity == 0) return -1;

    memset(table, 0, sizeof(*table));

    table->inodes = calloc(capacity, sizeof(struct razorfs_inode));
    if (!table->inodes) {
        return -1;
    }

    table->capacity = capacity;
    table->used = 1;  /* Inode 0 is reserved, 1 is root */
    table->next_inode = 2;
    table->free_head = 0; /* 0 indicates no free list */

    if (pthread_rwlock_init(&table->lock, NULL) != 0) {
        free(table->inodes);
        return -1;
    }

    /* Initialize hash table with prime size for better distribution */
    table->hash_capacity = next_prime(capacity);
    table->hash_table = calloc(table->hash_capacity, sizeof(struct inode_hash_entry *));
    if (!table->hash_table) {
        pthread_rwlock_destroy(&table->lock);
        free(table->inodes);
        return -1;
    }

    return 0;
}

/* Hash function for inode numbers */
static uint32_t hash_inode(uint32_t inode_num, uint32_t capacity) {
    /* Simple multiplicative hash */
    return (inode_num * 2654435761U) % capacity;
}


/* Destroy inode table */
void inode_table_destroy(struct inode_table *table) __attribute__((unused));
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
uint32_t inode_alloc(struct inode_table *table, mode_t mode) __attribute__((unused));
uint32_t inode_alloc(struct inode_table *table, mode_t mode) {
    if (!table) return 0;

    pthread_rwlock_wrlock(&table->lock);

    uint32_t index;

    /* Try to reuse from free list first */
    if (table->free_head != 0) {
        index = table->free_head;
        /* The 'next' pointer is stored in the otherwise unused 'xattr_head' */
        table->free_head = table->inodes[index].xattr_head;
    } else {
        /* If free list is empty, allocate from the end */
        if (table->used >= table->capacity) {
            pthread_rwlock_unlock(&table->lock);
            return 0; /* Table is full */
        }
        index = table->used++;
    }

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
int inode_link(struct inode_table *table, uint32_t inode_num) __attribute__((unused));
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
int inode_unlink(struct inode_table *table, uint32_t inode_num) __attribute__((unused));
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
        uint32_t index = hash_lookup(table, inode_num);

        /* Remove from hash table */
        hash_remove(table, inode_num);

        /* Add to free list for reuse */
        memset(inode, 0, sizeof(*inode));
        inode->xattr_head = table->free_head; /* Reuse xattr_head as next pointer */
        table->free_head = index;
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
