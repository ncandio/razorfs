/**
 * Crash Recovery Implementation
 */

#include "recovery.h"
#include "string_table.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

/* Helper to find or create transaction in table */
static struct tx_info* find_or_create_tx(struct recovery_ctx *ctx, uint64_t tx_id) {
    /* Search for existing */
    for (uint32_t i = 0; i < ctx->tx_count; i++) {
        if (ctx->tx_table[i].tx_id == tx_id) {
            return &ctx->tx_table[i];
        }
    }

    /* Create new */
    if (ctx->tx_count >= ctx->tx_capacity) {
        /* Grow table */
        uint32_t new_cap = ctx->tx_capacity * 2;
        struct tx_info *new_table = realloc(ctx->tx_table,
                                            new_cap * sizeof(struct tx_info));
        if (!new_table) return NULL;

        ctx->tx_table = new_table;
        ctx->tx_capacity = new_cap;
    }

    struct tx_info *tx = &ctx->tx_table[ctx->tx_count++];
    memset(tx, 0, sizeof(*tx));
    tx->tx_id = tx_id;
    tx->state = TX_ACTIVE;
    tx->first_lsn = 0;
    tx->last_lsn = 0;
    tx->op_count = 0;

    return tx;
}

/* Initialize recovery context */
int recovery_init(struct recovery_ctx *ctx, struct wal *wal,
                  struct nary_tree_mt *tree, struct string_table *strings) {
    if (!ctx || !wal || !tree) return -1;

    memset(ctx, 0, sizeof(*ctx));
    ctx->wal = wal;
    ctx->tree = tree;
    ctx->strings = strings;

    /* Allocate initial transaction table */
    ctx->tx_capacity = 32;
    ctx->tx_table = malloc(ctx->tx_capacity * sizeof(struct tx_info));
    if (!ctx->tx_table) {
        return -1;
    }
    ctx->tx_count = 0;

    ctx->verbose = 1;  // Can be enabled for debugging

    return 0;
}

/* Cleanup recovery context */
void recovery_destroy(struct recovery_ctx *ctx) {
    if (!ctx) return;

    if (ctx->tx_table) {
        free(ctx->tx_table);
        ctx->tx_table = NULL;
    }

    memset(ctx, 0, sizeof(*ctx));
}

/* Check if WAL needs recovery */
int wal_needs_recovery(const struct wal *wal) {
    if (!wal || !wal->header) return 0;

    /* No entries = clean */
    if (wal->header->entry_count == 0) {
        return 0;
    }

    /* Only checkpoint entry = clean shutdown */
    if (wal->header->entry_count == 1) {
        /* Read the entry to check if it's a checkpoint */
        const struct wal_entry *entry = (const struct wal_entry *)(wal->log_buffer + wal->header->tail_offset);
        if (entry->op_type == WAL_OP_CHECKPOINT) {
            return 0;  // Clean shutdown
        }
    }

    /* Has entries (not just checkpoint) = needs recovery */
    return 1;
}

/* Read entry at given offset in WAL */
static struct wal_entry* read_entry_at(const struct wal *wal, uint64_t offset,
                                       void **data_out) {
    if (offset >= wal->buffer_size) {
        return NULL;
    }

    // Ensure that at least the wal_entry structure can be read without going out of bounds.
    if (offset + sizeof(struct wal_entry) > wal->buffer_size) {
        return NULL;
    }

    struct wal_entry *entry = (struct wal_entry *)(wal->log_buffer + offset);

    // Validate data_len to prevent reading beyond the buffer if it's corrupted.
    // This check must happen before using entry->data_len in size calculations or memcpy.
    if (entry->data_len > wal->buffer_size || // data_len itself is too large
        offset + sizeof(struct wal_entry) + entry->data_len > wal->buffer_size) {
        return NULL; // Corrupted data_len or entry extends beyond buffer
    }

    /* Validate checksum */
    struct wal_entry temp_entry;
    memcpy(&temp_entry, entry, sizeof(struct wal_entry));
    temp_entry.checksum = 0; // Zero out checksum in the copy, not the original

    uint32_t header_checksum = wal_crc32(&temp_entry, sizeof(struct wal_entry));
    uint32_t expected_checksum;

    if (entry->data_len > 0) {
        void *data_ptr = wal->log_buffer + offset + sizeof(struct wal_entry);
        uint32_t data_checksum = wal_crc32(data_ptr, entry->data_len);
        expected_checksum = wal_crc32_combine(header_checksum, data_checksum, entry->data_len);
    } else {
        expected_checksum = header_checksum;
    }

    if (entry->checksum != expected_checksum) {
        return NULL;  /* Corrupted */
    }

    /* Extract data if present */
    if (data_out) {
        *data_out = NULL;  // Initialize to NULL
        if (entry->data_len > 0) {
            *data_out = malloc(entry->data_len);
            if (*data_out) {
                memcpy(*data_out, wal->log_buffer + offset + sizeof(struct wal_entry),
                       entry->data_len);
            }
        }
    }

    return entry;
}

