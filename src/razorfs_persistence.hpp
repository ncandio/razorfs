#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <fstream>
#include <atomic>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <condition_variable>
#include <functional>
#include <chrono>
#include <cstring>
#include <iostream>
#include <iomanip>

/**
 * RazorFS Advanced Persistence Engine
 *
 * Features:
 * - Crash-safe atomic writes with journaling
 * - Data integrity with CRC32 checksums
 * - Efficient binary format with string interning
 * - Version compatibility and migration
 * - Background sync and recovery mechanisms
 */

namespace razorfs {

// File format constants
constexpr uint32_t RAZORFS_MAGIC = 0x72617A72;  // "razr"
constexpr uint16_t RAZORFS_VERSION_MAJOR = 1;
constexpr uint16_t RAZORFS_VERSION_MINOR = 0;
constexpr size_t MAX_PATH_LENGTH = 4096;
constexpr size_t MAX_FILE_SIZE = 1024 * 1024;  // 1MB for small files

// Persistence modes
enum class PersistenceMode {
    SYNCHRONOUS,    // Immediate write to disk
    ASYNCHRONOUS,   // Background thread writes
    JOURNAL_ONLY    // Write-ahead logging only
};

// Journal entry types
enum class JournalEntryType : uint8_t {
    CREATE_FILE = 1,
    DELETE_FILE = 2,
    WRITE_DATA = 3,
    CREATE_DIR = 4,
    DELETE_DIR = 5,
    RENAME = 6,
    CHECKPOINT = 7
};

// File format structures
#pragma pack(push, 1)

struct FileHeader {
    uint32_t magic;           // RAZORFS_MAGIC
    uint16_t version_major;   // Format version
    uint16_t version_minor;
    uint32_t header_crc;      // CRC32 of this header
    uint64_t timestamp;       // Creation time
    uint64_t next_inode;      // Next available inode
    uint32_t string_table_offset;
    uint32_t string_table_size;
    uint32_t inode_table_offset;
    uint32_t inode_table_size;
    uint32_t data_section_offset;
    uint32_t data_section_size;
    uint32_t journal_offset;
    uint32_t journal_size;
    uint32_t file_crc;        // CRC32 of entire file
    uint8_t reserved[32];     // Future expansion
};

struct InodeEntry {
    uint64_t inode_number;
    uint64_t parent_inode;
    uint32_t name_offset;     // Offset in string table
    uint16_t mode;            // File type and permissions
    uint16_t flags;
    uint64_t size;
    uint64_t timestamp;
    uint32_t data_offset;     // Offset in data section (0 if directory)
    uint32_t data_size;
    uint32_t crc32;          // CRC32 of this entry
};

struct JournalEntry {
    uint32_t magic;          // Entry validation
    uint8_t type;            // JournalEntryType
    uint8_t reserved[3];
    uint64_t timestamp;
    uint64_t inode;
    uint32_t data_size;
    uint32_t crc32;
    // Variable length data follows
};

#pragma pack(pop)

/**
 * String table for efficient string storage
 */
class StringTable {
private:
    std::vector<char> data_;
    std::unordered_map<std::string, uint32_t> string_to_offset_;
    mutable std::mutex mutex_;

public:
    uint32_t intern_string(const std::string& str) {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = string_to_offset_.find(str);
        if (it != string_to_offset_.end()) {
            return it->second;
        }

        uint32_t offset = static_cast<uint32_t>(data_.size());

        // Store length + string
        uint32_t len = static_cast<uint32_t>(str.length());
        data_.insert(data_.end(),
                    reinterpret_cast<const char*>(&len),
                    reinterpret_cast<const char*>(&len) + sizeof(len));
        data_.insert(data_.end(), str.begin(), str.end());

        string_to_offset_[str] = offset;
        return offset;
    }

    std::string get_string(uint32_t offset) const {
        std::lock_guard<std::mutex> lock(mutex_);

        if (offset >= data_.size()) return "";

        uint32_t len;
        std::memcpy(&len, &data_[offset], sizeof(len));

        if (offset + sizeof(len) + len > data_.size()) return "";

        return std::string(&data_[offset + sizeof(len)], len);
    }

