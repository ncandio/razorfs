/**
 * Write-Ahead Log (WAL) Implementation
 */

#include "wal.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

/* CRC32 lookup table */
static uint32_t crc32_table[256];
static int crc32_table_initialized = 0;

/* Initialize CRC32 lookup table */
static void init_crc32_table(void) {
    if (crc32_table_initialized) return;

    for (uint32_t i = 0; i < 256; i++) {
        uint32_t c = i;
        for (int j = 0; j < 8; j++) {
            c = (c & 1) ? (0xEDB88320 ^ (c >> 1)) : (c >> 1);
        }
        crc32_table[i] = c;
    }
    crc32_table_initialized = 1;
}

/* Calculate CRC32 checksum */
uint32_t wal_crc32(const void *data, size_t len) {
    if (!crc32_table_initialized) {
        init_crc32_table();
    }

    const uint8_t *bytes = (const uint8_t *)data;
    uint32_t crc = 0xFFFFFFFF;

    for (size_t i = 0; i < len; i++) {
        crc = crc32_table[(crc ^ bytes[i]) & 0xFF] ^ (crc >> 8);
    }

    return crc ^ 0xFFFFFFFF;
}

/* Get current timestamp in microseconds */
uint64_t wal_timestamp(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000000 + tv.tv_usec;
}

#define GF2_DIM 32

static uint32_t gf2_matrix_times(const uint32_t *mat, uint32_t vec) {
    uint32_t sum = 0;
    int i = 0;
    while (vec) {
        if (vec & 1) {
            sum ^= mat[i];
        }
        vec >>= 1;
        i++;
    }
    return sum;
}

static void gf2_matrix_square(uint32_t *square, const uint32_t *mat) {
    for (int i = 0; i < GF2_DIM; i++) {
        square[i] = gf2_matrix_times(mat, mat[i]);
    }
}

uint32_t wal_crc32_combine(uint32_t crc1, uint32_t crc2, size_t len2) {
    if (len2 == 0) {
        return crc1;
    }

    uint32_t even[GF2_DIM];
    uint32_t odd[GF2_DIM];

    // operator for one zero bit
    odd[0] = 0xEDB88320L;          // CRC-32 polynomial
    uint32_t row = 1;
    for (int i = 1; i < GF2_DIM; i++) {
        odd[i] = row;
        row <<= 1;
    }

    // square to get operator for two zero bits
    gf2_matrix_square(even, odd);

    // square to get operator for four zero bits
    gf2_matrix_square(odd, even);

    // apply len2 zeros to crc1
    do {
        // apply matrix multiplication for each bit of len2
        gf2_matrix_square(even, odd);
        if (len2 & 1) {
            crc1 = gf2_matrix_times(even, crc1);
        }
        len2 >>= 1;

        if (len2 == 0) {
            break;
        }

        gf2_matrix_square(odd, even);
        if (len2 & 1) {
            crc1 = gf2_matrix_times(odd, crc1);
        }
        len2 >>= 1;
    } while (len2 != 0);

    crc1 ^= crc2;
    return crc1;
}

/* Calculate header checksum (excluding checksum field itself) */
static uint32_t calc_header_checksum(const struct wal_header *header) {
    /* Checksum everything except the checksum field */
    size_t offset = offsetof(struct wal_header, checksum);
    return wal_crc32(header, offset);
}

/* Validate WAL header */
static int validate_header(const struct wal_header *header) {
    if (header->magic != WAL_MAGIC) {
        return -1;
    }
    if (header->version != WAL_VERSION) {
        return -1;
    }

    uint32_t expected = calc_header_checksum(header);
    if (header->checksum != expected) {
        return -1;
    }

    return 0;
}

/* Update and write header checksum */
static void update_header_checksum(struct wal_header *header) {
    header->checksum = calc_header_checksum(header);
}

