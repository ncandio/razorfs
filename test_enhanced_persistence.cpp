#include <iostream>
#include <chrono>
#include <random>
#include <vector>
#include <string>
#include <thread>
#include <fstream>
#include <cassert>
#include <sys/stat.h>
#include <signal.h>
#include <unistd.h>

#include "src/razorfs_persistence.hpp"

class PersistenceTestSuite {
private:
    std::string test_data_path_;
    std::mt19937 rng_;
    std::vector<std::string> test_results_;

public:
    PersistenceTestSuite() : test_data_path_("/tmp/razorfs_test"), rng_(std::random_device{}()) {
        cleanup_test_files();
    }

    ~PersistenceTestSuite() {
        cleanup_test_files();
        print_test_summary();
    }

    void run_all_tests() {
        std::cout << "=== Enhanced RazorFS Persistence Test Suite ===" << std::endl;
        std::cout << "Testing crash safety, data integrity, and performance" << std::endl;
        std::cout << std::endl;

        test_basic_persistence();
        test_string_table();
        test_crc32_integrity();
        test_journal_functionality();
        test_crash_recovery();
        test_concurrent_access();
        test_large_dataset();
        test_performance_benchmark();
        test_corruption_detection();
        test_version_compatibility();
    }

private:
    void cleanup_test_files() {
        std::vector<std::string> files = {
            test_data_path_ + ".dat",
            test_data_path_ + ".journal",
            test_data_path_ + "_backup.dat",
            test_data_path_ + "_corrupt.dat"
        };

        for (const auto& file : files) {
            std::remove(file.c_str());
        }
    }

    void add_test_result(const std::string& test_name, bool passed, const std::string& details = "") {
        std::string result = (passed ? "✅ PASS" : "❌ FAIL") + std::string(": ") + test_name;
        if (!details.empty()) {
            result += " (" + details + ")";
        }
        test_results_.push_back(result);
        std::cout << result << std::endl;
    }

    void print_test_summary() {
        std::cout << std::endl << "=== Test Summary ===" << std::endl;
        size_t passed = 0;
        for (const auto& result : test_results_) {
            std::cout << result << std::endl;
            if (result.find("✅") != std::string::npos) passed++;
        }
        std::cout << std::endl;
        std::cout << "Total: " << test_results_.size() << " tests" << std::endl;
        std::cout << "Passed: " << passed << " tests" << std::endl;
        std::cout << "Failed: " << (test_results_.size() - passed) << " tests" << std::endl;
    }

    std::unordered_map<uint64_t, std::string> generate_test_data(size_t num_files, size_t max_content_size = 1024) {
        std::unordered_map<uint64_t, std::string> file_contents;

        for (size_t i = 0; i < num_files; i++) {
            size_t content_size = rng_() % max_content_size;
            std::string content;
            content.reserve(content_size);

            for (size_t j = 0; j < content_size; j++) {
                content += static_cast<char>(32 + (rng_() % 94)); // Printable ASCII
            }

            file_contents[i + 100] = content; // Start inodes at 100
        }

        return file_contents;
    }

    std::unordered_map<uint64_t, std::string> generate_test_paths(size_t num_files) {
        std::unordered_map<uint64_t, std::string> inode_to_name;

        inode_to_name[1] = "/"; // Root

        for (size_t i = 0; i < num_files; i++) {
            std::string path = "/file_" + std::to_string(i) + ".txt";
            inode_to_name[i + 100] = path;
        }

        return inode_to_name;
    }