    const std::vector<char>& get_data() const { return data_; }

    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        data_.clear();
        string_to_offset_.clear();
    }

    void load_from_data(const char* data, size_t size) {
        std::lock_guard<std::mutex> lock(mutex_);
        data_.assign(data, data + size);

        // Rebuild string-to-offset map
        string_to_offset_.clear();
        size_t offset = 0;
        while (offset < size) {
            if (offset + sizeof(uint32_t) > size) break;

            uint32_t len;
            std::memcpy(&len, &data_[offset], sizeof(len));

            if (offset + sizeof(len) + len > size) break;

            std::string str(&data_[offset + sizeof(len)], len);
            string_to_offset_[str] = static_cast<uint32_t>(offset);

            offset += sizeof(len) + len;
        }
    }
};

/**
 * CRC32 calculation for data integrity
 */
class CRC32 {
private:
    static std::array<uint32_t, 256> crc_table_;
    static bool table_initialized_;

    static void init_table();

public:
    static uint32_t calculate(const void* data, size_t length);
    static uint32_t calculate(const std::string& str);
};

/**
 * Journal for crash recovery
 */
class Journal {
private:
    std::string journal_path_;
    std::ofstream journal_file_;
    std::mutex journal_mutex_;
    std::atomic<uint64_t> sequence_number_;

public:
    Journal(const std::string& path);
    ~Journal();

    bool open();
    void close();

    bool write_entry(JournalEntryType type, uint64_t inode,
                    const void* data, size_t data_size);

    bool replay_journal(std::function<bool(const JournalEntry&, const void*)> callback);

    bool checkpoint();  // Mark journal as applied
    bool truncate();    // Remove old entries
};

/**
 * Main persistence engine
 */
class PersistenceEngine {
private:
    std::string data_file_path_;
    std::string journal_path_;
    std::unique_ptr<Journal> journal_;
    StringTable string_table_;
    PersistenceMode mode_;

    // Async persistence
    std::atomic<bool> async_thread_running_;
    std::thread async_thread_;
    std::mutex pending_writes_mutex_;
    std::vector<std::function<void()>> pending_writes_;
    std::condition_variable async_cv_;

    mutable std::shared_mutex persistence_mutex_;

public:
    PersistenceEngine(const std::string& data_path,
                     PersistenceMode mode = PersistenceMode::SYNCHRONOUS);
    ~PersistenceEngine();

    // Main persistence operations
    bool save_filesystem(uint64_t next_inode,
                        const std::unordered_map<uint64_t, std::string>& inode_to_name,
                        const std::unordered_map<uint64_t, std::string>& file_contents);

    bool load_filesystem(uint64_t& next_inode,
                        std::unordered_map<uint64_t, std::string>& inode_to_name,
                        std::unordered_map<uint64_t, std::string>& file_contents);

    // Incremental operations (for performance)
    bool journal_create_file(uint64_t inode, const std::string& path,
                           const std::string& content = "");
    bool journal_delete_file(uint64_t inode);
    bool journal_write_data(uint64_t inode, const std::string& content);

    // Recovery and maintenance
    bool recover_from_crash();
    bool verify_integrity();
    bool compact();  // Remove fragmentation

    // Configuration
    void set_mode(PersistenceMode mode);
    PersistenceMode get_mode() const { return mode_; }

    // Statistics
    struct Stats {
        size_t total_files;
        size_t total_size;
        size_t journal_entries;
        double last_save_time_ms;
        double last_load_time_ms;
    };

    Stats get_stats() const;

private:
    void async_worker();
    bool write_file_format(std::ofstream& file,
                          uint64_t next_inode,
                          const std::unordered_map<uint64_t, std::string>& inode_to_name,
                          const std::unordered_map<uint64_t, std::string>& file_contents);

    bool read_file_format(std::ifstream& file,
                         uint64_t& next_inode,
                         std::unordered_map<uint64_t, std::string>& inode_to_name,
                         std::unordered_map<uint64_t, std::string>& file_contents);

    bool validate_header(const FileHeader& header);
    bool atomic_write(const std::string& path, const std::function<bool(std::ofstream&)>& writer);
};

} // namespace razorfs