/* Analysis phase: scan WAL and build transaction table */
int recovery_analysis(struct recovery_ctx *ctx) {
    if (!ctx) return -1;

    if (ctx->verbose) {
        printf("[RECOVERY] Starting analysis phase...\n");
    }



    if (ctx->verbose) {
        printf("[RECOVERY] Starting analysis phase...\n");
    }

    /* Scan from tail to head */
    uint64_t offset = ctx->wal->header->tail_offset;
    uint64_t head = ctx->wal->header->head_offset;

    while (offset != head) {
        const struct wal_entry *entry = read_entry_at(ctx->wal, offset, NULL);
        if (!entry) {
            /* Corrupted entry - stop here */
            break;
        }

        ctx->entries_scanned++;

        /* Find or create transaction */
        struct tx_info *tx = find_or_create_tx(ctx, entry->tx_id);
        if (!tx) {
            return -1;  // Out of memory
        }

        /* Update transaction state based on operation */
        switch (entry->op_type) {
            case WAL_OP_BEGIN:
                tx->state = TX_ACTIVE;
                tx->first_lsn = entry->lsn;
                break;

            case WAL_OP_COMMIT:
                tx->state = TX_COMMITTED;
                tx->last_lsn = entry->lsn;
                break;

            case WAL_OP_ABORT:
                tx->state = TX_ABORTED;
                tx->last_lsn = entry->lsn;
                break;

            case WAL_OP_INSERT:
            case WAL_OP_DELETE:
            case WAL_OP_UPDATE:
            case WAL_OP_WRITE:
                tx->op_count++;
                tx->last_lsn = entry->lsn;
                break;

            case WAL_OP_CHECKPOINT:
                /* Stop at checkpoint */
                goto done;

            default:
                break;
        }

        /* Move to next entry */
        offset += sizeof(struct wal_entry) + entry->data_len;
        if (offset >= ctx->wal->buffer_size) {
            offset = 0;  // Wraparound
        }
    }

done:
    if (ctx->verbose) {
        printf("[RECOVERY] Analysis complete: %u transactions, %u entries\n",
               ctx->tx_count, ctx->entries_scanned);
    }

    return 0;
}

/* Check if insert was already applied (idempotency) */
static int check_insert_applied(const struct recovery_ctx *ctx,
                               const struct wal_insert_data *data) {
    /* Look up by inode */
    for (uint32_t i = 0; i < ctx->tree->used; i++) {
        if (ctx->tree->nodes[i].node.inode == data->inode) {
            return 1;  // Already exists
        }
    }
    return 0;
}

/* Replay insert operation */
static int replay_insert(struct recovery_ctx *ctx, const struct wal_insert_data *data) {
    /* Check idempotency */
    if (check_insert_applied(ctx, data)) {
        ctx->ops_skipped++;
        return 1;  // Already applied
    }

    /* Get name from string table */
    const char *name = string_table_get(ctx->strings, data->name_offset);
    if (!name) {
        return -1;
    }

    /* Insert node */
    uint16_t idx = nary_insert_mt(ctx->tree, data->parent_idx, name, data->mode);
    if (idx == NARY_INVALID_IDX) {
        return -1;
    }

    /* Set inode and timestamp */
    ctx->tree->nodes[idx].node.inode = data->inode;
    ctx->tree->nodes[idx].node.mtime = data->timestamp;

    ctx->ops_redone++;
    return 0;
}