    void test_basic_persistence() {
        try {
            auto engine = std::make_unique<razorfs::PersistenceEngine>(
                test_data_path_ + "_basic.dat",
                razorfs::PersistenceMode::SYNCHRONOUS
            );

            // Create test data
            uint64_t next_inode = 150;
            auto inode_to_name = generate_test_paths(10);
            auto file_contents = generate_test_data(10, 500);

            // Save
            bool save_success = engine->save_filesystem(next_inode, inode_to_name, file_contents);
            if (!save_success) {
                add_test_result("Basic Persistence - Save", false, "Save operation failed");
                return;
            }

            // Load
            uint64_t loaded_next_inode;
            std::unordered_map<uint64_t, std::string> loaded_names;
            std::unordered_map<uint64_t, std::string> loaded_contents;

            bool load_success = engine->load_filesystem(loaded_next_inode, loaded_names, loaded_contents);
            if (!load_success) {
                add_test_result("Basic Persistence - Load", false, "Load operation failed");
                return;
            }

            // Verify data integrity
            bool data_intact = (loaded_next_inode == next_inode) &&
                              (loaded_names == inode_to_name) &&
                              (loaded_contents == file_contents);

            add_test_result("Basic Persistence", data_intact,
                          std::to_string(loaded_names.size()) + " files, " +
                          std::to_string(loaded_contents.size()) + " with content");

        } catch (const std::exception& e) {
            add_test_result("Basic Persistence", false, std::string("Exception: ") + e.what());
        }
    }

    void test_string_table() {
        try {
            razorfs::StringTable table;

            // Test string interning
            std::vector<std::string> test_strings = {
                "/",
                "/home",
                "/home/user",
                "/home/user/documents",
                "/home/user/documents/file.txt",
                "/very/long/path/with/many/components/and/a/really/long/filename.txt"
            };

            std::vector<uint32_t> offsets;
            for (const auto& str : test_strings) {
                offsets.push_back(table.intern_string(str));
            }

            // Test duplicate detection
            for (size_t i = 0; i < test_strings.size(); i++) {
                uint32_t duplicate_offset = table.intern_string(test_strings[i]);
                if (duplicate_offset != offsets[i]) {
                    add_test_result("String Table - Deduplication", false,
                                  "Duplicate string got different offset");
                    return;
                }
            }

            // Test retrieval
            bool retrieval_success = true;
            for (size_t i = 0; i < test_strings.size(); i++) {
                std::string retrieved = table.get_string(offsets[i]);
                if (retrieved != test_strings[i]) {
                    retrieval_success = false;
                    break;
                }
            }

            // Test serialization/deserialization
            const auto& data = table.get_data();
            razorfs::StringTable new_table;
            new_table.load_from_data(data.data(), data.size());

            bool serialization_success = true;
            for (size_t i = 0; i < test_strings.size(); i++) {
                std::string retrieved = new_table.get_string(offsets[i]);
                if (retrieved != test_strings[i]) {
                    serialization_success = false;
                    break;
                }
            }

            add_test_result("String Table",
                          retrieval_success && serialization_success,
                          std::to_string(test_strings.size()) + " strings, " +
                          std::to_string(data.size()) + " bytes");

        } catch (const std::exception& e) {
            add_test_result("String Table", false, std::string("Exception: ") + e.what());
        }
    }

    void test_crc32_integrity() {
        try {
            std::vector<std::string> test_data = {
                "",
                "a",
                "abc",
                "The quick brown fox jumps over the lazy dog",
                std::string(1000, 'x'), // Long string
                "Binary data: \x00\x01\x02\x03\xFF\xFE\xFD"
            };

            std::vector<uint32_t> expected_crcs = {
                0x00000000, // Empty string
                0xE8B7BE43, // "a"
                0x352441C2, // "abc"
                0x414FA339, // "The quick brown fox..."
                0, // Will be calculated
                0  // Will be calculated
            };

            // Calculate expected CRCs for longer strings
            expected_crcs[4] = razorfs::CRC32::calculate(test_data[4]);
            expected_crcs[5] = razorfs::CRC32::calculate(test_data[5]);

            bool all_correct = true;
            for (size_t i = 0; i < test_data.size(); i++) {
                uint32_t calculated = razorfs::CRC32::calculate(test_data[i]);
                if (i < 4 && calculated != expected_crcs[i]) {
                    all_correct = false;
                    break;
                }

                // Test consistency
                uint32_t recalculated = razorfs::CRC32::calculate(test_data[i]);
                if (calculated != recalculated) {
                    all_correct = false;
                    break;
                }
            }

            add_test_result("CRC32 Integrity", all_correct,
                          std::to_string(test_data.size()) + " test cases");

        } catch (const std::exception& e) {
            add_test_result("CRC32 Integrity", false, std::string("Exception: ") + e.what());
        }
    }

