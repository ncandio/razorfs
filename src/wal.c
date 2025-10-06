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

    return 0;
}

/* Initialize WAL in shared memory mode */
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

        /* Flush header to persistent storage */
        if (msync(wal->header, sizeof(struct wal_header), MS_SYNC) != 0) {
            return -1;
        }
    }

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
    return 0;
}

/* Initialize WAL with file-backed storage */
int wal_init_file(struct wal *wal, const char *filepath, size_t size) {
    if (!wal || !filepath) return -1;
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

    return 0;
}

/* Note: wal_needs_recovery is implemented in recovery.c */

/* Destroy WAL and free resources */
void wal_destroy(struct wal *wal) {
    if (!wal) return;

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

    /* Calculate checksum of entry + data */
    uint32_t crc = wal_crc32(entry, sizeof(struct wal_entry));
    if (data_len > 0 && data) {
        /* Chain CRC for data */
        crc ^= wal_crc32(data, data_len);
    }
    entry->checksum = crc;

    pthread_mutex_lock(&wal->log_lock);

    /* Check available space */
    size_t available = wal_available_space(wal);
    if (entry_size > available) {
        /* TODO: Trigger checkpoint or return ENOSPC */
        pthread_mutex_unlock(&wal->log_lock);
        errno = ENOSPC;
        return -1;
    }

    uint64_t write_offset = wal->header->head_offset;

    /* Handle wraparound */
    if (write_offset + entry_size > wal->buffer_size) {
        /* Wrap to beginning */
        write_offset = 0;
        wal->header->head_offset = 0;
    }

    /* Write entry */
    memcpy(wal->log_buffer + write_offset, entry, sizeof(struct wal_entry));
    if (data_len > 0 && data) {
        memcpy(wal->log_buffer + write_offset + sizeof(struct wal_entry),
               data, data_len);
    }

    /* Update header */
    wal->header->head_offset = write_offset + entry_size;
    wal->header->entry_count++;
    wal->header->next_lsn++;
    update_header_checksum(wal->header);

    /* Flush to persistent storage if in shm mode */
    if (wal->is_shm) {
        msync(wal->log_buffer + write_offset, entry_size, MS_SYNC);
        msync(wal->header, sizeof(struct wal_header), MS_SYNC);
    }

    pthread_mutex_unlock(&wal->log_lock);
    return 0;
}

/* Begin a new transaction */
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

    /* Log checkpoint entry */
    struct wal_entry entry = {
        .tx_id = 0,  // Checkpoint has no TX
        .lsn = wal->header->next_lsn,
        .op_type = WAL_OP_CHECKPOINT,
        .data_len = 0,
        .timestamp = wal_timestamp(),
        .checksum = 0,
        .reserved = 0
    };

    /* Calculate checksum */
    entry.checksum = wal_crc32(&entry, sizeof(entry));

    /* Write checkpoint entry at head */
    uint64_t checkpoint_offset = wal->header->head_offset;
    if (checkpoint_offset + sizeof(entry) > wal->buffer_size) {
        checkpoint_offset = 0;
    }

    memcpy(wal->log_buffer + checkpoint_offset, &entry, sizeof(entry));

    /* Advance tail to checkpoint (reclaim space) */
    wal->header->tail_offset = checkpoint_offset;
    wal->header->checkpoint_lsn = wal->header->next_lsn;
    wal->header->head_offset = checkpoint_offset + sizeof(entry);
    wal->header->entry_count = 1;  // Only checkpoint entry remains
    wal->header->next_lsn++;
    update_header_checksum(wal->header);

    /* Flush to storage */
    if (wal->is_shm) {
        msync(wal->log_buffer + checkpoint_offset, sizeof(entry), MS_SYNC);
        msync(wal->header, sizeof(struct wal_header), MS_SYNC);
    }

    pthread_mutex_unlock(&wal->log_lock);
    return 0;
}

/* Force WAL to persistent storage */
int wal_flush(struct wal *wal) {
    if (!wal || !wal->is_shm) return 0;

    pthread_mutex_lock(&wal->log_lock);
    int ret = msync(wal->header, sizeof(struct wal_header) + wal->buffer_size, MS_SYNC);
    pthread_mutex_unlock(&wal->log_lock);

    return ret;
}

/* Get WAL statistics */
void wal_get_stats(const struct wal *wal, struct wal_stats *stats) {
    if (!wal || !stats) return;

    memset(stats, 0, sizeof(*stats));

    stats->total_entries = wal->header->next_lsn - 1;
    stats->bytes_logged = wal->header->head_offset;

    /* TODO: Track commits/aborts/checkpoints separately */
}
