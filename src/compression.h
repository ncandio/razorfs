/**
 * Lightweight Compression - RAZORFS Phase 8
 *
 * Strategy:
 * - Only compress files > 512 bytes
 * - Use zlib level 1 (fastest)
 * - Skip if compressed size >= original (no benefit)
 * - Transparent to read/write operations
 */

#ifndef RAZORFS_COMPRESSION_H
#define RAZORFS_COMPRESSION_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Compression threshold - don't compress smaller files */
#define COMPRESSION_MIN_SIZE 512

/* Compression header magic */
#define COMPRESSION_MAGIC 0x525A4350  /* "RZCP" */

/**
 * Compressed data header
 */
struct compression_header {
    uint32_t magic;              /* COMPRESSION_MAGIC */
    uint32_t original_size;      /* Uncompressed size */
    uint32_t compressed_size;    /* Compressed size (without header) */
};

/**
 * Compress data (if beneficial)
 * Returns compressed buffer (caller must free) or NULL if compression not beneficial
 * Sets *out_size to compressed size (including header)
 */
void *compress_data(const void *data, size_t size, size_t *out_size);

/**
 * Decompress data
 * Returns decompressed buffer (caller must free) or NULL on error
 * Sets *out_size to decompressed size
 */
void *decompress_data(const void *data, size_t size, size_t *out_size);

/**
 * Check if data is compressed
 */
int is_compressed(const void *data, size_t size);

/**
 * Get compression statistics
 */
struct compression_stats {
    uint64_t total_reads;
    uint64_t compressed_reads;
    uint64_t total_writes;
    uint64_t compressed_writes;
    uint64_t bytes_saved;
};

void get_compression_stats(struct compression_stats *stats);

#ifdef __cplusplus
}
#endif

#endif /* RAZORFS_COMPRESSION_H */