    void test_journal_functionality() {
        try {
            std::string journal_path = test_data_path_ + "_journal_test.journal";
            std::remove(journal_path.c_str());

            razorfs::Journal journal(journal_path);
            if (!journal.open()) {
                add_test_result("Journal Functionality", false, "Failed to open journal");
                return;
            }

            // Write some journal entries
            std::vector<std::pair<razorfs::JournalEntryType, std::string>> test_entries = {
                {razorfs::JournalEntryType::CREATE_FILE, "test_file.txt"},
                {razorfs::JournalEntryType::WRITE_DATA, "Hello, World!"},
                {razorfs::JournalEntryType::CREATE_DIR, "test_directory"},
                {razorfs::JournalEntryType::DELETE_FILE, ""},
                {razorfs::JournalEntryType::CHECKPOINT, ""}
            };

            for (size_t i = 0; i < test_entries.size(); i++) {
                bool success = journal.write_entry(test_entries[i].first, i + 200,
                                                  test_entries[i].second.data(),
                                                  test_entries[i].second.size());
                if (!success) {
                    add_test_result("Journal Functionality - Write", false,
                                  "Failed to write entry " + std::to_string(i));
                    return;
                }
            }

            journal.close();

            // Read back and verify
            size_t entries_read = 0;
            bool replay_success = journal.replay_journal([&](const razorfs::JournalEntry& entry, const void* data) -> bool {
                if (entries_read < test_entries.size()) {
                    auto& expected = test_entries[entries_read];

                    if (static_cast<razorfs::JournalEntryType>(entry.type) != expected.first) {
                        return false;
                    }

                    if (entry.inode != entries_read + 200) {
                        return false;
                    }

                    if (data && entry.data_size > 0) {
                        std::string received_data(static_cast<const char*>(data), entry.data_size);
                        if (received_data != expected.second) {
                            return false;
                        }
                    }
                }

                entries_read++;
                return true;
            });

            bool test_passed = replay_success && (entries_read == test_entries.size());
            add_test_result("Journal Functionality", test_passed,
                          std::to_string(entries_read) + " entries replayed");

            std::remove(journal_path.c_str());

        } catch (const std::exception& e) {
            add_test_result("Journal Functionality", false, std::string("Exception: ") + e.what());
        }
    }

    void test_crash_recovery() {
        try {
            std::string data_path = test_data_path_ + "_crash_test.dat";
            std::remove(data_path.c_str());
            std::remove((data_path + ".journal").c_str());

            // Create initial state
            {
                auto engine = std::make_unique<razorfs::PersistenceEngine>(
                    data_path, razorfs::PersistenceMode::SYNCHRONOUS);

                auto inode_to_name = generate_test_paths(5);
                auto file_contents = generate_test_data(5, 100);
                engine->save_filesystem(105, inode_to_name, file_contents);
            }

            // Simulate crash during operations (journal entries without final save)
            {
                auto engine = std::make_unique<razorfs::PersistenceEngine>(
                    data_path, razorfs::PersistenceMode::JOURNAL_ONLY);

                // Add some journal entries that won't be saved to main file
                engine->journal_create_file(200, "/crash_test_file.txt", "crash test content");
                engine->journal_create_file(201, "/another_file.txt", "more content");
                engine->journal_write_data(200, "updated content after crash simulation");

                // Don't call save_filesystem - simulate crash
            }

            // Recovery attempt
            {
                auto engine = std::make_unique<razorfs::PersistenceEngine>(
                    data_path, razorfs::PersistenceMode::SYNCHRONOUS);

                uint64_t recovered_next_inode;
                std::unordered_map<uint64_t, std::string> recovered_names;
                std::unordered_map<uint64_t, std::string> recovered_contents;

                bool recovery_success = engine->load_filesystem(recovered_next_inode,
                                                               recovered_names,
                                                               recovered_contents);

                if (!recovery_success) {
                    add_test_result("Crash Recovery", false, "Recovery failed");
                    return;
                }

                // Check if journal entries were applied
                bool journal_applied = (recovered_names.find(200) != recovered_names.end()) &&
                                     (recovered_names.find(201) != recovered_names.end()) &&
                                     (recovered_contents[200] == "updated content after crash simulation");

                add_test_result("Crash Recovery", journal_applied,
                              std::to_string(recovered_names.size()) + " files recovered");
            }

            std::remove(data_path.c_str());
            std::remove((data_path + ".journal").c_str());

        } catch (const std::exception& e) {
            add_test_result("Crash Recovery", false, std::string("Exception: ") + e.what());
        }
    }

