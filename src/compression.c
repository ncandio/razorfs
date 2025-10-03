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

int is_compressed(const void *data, size_t size) {
    if (!data || size < sizeof(struct compression_header)) {
        return 0;
    }

    const struct compression_header *hdr = data;
    return hdr->magic == COMPRESSION_MAGIC;
}

void *compress_data(const void *data, size_t size, size_t *out_size) {
    if (!data || !out_size || size < COMPRESSION_MIN_SIZE) {
        return NULL;  /* Too small, not worth compressing */
    }

    /* Allocate worst-case buffer (header + compressed data) */
    uLongf compressed_bound = compressBound(size);
    size_t total_size = sizeof(struct compression_header) + compressed_bound;

    unsigned char *buffer = malloc(total_size);
    if (!buffer) {
        return NULL;
    }

    /* Compress data (zlib level 1 = fastest) */
    uLongf compressed_size = compressed_bound;
    struct compression_header *hdr = (struct compression_header *)buffer;
    unsigned char *compressed_data = buffer + sizeof(struct compression_header);

    int ret = compress2(compressed_data, &compressed_size, data, size, 1);
    if (ret != Z_OK) {
        free(buffer);
        return NULL;
    }

    /* Check if compression was beneficial */
    size_t final_size = sizeof(struct compression_header) + compressed_size;
    if (final_size >= size) {
        free(buffer);
        return NULL;  /* No benefit */
    }

    /* Fill header */
    hdr->magic = COMPRESSION_MAGIC;
    hdr->original_size = size;
    hdr->compressed_size = compressed_size;

    *out_size = final_size;

    /* Update stats */
    __sync_fetch_and_add(&g_stats.total_writes, 1);
    __sync_fetch_and_add(&g_stats.compressed_writes, 1);
    __sync_fetch_and_add(&g_stats.bytes_saved, size - final_size);

    return buffer;
}

void *decompress_data(const void *data, size_t size, size_t *out_size) {
    if (!data || !out_size || size < sizeof(struct compression_header)) {
        return NULL;
    }

    const struct compression_header *hdr = data;

    /* Verify magic */
    if (hdr->magic != COMPRESSION_MAGIC) {
        return NULL;
    }

    /* Allocate output buffer */
    unsigned char *output = malloc(hdr->original_size);
    if (!output) {
        return NULL;
    }

    /* Decompress */
    const unsigned char *compressed_data = (const unsigned char *)data + sizeof(struct compression_header);
    uLongf decompressed_size = hdr->original_size;

    int ret = uncompress(output, &decompressed_size, compressed_data, hdr->compressed_size);
    if (ret != Z_OK || decompressed_size != hdr->original_size) {
        free(output);
        return NULL;
    }

    *out_size = decompressed_size;

    /* Update stats */
    __sync_fetch_and_add(&g_stats.total_reads, 1);
    __sync_fetch_and_add(&g_stats.compressed_reads, 1);

    return output;
}

void get_compression_stats(struct compression_stats *stats) {
    if (!stats) return;

    stats->total_reads = g_stats.total_reads;
    stats->compressed_reads = g_stats.compressed_reads;
    stats->total_writes = g_stats.total_writes;
    stats->compressed_writes = g_stats.compressed_writes;
    stats->bytes_saved = g_stats.bytes_saved;
}

void reset_compression_stats(void) {
    g_stats.total_reads = 0;
    g_stats.compressed_reads = 0;
    g_stats.total_writes = 0;
    g_stats.compressed_writes = 0;
    g_stats.bytes_saved = 0;
}