/* Check if delete was already applied */
static int check_delete_applied(const struct recovery_ctx *ctx,
                               const struct wal_delete_data *data) {
    if (data->node_idx >= ctx->tree->used) {
        return 1;  // Node doesn't exist
    }

    if (ctx->tree->nodes[data->node_idx].node.inode == 0) {
        return 1;  // Already deleted
    }

    return 0;
}

/* Replay delete operation */
static int replay_delete(struct recovery_ctx *ctx, const struct wal_delete_data *data) {
    /* Check idempotency */
    if (check_delete_applied(ctx, data)) {
        ctx->ops_skipped++;
        return 1;
    }

    /* Delete node by index */
    int ret = nary_delete_mt(ctx->tree, data->node_idx, ctx->wal, 0);
    if (ret != 0) {
        return -1;
    }

    ctx->ops_redone++;
    return 0;
}

/* Check if update was already applied */
static int check_update_applied(const struct recovery_ctx *ctx,
                               const struct wal_update_data *data) {
    if (data->node_idx >= ctx->tree->used) {
        return -1;  // Invalid node
    }

    /* Check by timestamp */
    if (ctx->tree->nodes[data->node_idx].node.mtime >= data->new_mtime) {
        return 1;  // Already updated
    }

    return 0;
}

/* Replay update operation */
static int replay_update(struct recovery_ctx *ctx, const struct wal_update_data *data) {
    /* Check idempotency */
    int applied = check_update_applied(ctx, data);
    if (applied == 1) {
        ctx->ops_skipped++;
        return 1;
    }
    if (applied < 0) {
        return -1;
    }

    /* Apply update */
    struct nary_node *node = &ctx->tree->nodes[data->node_idx].node;
    node->size = data->new_size;
    node->mtime = data->new_mtime;
    node->mode = data->mode;

    ctx->ops_redone++;
    return 0;
}

/* Replay write operation */
static int replay_write(struct recovery_ctx *ctx, const struct wal_entry *entry, const struct wal_write_data *data) {
    if (data->node_idx >= ctx->tree->used) {
        return -1;  /* Invalid node */
    }

    /* Apply update */
    struct nary_node *node = &ctx->tree->nodes[data->node_idx].node;
    node->size = data->new_size;
    node->mtime = entry->timestamp / 1000000;

    ctx->ops_redone++;
    return 0;
}

/* Replay a single operation */
static int replay_operation(struct recovery_ctx *ctx, const struct wal_entry *entry,
                           void *data) {
    if (!data) return -1;

    switch (entry->op_type) {
        case WAL_OP_INSERT:
            if (entry->data_len < sizeof(struct wal_insert_data)) return -1; // Corrupted data_len
            return replay_insert(ctx, (struct wal_insert_data *)data);

        case WAL_OP_DELETE:
            if (entry->data_len < sizeof(struct wal_delete_data)) return -1; // Corrupted data_len
            return replay_delete(ctx, (struct wal_delete_data *)data);

        case WAL_OP_UPDATE:
            if (entry->data_len < sizeof(struct wal_update_data)) return -1; // Corrupted data_len
            return replay_update(ctx, (struct wal_update_data *)data);

        case WAL_OP_WRITE:
            if (entry->data_len < sizeof(struct wal_write_data)) return -1; // Corrupted data_len
            return replay_write(ctx, entry, (struct wal_write_data *)data);

        default:
            return 0;
    }
}