/* Initialize WAL in heap mode */
int wal_init(struct wal *wal, size_t size) __attribute__((unused));
int wal_init(struct wal *wal, size_t size) {
    if (!wal) return -1;
    if (size < WAL_MIN_SIZE) size = WAL_DEFAULT_SIZE;
    if (size > WAL_MAX_SIZE) size = WAL_MAX_SIZE;

    memset(wal, 0, sizeof(*wal));

    /* Allocate buffer (header + log) */
    size_t total_size = sizeof(struct wal_header) + size;
    void *buffer = malloc(total_size);
    if (!buffer) {
        return -1;
    }
    memset(buffer, 0, total_size);

    wal->header = (struct wal_header *)buffer;
    wal->log_buffer = (char *)buffer + sizeof(struct wal_header);
    wal->buffer_size = size;
    wal->is_shm = 0;

    /* Initialize header */
    wal->header->magic = WAL_MAGIC;
    wal->header->version = WAL_VERSION;
    wal->header->next_tx_id = 1;
    wal->header->next_lsn = 1;
    wal->header->head_offset = 0;
    wal->header->tail_offset = 0;
    wal->header->checkpoint_lsn = 0;
    wal->header->entry_count = 0;
    update_header_checksum(wal->header);

    /* Initialize locks */
    if (pthread_mutex_init(&wal->log_lock, NULL) != 0) {
        free(buffer);
        return -1;
    }
    if (pthread_mutex_init(&wal->tx_lock, NULL) != 0) {
        pthread_mutex_destroy(&wal->log_lock);
        free(buffer);
        return -1;
    }

    /* Initialize checkpoint automation fields */
    wal->auto_checkpoint = 0;
    wal->checkpoint_thread_running = 0;
    wal->last_checkpoint_time = wal_timestamp();
    if (pthread_mutex_init(&wal->checkpoint_lock, NULL) != 0) {
        pthread_mutex_destroy(&wal->tx_lock);
        pthread_mutex_destroy(&wal->log_lock);
        free(buffer);
        return -1;
    }
    if (pthread_cond_init(&wal->checkpoint_cond, NULL) != 0) {
        pthread_mutex_destroy(&wal->checkpoint_lock);
        pthread_mutex_destroy(&wal->tx_lock);
        pthread_mutex_destroy(&wal->log_lock);
        free(buffer);
        return -1;
    }

    return 0;
}

/* Initialize WAL in shared memory mode */
int wal_init_shm(struct wal *wal, void *shm_buffer, size_t size, int existing) __attribute__((unused));
int wal_init_shm(struct wal *wal, void *shm_buffer, size_t size, int existing) {
    if (!wal || !shm_buffer) return -1;
    if (size < sizeof(struct wal_header) + WAL_MIN_SIZE) {
        return -1;
    }

    memset(wal, 0, sizeof(*wal));

    wal->header = (struct wal_header *)shm_buffer;
    wal->log_buffer = (char *)shm_buffer + sizeof(struct wal_header);
    wal->buffer_size = size - sizeof(struct wal_header);
    wal->is_shm = 1;
    wal->fd = -1;

    if (existing) {
        /* Attach to existing WAL - validate header */
        if (validate_header(wal->header) != 0) {
            return -1;
        }
    } else {
        /* Create new WAL */
        memset(shm_buffer, 0, size);

        wal->header->magic = WAL_MAGIC;
        wal->header->version = WAL_VERSION;
        wal->header->next_tx_id = 1;
        wal->header->next_lsn = 1;
        wal->header->head_offset = 0;
        wal->header->tail_offset = 0;
        wal->header->checkpoint_lsn = 0;
        wal->header->entry_count = 0;
        update_header_checksum(wal->header);

        /* Initialize locks */
        pthread_mutexattr_t attr;
        pthread_mutexattr_init(&attr);
        pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);

        if (pthread_mutex_init(&wal->log_lock, &attr) != 0) {
            pthread_mutexattr_destroy(&attr);
            return -1;
        }
        if (pthread_mutex_init(&wal->tx_lock, &attr) != 0) {
            pthread_mutex_destroy(&wal->log_lock);
            pthread_mutexattr_destroy(&attr);
            return -1;
        }

        pthread_mutexattr_destroy(&attr);

        /* Flush header to persistent storage */
        if (msync(wal->header, sizeof(struct wal_header), MS_SYNC) != 0) {
            return -1;
        }
    }
    return 0;
}