    void test_concurrent_access() {
        try {
            std::string data_path = test_data_path_ + "_concurrent.dat";
            std::remove(data_path.c_str());

            auto engine = std::make_unique<razorfs::PersistenceEngine>(
                data_path, razorfs::PersistenceMode::ASYNCHRONOUS);

            const size_t num_threads = 4;
            const size_t operations_per_thread = 10;
            std::vector<std::thread> threads;
            std::atomic<size_t> successful_operations(0);

            for (size_t t = 0; t < num_threads; t++) {
                threads.emplace_back([&, t]() {
                    for (size_t i = 0; i < operations_per_thread; i++) {
                        uint64_t inode = t * 1000 + i;
                        std::string path = "/thread_" + std::to_string(t) + "_file_" + std::to_string(i) + ".txt";
                        std::string content = "Content from thread " + std::to_string(t) + ", operation " + std::to_string(i);

                        if (engine->journal_create_file(inode, path, content)) {
                            successful_operations.fetch_add(1);
                        }
                    }
                });
            }

            for (auto& thread : threads) {
                thread.join();
            }

            // Give async operations time to complete
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));

            size_t expected_operations = num_threads * operations_per_thread;
            bool concurrent_success = (successful_operations.load() == expected_operations);

            add_test_result("Concurrent Access", concurrent_success,
                          std::to_string(successful_operations.load()) + "/" +
                          std::to_string(expected_operations) + " operations");