/* Redo phase: replay committed transactions */
int recovery_redo(struct recovery_ctx *ctx) {
    if (!ctx) return -1;

    if (ctx->verbose) {
        printf("[RECOVERY] Starting redo phase...\n");
    }

    /* Scan WAL again, replay operations from committed transactions */
    uint64_t offset = ctx->wal->header->tail_offset;
    uint64_t head = ctx->wal->header->head_offset;

    while (offset != head) {
        void *data = NULL;
        const struct wal_entry *entry = read_entry_at(ctx->wal, offset, &data);
        if (!entry) break;

        /* Find transaction */
        const struct tx_info *tx = NULL;
        for (uint32_t i = 0; i < ctx->tx_count; i++) {
            if (ctx->tx_table[i].tx_id == entry->tx_id) {
                tx = &ctx->tx_table[i];
                break;
            }
        }

        /* Only replay if transaction was committed or not part of a transaction */
        if (!tx || tx->state == TX_COMMITTED) {
            if (entry->op_type >= WAL_OP_INSERT && entry->op_type <= WAL_OP_WRITE) {
                replay_operation(ctx, entry, data);
            }
        }

        if (data) free(data);

        /* Move to next entry */
        offset += sizeof(struct wal_entry) + entry->data_len;
        if (offset >= ctx->wal->buffer_size) {
            offset = 0;
        }
    }

    if (ctx->verbose) {
        printf("[RECOVERY] Redo complete: %u ops redone, %u skipped\n",
               ctx->ops_redone, ctx->ops_skipped);
    }

    return 0;
}

static int undo_insert(struct recovery_ctx *ctx, const struct wal_insert_data *data) {
    /* Find the node by inode - it should exist if insert was applied */
    for (uint32_t i = 0; i < ctx->tree->used; i++) {
        if (ctx->tree->nodes[i].node.inode == 0) {
            continue; // Skip logically deleted nodes
        }
        if (ctx->tree->nodes[i].node.inode == data->inode) {
            if (ctx->verbose) {
                printf("[RECOVERY] undo_insert: Found node with inode %u at index %u. Attempting delete.\n", data->inode, i);
            }
            /* Found it, now delete it */
            int delete_ret = nary_delete_mt(ctx->tree, i, ctx->wal, 0);
            if (delete_ret == 0) {
                ctx->ops_undone++;
                if (ctx->verbose) {
                    printf("[RECOVERY] undo_insert: nary_delete_mt successful. ops_undone: %u\n", ctx->ops_undone);
                }
                return 0;
            }
            if (ctx->verbose) {
                printf("[RECOVERY] undo_insert: nary_delete_mt failed with code %d.\n", delete_ret);
            }
            return -1; /* Failed to delete */
        }
    }
    if (ctx->verbose) {
        printf("[RECOVERY] undo_insert: Node with inode %u not found. Skipping undo.\n", data->inode);
    }
    return 0; /* Node not found, insert was not applied, so undo is a no-op */
}

/* Undo a single delete operation */
static int undo_delete(struct recovery_ctx *ctx, const struct wal_delete_data *data) {
    /* Re-insert the deleted node */
    const char *name = string_table_get(ctx->strings, data->name_offset);
    if (!name) {
        return -1;
    }

    uint16_t new_idx = nary_insert_mt(ctx->tree, data->parent_idx, name, data->mode);
    if (new_idx == NARY_INVALID_IDX) {
        return -1; /* Failed to re-insert */
    }

    /* Restore its attributes */
    struct nary_node *node = &ctx->tree->nodes[new_idx].node;
    node->inode = data->inode;
    node->mtime = data->timestamp;

    ctx->ops_undone++;
    return 0;
}

/* Undo a single update operation */
static int undo_update(struct recovery_ctx *ctx, const struct wal_update_data *data) {
    if (data->node_idx >= ctx->tree->used) {
        return 0; /* Node doesn't exist, update was not applied */
    }

    /* Restore old attributes */
    struct nary_node *node = &ctx->tree->nodes[data->node_idx].node;
    node->size = data->old_size;
    node->mtime = data->old_mtime;
    /* Mode is not changed in update, so no need to restore */

    ctx->ops_undone++;
    return 0;
}

/* Undo a single write operation */
static int undo_write(struct recovery_ctx *ctx, const struct wal_write_data *data) {
    if (data->node_idx >= ctx->tree->used) {
        return 0; /* Node doesn't exist, so write was not applied */
    }

    /* Restore old file size. This is critical to prevent corruption
     * where a file has a larger size than its actual content post-recovery. */
    struct nary_node *node = &ctx->tree->nodes[data->node_idx].node;
    node->size = data->old_size;

    ctx->ops_undone++;
    return 0;
}