/* Initialize WAL with file-backed storage */
int wal_init_file(struct wal *wal, const char *filepath, size_t size) {
    if (!wal || !filepath) return -1;
    if (size == 0) return -1; // Add this check
    if (size < WAL_MIN_SIZE) size = WAL_DEFAULT_SIZE;
    if (size > WAL_MAX_SIZE) size = WAL_MAX_SIZE;

    memset(wal, 0, sizeof(*wal));

    size_t total_size = sizeof(struct wal_header) + size;

    /* Open or create WAL file */
    int fd = open(filepath, O_RDWR | O_CREAT, 0600);
    if (fd < 0) {
        return -1;
    }

    /* Get file size */
    struct stat st;
    int existing = 0;
    if (fstat(fd, &st) == 0 && st.st_size >= (off_t)total_size) {
        existing = 1;
    } else {
        /* Resize file to needed size */
        if (ftruncate(fd, total_size) != 0) {
            close(fd);
            return -1;
        }
    }

    /* Memory-map the file */
    void *addr = mmap(NULL, total_size, PROT_READ | PROT_WRITE,
                      MAP_SHARED, fd, 0);
    if (addr == MAP_FAILED) {
        close(fd);
        return -1;
    }

    wal->header = (struct wal_header *)addr;
    wal->log_buffer = (char *)addr + sizeof(struct wal_header);
    wal->buffer_size = size;
    wal->is_shm = 0;
    wal->fd = fd;

    if (existing) {
        /* Attach to existing WAL - validate header */
        if (validate_header(wal->header) != 0) {
            /* Invalid header - reinitialize */
            existing = 0;
        }
    }

    if (!existing) {
        /* Create new WAL */
        memset(addr, 0, total_size);
        wal->header->magic = WAL_MAGIC;
        wal->header->version = WAL_VERSION;
        wal->header->next_tx_id = 1;
        wal->header->next_lsn = 1;
        wal->header->head_offset = 0;
        wal->header->tail_offset = 0;
        wal->header->checkpoint_lsn = 0;
        wal->header->entry_count = 0;
        update_header_checksum(wal->header);

        /* Flush header to disk */
        if (msync(wal->header, sizeof(struct wal_header), MS_SYNC) != 0) {
            munmap(addr, total_size);
            close(fd);
            return -1;
        }

        /* Ensure WAL file is persisted to stable storage */
        if (fsync(fd) != 0) {
            munmap(addr, total_size);
            close(fd);
            return -1;
        }
    }

    /* Initialize locks */
    if (pthread_mutex_init(&wal->log_lock, NULL) != 0) {
        munmap(addr, total_size);
        close(fd);
        return -1;
    }
    if (pthread_mutex_init(&wal->tx_lock, NULL) != 0) {
        pthread_mutex_destroy(&wal->log_lock);
        munmap(addr, total_size);
        close(fd);
        return -1;
    }

    /* Initialize checkpoint automation fields */
    wal->auto_checkpoint = 0;
    wal->checkpoint_thread_running = 0;
    wal->last_checkpoint_time = wal_timestamp();
    if (pthread_mutex_init(&wal->checkpoint_lock, NULL) != 0) {
        pthread_mutex_destroy(&wal->tx_lock);
        pthread_mutex_destroy(&wal->log_lock);
        munmap(addr, total_size);
        close(fd);
        return -1;
    }
    if (pthread_cond_init(&wal->checkpoint_cond, NULL) != 0) {
        pthread_mutex_destroy(&wal->checkpoint_lock);
        pthread_mutex_destroy(&wal->tx_lock);
        pthread_mutex_destroy(&wal->log_lock);
        munmap(addr, total_size);
        close(fd);
        return -1;
    }

    return 0;
}

