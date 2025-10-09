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

    ctx->verbose = 0;  // Can be enabled for debugging

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

    struct wal_entry *entry = (struct wal_entry *)(wal->log_buffer + offset);

    /* Validate checksum (make a copy to avoid modifying the WAL) */
    struct wal_entry entry_copy = *entry;
    entry_copy.checksum = 0;  // Zero out for CRC calculation

    uint32_t expected = wal_crc32(&entry_copy, sizeof(struct wal_entry));
    if (entry->data_len > 0) {
        expected ^= wal_crc32(wal->log_buffer + offset + sizeof(struct wal_entry),
                             entry->data_len);
    }

    if (entry->checksum != expected) {
        return NULL;  // Corrupted
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
    int ret = nary_delete_mt(ctx->tree, data->node_idx);
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

/* Replay a single operation */
static int replay_operation(struct recovery_ctx *ctx, const struct wal_entry *entry,
                           void *data) {
    if (!data) return -1;

    switch (entry->op_type) {
        case WAL_OP_INSERT:
            return replay_insert(ctx, (struct wal_insert_data *)data);

        case WAL_OP_DELETE:
            return replay_delete(ctx, (struct wal_delete_data *)data);

        case WAL_OP_UPDATE:
            return replay_update(ctx, (struct wal_update_data *)data);

        case WAL_OP_WRITE:
            /* Write operations don't need replay (data already in tree) */
            ctx->ops_skipped++;
            return 1;

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

        /* Only replay if transaction was committed */
        if (tx && tx->state == TX_COMMITTED) {
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

/* Undo phase: roll back uncommitted transactions */
int recovery_undo(struct recovery_ctx *ctx) {
    if (!ctx) return -1;

    if (ctx->verbose) {
        printf("[RECOVERY] Starting undo phase...\n");
    }

    /* For now, we simply discard uncommitted transactions */
    /* Full undo would require storing old values in WAL */

    uint32_t uncommitted = 0;
    for (uint32_t i = 0; i < ctx->tx_count; i++) {
        if (ctx->tx_table[i].state == TX_ACTIVE) {
            uncommitted++;
        }
    }

    ctx->ops_undone = uncommitted;

    if (ctx->verbose && uncommitted > 0) {
        printf("[RECOVERY] Undo complete: %u uncommitted TX discarded\n",
               uncommitted);
    }

    return 0;
}

/* Run complete recovery */
int recovery_run(struct recovery_ctx *ctx) {
    if (!ctx) return -1;

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