/* Undo a single operation */
static int undo_operation(struct recovery_ctx *ctx, const struct wal_entry *entry, void *data) {
    if (!data) return -1;
    if (ctx->verbose) {
        printf("[RECOVERY] Undoing op_type: %u, tx_id: %lu, lsn: %lu\n",
               entry->op_type, entry->tx_id, entry->lsn);
    }

    switch (entry->op_type) {
        case WAL_OP_INSERT:
            if (entry->data_len < sizeof(struct wal_insert_data)) return -1; // Corrupted data_len
            return undo_insert(ctx, (struct wal_insert_data *)data);
        case WAL_OP_DELETE:
            if (entry->data_len < sizeof(struct wal_delete_data)) return -1; // Corrupted data_len
            return undo_delete(ctx, (struct wal_delete_data *)data);
        case WAL_OP_UPDATE:
            if (entry->data_len < sizeof(struct wal_update_data)) return -1; // Corrupted data_len
            return undo_update(ctx, (struct wal_update_data *)data);
        case WAL_OP_WRITE:
            if (entry->data_len < sizeof(struct wal_write_data)) return -1; // Corrupted data_len
            return undo_write(ctx, (struct wal_write_data *)data);
        default:
            return 0;
    }
}

/* Read entry at previous offset in WAL (for backward scan) */
/* Structure to cache entry offsets for efficient backward traversal */
struct offset_cache {
    uint64_t *offsets;
    uint32_t count;
    uint32_t capacity;
};

static struct offset_cache* build_offset_cache(const struct wal *wal) {
    struct offset_cache *cache = calloc(1, sizeof(struct offset_cache));
    if (!cache) return NULL;

    cache->capacity = 1024;  /* Initial capacity */
    cache->offsets = malloc(cache->capacity * sizeof(uint64_t));
    if (!cache->offsets) {
        free(cache);
        return NULL;
    }

    /* Scan forward from tail to head, recording all entry offsets */
    uint64_t offset = wal->header->tail_offset;
    uint64_t head = wal->header->head_offset;

    while (offset != head) {
        /* Grow cache if needed */
        if (cache->count >= cache->capacity) {
            uint32_t new_capacity = cache->capacity * 2;
            uint64_t *new_offsets = realloc(cache->offsets, new_capacity * sizeof(uint64_t));
            if (!new_offsets) {
                free(cache->offsets);
                free(cache);
                return NULL;
            }
            cache->offsets = new_offsets;
            cache->capacity = new_capacity;
        }

        cache->offsets[cache->count++] = offset;

        /* Read entry to get size */
        struct wal_entry *entry = (struct wal_entry *)(wal->log_buffer + offset);
        if (entry->data_len > wal->buffer_size) {
            /* Corruption detected */
            free(cache->offsets);
            free(cache);
            return NULL;
        }

        /* Advance to next entry */
        uint64_t entry_size = sizeof(struct wal_entry) + entry->data_len;
        offset += entry_size;
        if (offset >= wal->buffer_size) {
            offset = 0;  /* Wrap around circular buffer */
        }
    }

    return cache;
}

static void free_offset_cache(struct offset_cache *cache) {
    if (cache) {
        free(cache->offsets);
        free(cache);
    }
}

static struct wal_entry* read_prev_entry_at(const struct wal *wal, uint64_t *offset,
                                            void **data_out, struct offset_cache *cache) {
    if (!cache || cache->count == 0) {
        return NULL;
    }

    /* Find current offset in cache */
    int32_t current_idx = -1;
    for (uint32_t i = 0; i < cache->count; i++) {
        if (cache->offsets[i] == *offset) {
            current_idx = (int32_t)i;
            break;
        }
    }

    if (current_idx <= 0) {
        return NULL;  /* At beginning or not found */
    }

    /* Get previous offset */
    *offset = cache->offsets[current_idx - 1];
    return read_entry_at(wal, *offset, data_out);
}