/* Note: wal_needs_recovery is implemented in recovery.c */

/* Destroy WAL and free resources */
void wal_destroy(struct wal *wal) {
    if (!wal) return;

    /* Stop checkpoint thread if running */
    if (wal->checkpoint_thread_running) {
        wal_stop_checkpoint_thread(wal);
    }

    pthread_cond_destroy(&wal->checkpoint_cond);
    pthread_mutex_destroy(&wal->checkpoint_lock);
    pthread_mutex_destroy(&wal->log_lock);
    pthread_mutex_destroy(&wal->tx_lock);

    if (wal->fd >= 0) {
        /* File-backed WAL - unmap and close */
        size_t total_size = sizeof(struct wal_header) + wal->buffer_size;
        if (wal->header) {
            msync(wal->header, total_size, MS_SYNC);
            munmap(wal->header, total_size);
        }
        close(wal->fd);
    } else if (!wal->is_shm && wal->header) {
        /* Heap-backed WAL - free */
        free(wal->header);
    }

    memset(wal, 0, sizeof(*wal));
}

/* Get available space in WAL buffer */
size_t wal_available_space(const struct wal *wal) {
    if (!wal) return 0;

    uint64_t head = wal->header->head_offset;
    uint64_t tail = wal->header->tail_offset;

    if (head >= tail) {
        /* Head is ahead: available = total - used */
        uint64_t used = head - tail;
        return wal->buffer_size - used;
    } else {
        /* Head wrapped: available = tail - head */
        return tail - head;
    }
}

/* Check if WAL is valid */
int wal_is_valid(const struct wal *wal) __attribute__((unused));
int wal_is_valid(const struct wal *wal) {
    if (!wal || !wal->header) return 0;
    return validate_header(wal->header) == 0;
}

/* Append log entry to WAL */
static int wal_append_entry(struct wal *wal, struct wal_entry *entry,
                           const void *data, size_t data_len) {
    if (!wal || !entry) return -1;

    size_t entry_size = sizeof(struct wal_entry) + data_len;
    entry->data_len = data_len;
    entry->checksum = 0; // Will be calculated in-place later

    pthread_mutex_lock(&wal->log_lock);

    /* Check available space */
    size_t available = wal_available_space(wal);
    if (entry_size > available) {
        /* Attempt automatic checkpoint if enabled */
        if (wal->auto_checkpoint) {
            pthread_mutex_unlock(&wal->log_lock);

            /* Trigger checkpoint to reclaim space */
            if (wal_checkpoint(wal) == 0) {
                /* Retry after checkpoint */
                pthread_mutex_lock(&wal->log_lock);
                available = wal_available_space(wal);
                if (entry_size <= available) {
                    /* Checkpoint freed enough space - continue below */
                    goto space_available;
                }
                pthread_mutex_unlock(&wal->log_lock);
            }

            /* Checkpoint failed or didn't free enough space */
            errno = ENOSPC;
            return -1;
        }

        /* Auto-checkpoint disabled - return ENOSPC */
        pthread_mutex_unlock(&wal->log_lock);
        errno = ENOSPC;
        return -1;
    }

space_available:

    uint64_t write_offset = wal->header->head_offset;

    /* Handle wraparound */
    if (write_offset + entry_size > wal->buffer_size) {
        /* Check if there's enough space at the beginning */
        if (entry_size > wal->header->tail_offset) {
            pthread_mutex_unlock(&wal->log_lock);
            errno = ENOSPC;
            return -1;
        }
        write_offset = 0;
    }

    /* Write entry and data to log buffer */
    memcpy(wal->log_buffer + write_offset, entry, sizeof(struct wal_entry));
    if (data_len > 0 && data) {
        memcpy(wal->log_buffer + write_offset + sizeof(struct wal_entry),
               data, data_len);
    }

    /* Get a pointer to the entry in the log and calculate checksum in-place */
    struct wal_entry *entry_in_log = (struct wal_entry *)(wal->log_buffer + write_offset);

    /* CRITICAL: Zero out checksum field before calculating checksum.
     * The checksum must be calculated with checksum field set to 0,
     * matching the validation logic in recovery.c */
    entry_in_log->checksum = 0;

    /* Calculate checksum over entry with checksum field = 0, then over data */
    uint32_t checksum = wal_crc32(entry_in_log, sizeof(struct wal_entry));
    if (data_len > 0) {
        uint32_t data_checksum = wal_crc32(data, data_len);
        checksum = wal_crc32_combine(checksum, data_checksum, data_len);
    }

    /* Now store the calculated checksum */
    entry_in_log->checksum = checksum;

    /* Update header */
    wal->header->head_offset = write_offset + entry_size;
    wal->header->entry_count++;
    wal->header->next_lsn++;
    update_header_checksum(wal->header);

    /* Flush to persistent storage if in shm mode or file-backed */
    if (wal->is_shm || wal->fd >= 0) {
        msync(wal->log_buffer + write_offset, entry_size, MS_SYNC);
        msync(wal->header, sizeof(struct wal_header), MS_SYNC);
    }

    pthread_mutex_unlock(&wal->log_lock);
    return 0;
}

