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
#include <array>

template<typename T> class OptimizedFilesystemNaryTree;

namespace razorfs {

// File format constants
constexpr uint32_t RAZORFS_MAGIC = 0x72617A72;  // "razr"
constexpr uint16_t RAZORFS_VERSION_MAJOR = 1;
constexpr uint16_t RAZORFS_VERSION_MINOR = 0;

enum class PersistenceMode {
    SYNCHRONOUS,
    ASYNCHRONOUS,
    JOURNAL_ONLY
};

enum class JournalEntryType : uint8_t {
    CREATE_FILE = 1,
    DELETE_FILE = 2,
    WRITE_DATA = 3,
    CREATE_DIR = 4,
    DELETE_DIR = 5,
    RENAME = 6,
    CHECKPOINT = 7
};

#pragma pack(push, 1)
struct FileHeader {
    uint32_t magic;
    uint16_t version_major;
    uint16_t version_minor;
    uint32_t header_crc;
    uint64_t timestamp;
    uint64_t next_inode;
    uint32_t string_table_offset;
    uint32_t string_table_size;
    uint32_t inode_table_offset;
    uint32_t inode_table_size;
    uint32_t data_section_offset;
    uint32_t data_section_size;
    uint32_t journal_offset;
    uint32_t journal_size;
    uint32_t file_crc;
    uint8_t reserved[32];
};
struct InodeEntry {
    uint64_t inode_number;
    uint64_t parent_inode;
    uint32_t name_offset;
    uint16_t mode;
    uint16_t flags;
    uint64_t size;
    uint64_t timestamp;
    uint32_t data_offset;
    uint32_t data_size;
    uint32_t crc32;
};
struct JournalEntry {
    uint32_t magic;
    uint8_t type;
    uint8_t reserved[3];
    uint64_t timestamp;
    uint64_t inode;
    uint32_t data_size;
    uint32_t crc32;
};
#pragma pack(pop)

class StringTable {
private:
    std::vector<char> data_;
    std::unordered_map<std::string, uint32_t> string_to_offset_;
    mutable std::mutex mutex_;

    // Constants for validation
    static constexpr size_t MAX_STRING_LENGTH = 4096;  // 4KB max per string
    static constexpr size_t MAX_STRING_TABLE_SIZE = 64 * 1024 * 1024;  // 64MB max total

public:
    uint32_t intern_string(const std::string& str);
    std::string get_string(uint32_t offset) const;
    const std::vector<char>& get_data() const;
    void clear();
    void load_from_data(const char* data, size_t size);
    size_t size() const { return data_.size(); }
};

class CRC32 {
private:
    static std::array<uint32_t, 256> crc_table_;
    static bool table_initialized_;
    static void init_table();
public:
    static uint32_t calculate(const void* data, size_t length);
    static uint32_t calculate(const std::string& str);
};

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
    bool write_entry(JournalEntryType type, uint64_t inode, const void* data, size_t data_size);
    bool replay_journal(std::function<bool(const JournalEntry&, const void*)> callback);
    bool checkpoint();
    bool truncate();
};

class PersistenceEngine {
private:
    std::string data_file_path_;
    std::string journal_path_;
    std::unique_ptr<Journal> journal_;
    StringTable string_table_;
    PersistenceMode mode_;
    std::atomic<bool> async_thread_running_;
    std::thread async_thread_;
    std::mutex pending_writes_mutex_;
    std::vector<std::function<void()>> pending_writes_;
    std::condition_variable async_cv_;
    mutable std::shared_mutex persistence_mutex_;

public:
    PersistenceEngine(const std::string& data_path, PersistenceMode mode = PersistenceMode::SYNCHRONOUS);
    ~PersistenceEngine();

    bool save_filesystem(uint64_t next_inode,
                        OptimizedFilesystemNaryTree<uint64_t>& tree,
                        const std::unordered_map<uint64_t, std::string>& file_contents);

    bool load_filesystem(uint64_t& next_inode,
                        OptimizedFilesystemNaryTree<uint64_t>& tree,
                        std::unordered_map<uint64_t, std::string>& file_contents);

    bool journal_create_file(uint64_t inode, const std::string& path, const std::string& content = "");
    bool journal_delete_file(uint64_t inode);
    bool journal_write_data(uint64_t inode, const std::string& content);
    bool recover_from_crash();
    bool verify_integrity();
    bool compact();
    void set_mode(PersistenceMode mode);

private:
    void async_worker();
    bool write_file_format(std::ofstream& file,
                          uint64_t next_inode,
                          OptimizedFilesystemNaryTree<uint64_t>& tree,
                          const std::unordered_map<uint64_t, std::string>& file_contents);

    bool read_file_format(std::ifstream& file,
                         uint64_t& next_inode,
                         OptimizedFilesystemNaryTree<uint64_t>& tree,
                         std::unordered_map<uint64_t, std::string>& file_contents);

    bool validate_header(const FileHeader& header);
    bool atomic_write(const std::string& path, const std::function<bool(std::ofstream&)>& writer);
};

} // namespace razorfs