/* Undo phase: roll back uncommitted transactions */
int recovery_undo(struct recovery_ctx *ctx) {
    if (!ctx) return -1;

    if (ctx->verbose) {
        printf("[RECOVERY] Starting undo phase...\n");
    }

    /* Create a set of active transaction IDs for quick lookup */
    uint64_t active_tx_ids[ctx->tx_count];
    uint32_t active_tx_count = 0;
    for (uint32_t i = 0; i < ctx->tx_count; i++) {
        if (ctx->tx_table[i].state == TX_ACTIVE) {
            active_tx_ids[active_tx_count++] = ctx->tx_table[i].tx_id;
        }
    }

    if (active_tx_count == 0) {
        if (ctx->verbose) {
            printf("[RECOVERY] Undo complete: No active transactions to roll back.\n");
        }
        return 0; /* Nothing to do */
    }

    /* Build offset cache for efficient backward traversal (O(n) instead of O(n^2)) */
    struct offset_cache *cache = build_offset_cache(ctx->wal);
    if (!cache) {
        fprintf(stderr, "[RECOVERY] Failed to build offset cache for undo phase\n");
        return -1;
    }

    if (ctx->verbose) {
        printf("[RECOVERY] Built offset cache with %u entries for backward scan\n", cache->count);
    }

    /* Scan WAL backwards from head to tail using cached offsets */
    uint64_t offset = ctx->wal->header->head_offset;
    while (offset != ctx->wal->header->tail_offset) {
        if (ctx->verbose) {
            printf("[RECOVERY] Undo loop: current offset = %lu\n", offset);
        }
        void *data = NULL;
        struct wal_entry *entry = read_prev_entry_at(ctx->wal, &offset, &data, cache);
        if (!entry) break;

        /* Check if this entry belongs to an active transaction */
        int is_active = 0;
        for (uint32_t i = 0; i < active_tx_count; i++) {
            if (entry->tx_id == active_tx_ids[i]) {
                is_active = 1;
                break;
            }
        }

        if (is_active) {
            /* This operation needs to be undone */
            undo_operation(ctx, entry, data);
        }

        if (data) free(data);
    }

    /* Free the offset cache */
    free_offset_cache(cache);

    if (ctx->verbose) {
        printf("[RECOVERY] Undo complete: %u operations rolled back\n", ctx->ops_undone);
    }

    return 0;
}

/* Run complete recovery */
int recovery_run(struct recovery_ctx *ctx) {
    if (!ctx) return -1;

    if (ctx->verbose) {
        printf("[RECOVERY] Starting recovery_run...\n");
    }

    /* Check if recovery needed */
    if (!wal_needs_recovery(ctx->wal)) {
        if (ctx->verbose) {
            printf("[RECOVERY] Clean shutdown detected, skipping recovery\n");
        }
        return 0;
    }

    uint64_t start = wal_timestamp();

    /* Run three phases */
    int ret = recovery_analysis(ctx);
    if (ret != 0) {
        fprintf(stderr, "[RECOVERY] Analysis phase failed\n");
        return ret;
    }

    ret = recovery_redo(ctx);
    if (ret != 0) {
        fprintf(stderr, "[RECOVERY] Redo phase failed\n");
        return ret;
    }

    ret = recovery_undo(ctx);
    if (ret != 0) {
        fprintf(stderr, "[RECOVERY] Undo phase failed\n");
        return ret;
    }

    uint64_t end = wal_timestamp();
    ctx->recovery_time_us = end - start;

    return 0;
}

/* Print recovery statistics */
void recovery_print_stats(const struct recovery_ctx *ctx) __attribute__((unused));
void recovery_print_stats(const struct recovery_ctx *ctx) {
    if (!ctx) return;

    printf("\n=== Recovery Statistics ===\n");
    printf("Entries scanned:    %u\n", ctx->entries_scanned);
    printf("Transactions found: %u\n", ctx->tx_count);
    printf("Operations redone:  %u\n", ctx->ops_redone);
    printf("Operations undone:  %u\n", ctx->ops_undone);
    printf("Operations skipped: %u\n", ctx->ops_skipped);
    printf("Recovery time:      %lu μs (%.2f ms)\n",
           ctx->recovery_time_us, ctx->recovery_time_us / 1000.0);
    printf("===========================\n\n");
}
