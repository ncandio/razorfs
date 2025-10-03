/**
 * Compression Unit Tests
 * Tests for zlib compression/decompression
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <cstring>
#include <random>

extern "C" {
#include "compression.h"
}

class CompressionTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Reset global stats
        get_compression_stats(nullptr);
    }
};

// ============================================================================
// Basic Compression Tests
// ============================================================================

TEST_F(CompressionTest, CompressSmallData) {
    const char* input = "Hello, World! This is a test.";
    size_t input_size = strlen(input);

    size_t output_size;
    void* compressed = compress_data(input, input_size, &output_size);

    ASSERT_NE(compressed, nullptr);
    EXPECT_GT(output_size, 0u);

    free(compressed);
}

TEST_F(CompressionTest, CompressDecompress) {
    const char* input = "The quick brown fox jumps over the lazy dog. "
                       "Pack my box with five dozen liquor jugs.";
    size_t input_size = strlen(input);

    size_t compressed_size;
    void* compressed = compress_data(input, input_size, &compressed_size);
    ASSERT_NE(compressed, nullptr);

    size_t decompressed_size;
    void* decompressed = decompress_data(compressed, compressed_size,
                                         &decompressed_size);
    ASSERT_NE(decompressed, nullptr);

    EXPECT_EQ(decompressed_size, input_size);
    EXPECT_EQ(memcmp(decompressed, input, input_size), 0);

    free(compressed);
    free(decompressed);
}

TEST_F(CompressionTest, CompressEmptyData) {
    const char* input = "";
    size_t input_size = 0;

    size_t output_size;
    void* compressed = compress_data(input, input_size, &output_size);

    // Should handle gracefully (may return null or valid empty)
    if (compressed) {
        free(compressed);
    }
}

TEST_F(CompressionTest, CompressLargeData) {
    const size_t large_size = 1024 * 1024;  // 1MB
    char* large_data = (char*)malloc(large_size);
    ASSERT_NE(large_data, nullptr);

    // Fill with pattern
    for (size_t i = 0; i < large_size; i++) {
        large_data[i] = 'A' + (i % 26);
    }

    size_t compressed_size;
    void* compressed = compress_data(large_data, large_size, &compressed_size);
    ASSERT_NE(compressed, nullptr);

    // Should compress well (repetitive pattern)
    EXPECT_LT(compressed_size, large_size / 2);

    size_t decompressed_size;
    void* decompressed = decompress_data(compressed, compressed_size,
                                         &decompressed_size);
    ASSERT_NE(decompressed, nullptr);

    EXPECT_EQ(decompressed_size, large_size);
    EXPECT_EQ(memcmp(decompressed, large_data, large_size), 0);

    free(large_data);
    free(compressed);
    free(decompressed);
}

// ============================================================================
// Compression Ratio Tests
// ============================================================================

TEST_F(CompressionTest, HighlyCompressibleData) {
    const size_t size = 10000;
    char* data = (char*)malloc(size);
    memset(data, 'X', size);

    size_t compressed_size;
    void* compressed = compress_data(data, size, &compressed_size);
    ASSERT_NE(compressed, nullptr);

    // Highly compressible - should be much smaller
    EXPECT_LT(compressed_size, size / 10);

    free(data);
    free(compressed);
}

TEST_F(CompressionTest, RandomDataCompression) {
    const size_t size = 4096;
    char* data = (char*)malloc(size);

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 255);

    for (size_t i = 0; i < size; i++) {
        data[i] = dis(gen);
    }

    size_t compressed_size;
    void* compressed = compress_data(data, size, &compressed_size);
    ASSERT_NE(compressed, nullptr);

    // Random data won't compress well
    EXPECT_GT(compressed_size, size * 0.9);

    free(data);
    free(compressed);
}

// ============================================================================
// Header Tests
// ============================================================================

TEST_F(CompressionTest, IsCompressedCheck) {
    const char* input = "Test data for compression header check";
    size_t input_size = strlen(input);

    size_t compressed_size;
    void* compressed = compress_data(input, input_size, &compressed_size);
    ASSERT_NE(compressed, nullptr);

    EXPECT_TRUE(is_compressed(compressed, compressed_size));

    // Regular data should not be detected as compressed
    EXPECT_FALSE(is_compressed(input, input_size));

    free(compressed);
}

// ============================================================================
// Statistics Tests
// ============================================================================

TEST_F(CompressionTest, CompressionStats) {
    // Perform some compressions
    const char* data1 = "First test data";
    const char* data2 = "Second test data";

    size_t size1, size2;
    void* comp1 = compress_data(data1, strlen(data1), &size1);
    void* comp2 = compress_data(data2, strlen(data2), &size2);

    ASSERT_NE(comp1, nullptr);
    ASSERT_NE(comp2, nullptr);

    struct compression_stats stats;
    get_compression_stats(&stats);

    EXPECT_EQ(stats.total_writes, 2u);
    EXPECT_EQ(stats.compressed_writes, 2u);
    EXPECT_GT(stats.total_writes, 0u);
    EXPECT_GT(stats.compressed_writes, 0u);
    EXPECT_GT(stats.bytes_saved, 0u);

    free(comp1);
    free(comp2);
}

// ============================================================================
// Error Handling Tests
// ============================================================================

TEST_F(CompressionTest, CompressNullData) {
    size_t output_size;
    void* result = compress_data(nullptr, 100, &output_size);
    EXPECT_EQ(result, nullptr);
}

TEST_F(CompressionTest, DecompressNullData) {
    size_t output_size;
    void* result = decompress_data(nullptr, 100, &output_size);
    EXPECT_EQ(result, nullptr);
}

TEST_F(CompressionTest, DecompressInvalidData) {
    const char* invalid = "This is not compressed data";
    size_t size = strlen(invalid);

    size_t output_size;
    void* result = decompress_data(invalid, size, &output_size);

    // Should fail gracefully
    EXPECT_EQ(result, nullptr);
}

TEST_F(CompressionTest, DecompressCorruptedData) {
    const char* input = "Data to compress and then corrupt";
    size_t input_size = strlen(input);

    size_t compressed_size;
    void* compressed = compress_data(input, input_size, &compressed_size);
    ASSERT_NE(compressed, nullptr);

    // Corrupt the middle of the compressed data
    if (compressed_size > 20) {
        char* corrupt = (char*)compressed;
        corrupt[compressed_size / 2] ^= 0xFF;

        size_t output_size;
        void* result = decompress_data(compressed, compressed_size, &output_size);

        // Should detect corruption and fail
        EXPECT_EQ(result, nullptr);
    }

    free(compressed);
}

// ============================================================================
// Boundary Tests
// ============================================================================

TEST_F(CompressionTest, CompressExactlyPageSize) {
    const size_t page_size = 4096;
    char* data = (char*)malloc(page_size);
    memset(data, 'P', page_size);

    size_t compressed_size;
    void* compressed = compress_data(data, page_size, &compressed_size);
    ASSERT_NE(compressed, nullptr);

    size_t decompressed_size;
    void* decompressed = decompress_data(compressed, compressed_size,
                                         &decompressed_size);
    ASSERT_NE(decompressed, nullptr);

    EXPECT_EQ(decompressed_size, page_size);
    EXPECT_EQ(memcmp(decompressed, data, page_size), 0);

    free(data);
    free(compressed);
    free(decompressed);
}

TEST_F(CompressionTest, MultiplePagesData) {
    const size_t size = 4096 * 5;  // 5 pages
    char* data = (char*)malloc(size);

    for (size_t i = 0; i < size; i++) {
        data[i] = (i % 256);
    }

    size_t compressed_size;
    void* compressed = compress_data(data, size, &compressed_size);
    ASSERT_NE(compressed, nullptr);

    size_t decompressed_size;
    void* decompressed = decompress_data(compressed, compressed_size,
                                         &decompressed_size);
    ASSERT_NE(decompressed, nullptr);

    EXPECT_EQ(decompressed_size, size);
    EXPECT_EQ(memcmp(decompressed, data, size), 0);

    free(data);
    free(compressed);
    free(decompressed);
}

// ============================================================================
// Stress Tests
// ============================================================================

TEST_F(CompressionTest, MultipleCompressionCycles) {
    const char* original = "Stress test data for multiple compression cycles";
    size_t original_size = strlen(original);

    for (int cycle = 0; cycle < 100; cycle++) {
        size_t comp_size;
        void* compressed = compress_data(original, original_size, &comp_size);
        ASSERT_NE(compressed, nullptr) << "Failed at cycle " << cycle;

        size_t decomp_size;
        void* decompressed = decompress_data(compressed, comp_size, &decomp_size);
        ASSERT_NE(decompressed, nullptr) << "Failed at cycle " << cycle;

        EXPECT_EQ(decomp_size, original_size) << "Size mismatch at cycle " << cycle;
        EXPECT_EQ(memcmp(decompressed, original, original_size), 0)
            << "Data mismatch at cycle " << cycle;

        free(compressed);
        free(decompressed);
    }
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