/* Begin a new transaction */
int wal_begin_tx(struct wal *wal, uint64_t *tx_id) __attribute__((unused));
int wal_begin_tx(struct wal *wal, uint64_t *tx_id) {
    if (!wal || !tx_id) return -1;

    pthread_mutex_lock(&wal->tx_lock);
    *tx_id = wal->header->next_tx_id++;
    update_header_checksum(wal->header);
    pthread_mutex_unlock(&wal->tx_lock);

    /* Log BEGIN entry */
    struct wal_entry entry = {
        .tx_id = *tx_id,
        .lsn = wal->header->next_lsn,
        .op_type = WAL_OP_BEGIN,
        .data_len = 0,
        .timestamp = wal_timestamp(),
        .checksum = 0,
        .reserved = 0
    };

    return wal_append_entry(wal, &entry, NULL, 0);
}

/* Commit a transaction */
int wal_commit_tx(struct wal *wal, uint64_t tx_id) __attribute__((unused));
int wal_commit_tx(struct wal *wal, uint64_t tx_id) {
    if (!wal) return -1;

    struct wal_entry entry = {
        .tx_id = tx_id,
        .lsn = wal->header->next_lsn,
        .op_type = WAL_OP_COMMIT,
        .data_len = 0,
        .timestamp = wal_timestamp(),
        .checksum = 0,
        .reserved = 0
    };

    int ret = wal_append_entry(wal, &entry, NULL, 0);
    if (ret == 0 && wal->is_shm) {
        /* Force flush on commit */
        msync(wal->header, sizeof(struct wal_header), MS_SYNC);
    }

    return ret;
}

/* Abort a transaction */
int wal_abort_tx(struct wal *wal, uint64_t tx_id) __attribute__((unused));
int wal_abort_tx(struct wal *wal, uint64_t tx_id) {
    if (!wal) return -1;

    struct wal_entry entry = {
        .tx_id = tx_id,
        .lsn = wal->header->next_lsn,
        .op_type = WAL_OP_ABORT,
        .data_len = 0,
        .timestamp = wal_timestamp(),
        .checksum = 0,
        .reserved = 0
    };

    return wal_append_entry(wal, &entry, NULL, 0);
}

/* Log an insert operation */
int wal_log_insert(struct wal *wal, uint64_t tx_id,
                   const struct wal_insert_data *data) __attribute__((unused));
