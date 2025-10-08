/**
 * Lightweight Compression Implementation - RAZORFS Phase 8
 */

#define _GNU_SOURCE
#include "compression.h"
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

/* Global stats */
static struct compression_stats g_stats = {0};

/**
 * Compress data (if beneficial)
 * Returns compressed buffer (caller must free) or NULL if compression not beneficial
 * Sets *out_size to compressed size (including header)
 */
void *compress_data(const void *data, size_t size, size_t *out_size) {
    if (!data || size == 0) {
        return NULL;
    }

    /* Don't compress if smaller than threshold */
    if (size < COMPRESSION_MIN_SIZE) {
        return NULL;
    }

    /* Calculate maximum possible compressed size */
    uLongf compressed_bound = compressBound((uLong)size);
    Bytef *compressed_data = malloc(compressed_bound + sizeof(struct compression_header));
    if (!compressed_data) {
        return NULL;
    }

    /* Prepare header */
    struct compression_header *header = (struct compression_header *)compressed_data;
    header->magic = COMPRESSION_MAGIC;
    header->original_size = (uint32_t)size;
    header->compressed_size = compressed_bound;

    /* Compress data */
    uLongf final_size = compressed_bound;
    int result = compress2(compressed_data + sizeof(struct compression_header), &final_size,
                          (const Bytef *)data, (uLong)size, 1);  /* Level 1 - fastest */

    if (result != Z_OK) {
        free(compressed_data);
        return NULL;
    }

    /* Update header with actual compressed size */
    header->compressed_size = final_size;

    /* Check if compression was actually beneficial */
    size_t total_size = sizeof(struct compression_header) + final_size;
    if (total_size >= size) {
        /* Compression not beneficial - return NULL and let caller use original */
        free(compressed_data);
        return NULL;
    }

    *out_size = total_size;

    /* Update stats */
    g_stats.total_writes++;
    g_stats.compressed_writes++;
    g_stats.bytes_saved += (size - total_size);

    return compressed_data;
}

/**
 * Decompress data
 * Returns decompressed buffer (caller must free) or NULL on error
 * Sets *out_size to decompressed size
 */
void *decompress_data(const void *data, size_t size, size_t *out_size) {
    if (!data || size < sizeof(struct compression_header)) {
        return NULL;
    }

    const struct compression_header *header = (const struct compression_header *)data;

    /* Validate magic */
    if (header->magic != COMPRESSION_MAGIC) {
        return NULL;
    }

    /* Check for integer overflow */
    if (header->compressed_size > SIZE_MAX - sizeof(struct compression_header) ||
        header->original_size > SIZE_MAX) {
        return NULL;
    }

    if (sizeof(struct compression_header) + header->compressed_size > size) {
        return NULL;
    }

    /* Allocate buffer for decompressed data */
    void *output = malloc(header->original_size);
    if (!output) {
        return NULL;
    }

    /* Decompress */
    uLongf decompressed_size = header->original_size;
    int result = uncompress((Bytef *)output, &decompressed_size,
                           (const Bytef *)(header + 1), header->compressed_size);

    if (result != Z_OK) {
        free(output);
        return NULL;
    }

    /* Verify size matches */
    if (decompressed_size != header->original_size) {
        free(output);
        return NULL;
    }

    *out_size = decompressed_size;

    /* Update stats */
    g_stats.total_reads++;
    g_stats.compressed_reads++;

    return output;
}

/**
 * Check if data is compressed
 */
int is_compressed(const void *data, size_t size) __attribute__((unused));
int is_compressed(const void *data, size_t size) {
    if (!data || size < sizeof(struct compression_header)) {
        return 0;
    }

    const struct compression_header *header = (const struct compression_header *)data;
    return (header->magic == COMPRESSION_MAGIC);
}

/**
 * Get compression statistics
 */
void get_compression_stats(struct compression_stats *stats) {
    if (stats) {
        *stats = g_stats;
    }
}

/**
 * Reset compression statistics (for testing)
 */
void reset_compression_stats(void) {
    memset(&g_stats, 0, sizeof(g_stats));
}