            std::remove(data_path.c_str());
            std::remove((data_path + ".journal").c_str());

        } catch (const std::exception& e) {
            add_test_result("Concurrent Access", false, std::string("Exception: ") + e.what());
        }
    }

    void test_large_dataset() {
        try {
            std::string data_path = test_data_path_ + "_large.dat";
            std::remove(data_path.c_str());

            auto start_time = std::chrono::high_resolution_clock::now();

            auto engine = std::make_unique<razorfs::PersistenceEngine>(
                data_path, razorfs::PersistenceMode::SYNCHRONOUS);

            const size_t num_files = 1000;
            auto inode_to_name = generate_test_paths(num_files);
            auto file_contents = generate_test_data(num_files, 2048); // Larger files

            bool save_success = engine->save_filesystem(num_files + 100, inode_to_name, file_contents);

            auto save_time = std::chrono::high_resolution_clock::now();

            uint64_t loaded_next_inode;
            std::unordered_map<uint64_t, std::string> loaded_names;
            std::unordered_map<uint64_t, std::string> loaded_contents;

            bool load_success = engine->load_filesystem(loaded_next_inode, loaded_names, loaded_contents);

            auto end_time = std::chrono::high_resolution_clock::now();

            bool data_integrity = save_success && load_success &&
                                (loaded_names.size() == inode_to_name.size()) &&
                                (loaded_contents.size() == file_contents.size());

            auto save_duration = std::chrono::duration_cast<std::chrono::milliseconds>(save_time - start_time);
            auto load_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - save_time);

            add_test_result("Large Dataset", data_integrity,
                          std::to_string(num_files) + " files, save: " +
                          std::to_string(save_duration.count()) + "ms, load: " +
                          std::to_string(load_duration.count()) + "ms");

            // Check file size
            struct stat st;
            if (stat(data_path.c_str(), &st) == 0) {
                double file_size_mb = static_cast<double>(st.st_size) / (1024 * 1024);
                std::cout << "    File size: " << std::fixed << std::setprecision(2)
                         << file_size_mb << " MB" << std::endl;
            }

            std::remove(data_path.c_str());

        } catch (const std::exception& e) {
            add_test_result("Large Dataset", false, std::string("Exception: ") + e.what());
        }
    }

    void test_performance_benchmark() {
        try {
            std::string data_path = test_data_path_ + "_perf.dat";
            std::remove(data_path.c_str());

            const size_t num_iterations = 100;
            const size_t files_per_iteration = 10;

            auto engine = std::make_unique<razorfs::PersistenceEngine>(
                data_path, razorfs::PersistenceMode::SYNCHRONOUS);

            auto start_time = std::chrono::high_resolution_clock::now();

            for (size_t i = 0; i < num_iterations; i++) {
                auto inode_to_name = generate_test_paths(files_per_iteration);
                auto file_contents = generate_test_data(files_per_iteration, 512);

                // Offset inodes to avoid conflicts
                for (auto& pair : inode_to_name) {
                    uint64_t new_inode = pair.first + i * 1000;
                    std::string path = pair.second + "_iter_" + std::to_string(i);
                    inode_to_name[new_inode] = path;
                    inode_to_name.erase(pair.first);
                }

                engine->save_filesystem((i + 1) * 1000, inode_to_name, file_contents);
            }

            auto end_time = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

            double ops_per_second = static_cast<double>(num_iterations * files_per_iteration) /
                                   (static_cast<double>(duration.count()) / 1000.0);

            add_test_result("Performance Benchmark", true,
                          std::to_string(num_iterations) + " iterations, " +
                          std::to_string(static_cast<int>(ops_per_second)) + " ops/sec");

            std::remove(data_path.c_str());

        } catch (const std::exception& e) {
            add_test_result("Performance Benchmark", false, std::string("Exception: ") + e.what());
        }
    }

    void test_corruption_detection() {
        try {
            std::string data_path = test_data_path_ + "_corrupt.dat";
            std::remove(data_path.c_str());

            // Create a valid file
            {
                auto engine = std::make_unique<razorfs::PersistenceEngine>(
                    data_path, razorfs::PersistenceMode::SYNCHRONOUS);

                auto inode_to_name = generate_test_paths(5);
                auto file_contents = generate_test_data(5, 100);
                engine->save_filesystem(105, inode_to_name, file_contents);
            }

            // Corrupt the file
            {
                std::fstream file(data_path, std::ios::binary | std::ios::in | std::ios::out);
                if (file.is_open()) {
                    file.seekp(100); // Seek to some position in the file
                    char corrupt_data[] = {0xFF, 0xFF, 0xFF, 0xFF};
                    file.write(corrupt_data, sizeof(corrupt_data));
                    file.close();
                }
            }

            // Try to load corrupted file
            {
                auto engine = std::make_unique<razorfs::PersistenceEngine>(
                    data_path, razorfs::PersistenceMode::SYNCHRONOUS);

                uint64_t loaded_next_inode;
                std::unordered_map<uint64_t, std::string> loaded_names;
                std::unordered_map<uint64_t, std::string> loaded_contents;

                bool load_success = engine->load_filesystem(loaded_next_inode, loaded_names, loaded_contents);

                // Should fail due to corruption
                add_test_result("Corruption Detection", !load_success, "Corrupted file detected");
            }

            std::remove(data_path.c_str());

        } catch (const std::exception& e) {
            add_test_result("Corruption Detection", false, std::string("Exception: ") + e.what());
        }
    }

    void test_version_compatibility() {
        try {
            // This test would check version handling
            // For now, just verify the version constants are correct
            bool version_constants_valid = (razorfs::RAZORFS_MAGIC == 0x72617A72) &&
                                         (razorfs::RAZORFS_VERSION_MAJOR == 1) &&
                                         (razorfs::RAZORFS_VERSION_MINOR == 0);

            add_test_result("Version Compatibility", version_constants_valid,
                          "Magic: 0x" + std::to_string(razorfs::RAZORFS_MAGIC) +
                          ", Version: " + std::to_string(razorfs::RAZORFS_VERSION_MAJOR) +
                          "." + std::to_string(razorfs::RAZORFS_VERSION_MINOR));

        } catch (const std::exception& e) {
            add_test_result("Version Compatibility", false, std::string("Exception: ") + e.what());
        }
    }
};

int main() {
    try {
        PersistenceTestSuite test_suite;
        test_suite.run_all_tests();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test suite failed: " << e.what() << std::endl;
        return 1;
    }
}