int wal_log_insert(struct wal *wal, uint64_t tx_id,
                   const struct wal_insert_data *data) {
    if (!wal || !data) return -1;

    struct wal_entry entry = {
        .tx_id = tx_id,
        .lsn = wal->header->next_lsn,
        .op_type = WAL_OP_INSERT,
        .data_len = sizeof(struct wal_insert_data),
        .timestamp = wal_timestamp(),
        .checksum = 0,
        .reserved = 0
    };

    return wal_append_entry(wal, &entry, data, sizeof(*data));
}

/* Log a delete operation */
int wal_log_delete(struct wal *wal, uint64_t tx_id,
                   const struct wal_delete_data *data) __attribute__((unused));
int wal_log_delete(struct wal *wal, uint64_t tx_id,
                   const struct wal_delete_data *data) {
    if (!wal || !data) return -1;

    struct wal_entry entry = {
        .tx_id = tx_id,
        .lsn = wal->header->next_lsn,
        .op_type = WAL_OP_DELETE,
        .data_len = sizeof(struct wal_delete_data),
        .timestamp = wal_timestamp(),
        .checksum = 0,
        .reserved = 0
    };

    return wal_append_entry(wal, &entry, data, sizeof(*data));
}

/* Log an update operation */
int wal_log_update(struct wal *wal, uint64_t tx_id,
                   const struct wal_update_data *data) __attribute__((unused));
int wal_log_update(struct wal *wal, uint64_t tx_id,
                   const struct wal_update_data *data) {
    if (!wal || !data) return -1;

    struct wal_entry entry = {
        .tx_id = tx_id,
        .lsn = wal->header->next_lsn,
        .op_type = WAL_OP_UPDATE,
        .data_len = sizeof(struct wal_update_data),
        .timestamp = wal_timestamp(),
        .checksum = 0,
        .reserved = 0
    };

    return wal_append_entry(wal, &entry, data, sizeof(*data));
}

/* Log a write operation */
int wal_log_write(struct wal *wal, uint64_t tx_id,
                  const struct wal_write_data *data) __attribute__((unused));
int wal_log_write(struct wal *wal, uint64_t tx_id,
                  const struct wal_write_data *data) {
    if (!wal || !data) return -1;

    struct wal_entry entry = {
        .tx_id = tx_id,
        .lsn = wal->header->next_lsn,
        .op_type = WAL_OP_WRITE,
        .data_len = sizeof(struct wal_write_data),
        .timestamp = wal_timestamp(),
        .checksum = 0,
        .reserved = 0
    };

    return wal_append_entry(wal, &entry, data, sizeof(*data));
}

/* Perform a checkpoint */
int wal_checkpoint(struct wal *wal) {
    if (!wal) return -1;

    pthread_mutex_lock(&wal->log_lock);

    /*
     * NOTE: This is a MINIMAL checkpoint implementation that writes a checkpoint
     * record and updates the checkpoint LSN. For a production filesystem, this
     * MUST be extended to:
     *
     * 1. Coordinate with the main data store to force all dirty data covered by
     *    log entries up to the checkpoint LSN to persistent storage (fsync).
     * 2. Only after data is durable, write the checkpoint record.
     * 3. Consider advancing tail_offset to reclaim space (done here conservatively).
     *
     * Without proper coordination with the data store, checkpoints can still lead
     * to data loss on crash. This implementation provides the WAL mechanism, but
     * the caller MUST ensure data durability before calling this function.
     */

    uint64_t checkpoint_lsn = wal->header->next_lsn;

    /* Create checkpoint entry */
    struct wal_entry entry = {
        .tx_id = 0,  /* Checkpoint is not part of any transaction */
        .lsn = checkpoint_lsn,
        .op_type = WAL_OP_CHECKPOINT,
        .data_len = 0,
        .timestamp = wal_timestamp(),
        .checksum = 0,
        .reserved = 0
    };

    /* Calculate checksum with checksum field zeroed */
    uint32_t checksum = wal_crc32(&entry, sizeof(struct wal_entry));
    entry.checksum = checksum;

    /* Calculate space needed */
    uint64_t entry_size = sizeof(struct wal_entry);
    uint64_t available = wal_available_space(wal);

    if (available < entry_size) {
        pthread_mutex_unlock(&wal->log_lock);
        return -1;  /* Not enough space for checkpoint record */
    }

    /* Get write position */
    uint64_t write_offset = wal->header->head_offset;

    /* Copy checkpoint entry to log buffer */
    memcpy(wal->log_buffer + write_offset, &entry, sizeof(struct wal_entry));

    /* Update header */
    wal->header->head_offset = (write_offset + entry_size) % wal->buffer_size;
    wal->header->next_lsn++;
    wal->header->entry_count++;
    wal->header->checkpoint_lsn = checkpoint_lsn;
    update_header_checksum(wal->header);

    /* Force checkpoint record to storage */
    if (wal->is_shm || wal->fd >= 0) {
        msync(wal->log_buffer + write_offset, entry_size, MS_SYNC);
        msync(wal->header, sizeof(struct wal_header), MS_SYNC);
    }

    /* Conservatively advance tail to reclaim space up to the checkpoint.
     * In a full implementation, we'd scan for the oldest transaction that's
     * still active and only advance tail up to that point. For now, we
     * advance tail to the checkpoint LSN, assuming all earlier transactions
     * are complete. */
    if (wal->header->entry_count > 100) {  /* Only reclaim if log is getting full */
        /* Scan forward from current tail to find the checkpoint entry */
        uint64_t scan_offset = wal->header->tail_offset;
        uint64_t head = wal->header->head_offset;

        while (scan_offset != head) {
            struct wal_entry *scan_entry = (struct wal_entry *)(wal->log_buffer + scan_offset);

            /* If we found the checkpoint entry, advance tail to just after it */
            if (scan_entry->op_type == WAL_OP_CHECKPOINT && scan_entry->lsn == checkpoint_lsn) {
                uint64_t checkpoint_size = sizeof(struct wal_entry) + scan_entry->data_len;
                wal->header->tail_offset = (scan_offset + checkpoint_size) % wal->buffer_size;
                update_header_checksum(wal->header);

                if (wal->is_shm || wal->fd >= 0) {
                    msync(wal->header, sizeof(struct wal_header), MS_SYNC);
                }
                break;
            }

            /* Move to next entry */
            uint64_t scan_size = sizeof(struct wal_entry) + scan_entry->data_len;
            scan_offset = (scan_offset + scan_size) % wal->buffer_size;
        }
    }

    pthread_mutex_unlock(&wal->log_lock);
    return 0;
}

/* Force WAL to persistent storage */
int wal_flush(struct wal *wal) __attribute__((unused));
int wal_flush(struct wal *wal) {
    if (!wal || !wal->is_shm) return 0;

    pthread_mutex_lock(&wal->log_lock);
    int ret = msync(wal->header, sizeof(struct wal_header) + wal->buffer_size, MS_SYNC);
    pthread_mutex_unlock(&wal->log_lock);

    return ret;
}

/* Get WAL statistics */
void wal_get_stats(const struct wal *wal, struct wal_stats *stats) __attribute__((unused));
void wal_get_stats(const struct wal *wal, struct wal_stats *stats) {
    if (!wal || !stats) return;

    memset(stats, 0, sizeof(*stats));

    stats->total_entries = wal->header->next_lsn - 1;
    stats->bytes_logged = wal->header->head_offset;
    stats->total_checkpoints = 0; /* TODO: Track in header */
    stats->total_commits = 0;     /* TODO: Track in header */
    stats->total_aborts = 0;      /* TODO: Track in header */
}

/* === Checkpoint Automation === */

/**
 * Check if checkpoint is needed based on thresholds
 */
int wal_should_checkpoint(const struct wal *wal) {
    if (!wal || !wal->header) return 0;

    /* Check size threshold */
    size_t used = wal->buffer_size - wal_available_space(wal);
    double usage = (double)used / wal->buffer_size;
    if (usage >= WAL_CHECKPOINT_SIZE_THRESHOLD) {
        return 1;  /* WAL is 75% full */
    }

    /* Check entry count threshold */
    if (wal->header->entry_count >= WAL_CHECKPOINT_ENTRY_THRESHOLD) {
        return 1;  /* Too many entries */
    }

    /* Check time threshold (if auto-checkpoint enabled) */
    if (wal->auto_checkpoint) {
        uint64_t now = wal_timestamp();
        uint64_t elapsed_sec = (now - wal->last_checkpoint_time) / 1000000;
        if (elapsed_sec >= WAL_CHECKPOINT_TIME_INTERVAL) {
            return 1;  /* Time threshold exceeded */
        }
    }

    return 0;  /* No checkpoint needed */
}

/**
 * Enable/disable automatic checkpointing
 */
int wal_set_auto_checkpoint(struct wal *wal, int enable) {
    if (!wal) return -1;

    pthread_mutex_lock(&wal->checkpoint_lock);
    wal->auto_checkpoint = enable;
    pthread_mutex_unlock(&wal->checkpoint_lock);

    return 0;
}

/**
 * Background checkpoint thread function
 */
static void *checkpoint_thread_func(void *arg) {
    struct wal *wal = (struct wal *)arg;

    while (1) {
        pthread_mutex_lock(&wal->checkpoint_lock);

        /* Check if we should stop */
        if (!wal->checkpoint_thread_running) {
            pthread_mutex_unlock(&wal->checkpoint_lock);
            break;
        }

        /* Wait for signal or timeout */
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += WAL_CHECKPOINT_TIME_INTERVAL;

        pthread_cond_timedwait(&wal->checkpoint_cond, &wal->checkpoint_lock, &ts);

        /* Check again if we should stop (spurious wakeup or signal) */
        if (!wal->checkpoint_thread_running) {
            pthread_mutex_unlock(&wal->checkpoint_lock);
            break;
        }

        pthread_mutex_unlock(&wal->checkpoint_lock);

        /* Check if checkpoint is needed */
        if (wal_should_checkpoint(wal)) {
            /* Perform checkpoint */
            if (wal_checkpoint(wal) == 0) {
                pthread_mutex_lock(&wal->checkpoint_lock);
                wal->last_checkpoint_time = wal_timestamp();
                pthread_mutex_unlock(&wal->checkpoint_lock);
            }
        }
    }

    return NULL;
}

/**
 * Start background checkpoint thread
 */
int wal_start_checkpoint_thread(struct wal *wal) {
    if (!wal) return -1;

    pthread_mutex_lock(&wal->checkpoint_lock);

    if (wal->checkpoint_thread_running) {
        pthread_mutex_unlock(&wal->checkpoint_lock);
        return 0;  /* Already running */
    }

    wal->checkpoint_thread_running = 1;
    wal->auto_checkpoint = 1;  /* Enable auto-checkpoint */

    if (pthread_create(&wal->checkpoint_thread, NULL, checkpoint_thread_func, wal) != 0) {
        wal->checkpoint_thread_running = 0;
        wal->auto_checkpoint = 0;
        pthread_mutex_unlock(&wal->checkpoint_lock);
        return -1;
    }

    pthread_mutex_unlock(&wal->checkpoint_lock);
    return 0;
}

/**
 * Stop background checkpoint thread
 */
int wal_stop_checkpoint_thread(struct wal *wal) {
    if (!wal) return -1;

    pthread_mutex_lock(&wal->checkpoint_lock);

    if (!wal->checkpoint_thread_running) {
        pthread_mutex_unlock(&wal->checkpoint_lock);
        return 0;  /* Not running */
    }

    /* Signal thread to stop */
    wal->checkpoint_thread_running = 0;
    pthread_cond_signal(&wal->checkpoint_cond);

    pthread_mutex_unlock(&wal->checkpoint_lock);

    /* Wait for thread to finish */
    pthread_join(wal->checkpoint_thread, NULL);

    return 0;
}
