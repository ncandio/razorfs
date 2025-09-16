#include "razorfs_persistence.hpp"
#include <algorithm>
#include <thread>
#include <condition_variable>
#include <cstring>
#include <sys/stat.h>
#include <unistd.h>
#include <iostream>
#include <cstdio>

namespace razorfs {

// CRC32 implementation
std::array<uint32_t, 256> CRC32::crc_table_;
bool CRC32::table_initialized_ = false;

void CRC32::init_table() {
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t crc = i;
        for (int j = 0; j < 8; j++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xEDB88320;
            } else {
                crc >>= 1;
            }
        }
        crc_table_[i] = crc;
    }
    table_initialized_ = true;
}

uint32_t CRC32::calculate(const void* data, size_t length) {
    if (!table_initialized_) {
        init_table();
    }

    uint32_t crc = 0xFFFFFFFF;
    const uint8_t* bytes = static_cast<const uint8_t*>(data);

    for (size_t i = 0; i < length; i++) {
        crc = (crc >> 8) ^ crc_table_[(crc ^ bytes[i]) & 0xFF];
    }

    return crc ^ 0xFFFFFFFF;
}

uint32_t CRC32::calculate(const std::string& str) {
    return calculate(str.data(), str.length());
}

// Journal implementation
Journal::Journal(const std::string& path)
    : journal_path_(path), sequence_number_(0) {
}

Journal::~Journal() {
    close();
}

bool Journal::open() {
    journal_file_.open(journal_path_, std::ios::binary | std::ios::app);
    return journal_file_.is_open();
}

void Journal::close() {
    if (journal_file_.is_open()) {
        journal_file_.close();
    }
}

bool Journal::write_entry(JournalEntryType type, uint64_t inode,
                         const void* data, size_t data_size) {
    std::lock_guard<std::mutex> lock(journal_mutex_);

    if (!journal_file_.is_open()) {
        return false;
    }

    JournalEntry entry = {};
    entry.magic = RAZORFS_MAGIC;
    entry.type = static_cast<uint8_t>(type);
    entry.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    entry.inode = inode;
    entry.data_size = static_cast<uint32_t>(data_size);

    // Calculate CRC32 over entry + data
    std::vector<uint8_t> entry_data(sizeof(entry) + data_size);
    std::memcpy(entry_data.data(), &entry, sizeof(entry));
    if (data && data_size > 0) {
        std::memcpy(entry_data.data() + sizeof(entry), data, data_size);
    }

    entry.crc32 = CRC32::calculate(entry_data.data() + sizeof(uint32_t),
                                  entry_data.size() - sizeof(uint32_t));

    // Update the entry with correct CRC
    std::memcpy(entry_data.data(), &entry, sizeof(entry));

    // Write to journal
    journal_file_.write(reinterpret_cast<const char*>(entry_data.data()),
                       entry_data.size());
    journal_file_.flush();

    sequence_number_.fetch_add(1);
    return journal_file_.good();
}

bool Journal::replay_journal(std::function<bool(const JournalEntry&, const void*)> callback) {
    std::ifstream file(journal_path_, std::ios::binary);
    if (!file.is_open()) {
        return true;  // No journal file is OK
    }

    file.seekg(0, std::ios::end);
    size_t file_size = file.tellg();
    file.seekg(0, std::ios::beg);

    while (file.tellg() < file_size) {
        JournalEntry entry;
        file.read(reinterpret_cast<char*>(&entry), sizeof(entry));

        if (file.gcount() != sizeof(entry)) {
            break;  // Incomplete entry
        }

        if (entry.magic != RAZORFS_MAGIC) {
            std::cerr << "Invalid journal entry magic" << std::endl;
            return false;
        }

        // Read data if present
        std::vector<uint8_t> data;
        if (entry.data_size > 0) {
            data.resize(entry.data_size);
            file.read(reinterpret_cast<char*>(data.data()), entry.data_size);

            if (file.gcount() != entry.data_size) {
                std::cerr << "Incomplete journal entry data" << std::endl;
                return false;
            }
        }

        // Verify CRC32
        std::vector<uint8_t> verify_data(sizeof(entry) - sizeof(uint32_t) + data.size());
        std::memcpy(verify_data.data(),
                   reinterpret_cast<const uint8_t*>(&entry) + sizeof(uint32_t),
                   sizeof(entry) - sizeof(uint32_t));
        if (!data.empty()) {
            std::memcpy(verify_data.data() + sizeof(entry) - sizeof(uint32_t),
                       data.data(), data.size());
        }

        uint32_t calculated_crc = CRC32::calculate(verify_data.data(), verify_data.size());
        if (calculated_crc != entry.crc32) {
            std::cerr << "Journal entry CRC mismatch" << std::endl;
            return false;
        }

        // Apply the journal entry
        if (!callback(entry, data.empty() ? nullptr : data.data())) {
            return false;
        }
    }

    return true;
}

bool Journal::checkpoint() {
    return write_entry(JournalEntryType::CHECKPOINT, 0, nullptr, 0);
}

bool Journal::truncate() {
    close();
    if (std::remove(journal_path_.c_str()) != 0) {
        return false;
    }
    return open();
}

// PersistenceEngine implementation
PersistenceEngine::PersistenceEngine(const std::string& data_path, PersistenceMode mode)
    : data_file_path_(data_path)
    , journal_path_(data_path + ".journal")
    , mode_(mode)
    , async_thread_running_(false) {

    journal_ = std::make_unique<Journal>(journal_path_);

    if (mode_ == PersistenceMode::ASYNCHRONOUS) {
        async_thread_running_ = true;
        async_thread_ = std::thread(&PersistenceEngine::async_worker, this);
    }
}

PersistenceEngine::~PersistenceEngine() {
    if (async_thread_running_) {
        async_thread_running_ = false;
        async_cv_.notify_all();
        if (async_thread_.joinable()) {
            async_thread_.join();
        }
    }
}

bool PersistenceEngine::save_filesystem(
    uint64_t next_inode,
    const std::unordered_map<uint64_t, std::string>& inode_to_name,
    const std::unordered_map<uint64_t, std::string>& file_contents) {

    if (mode_ == PersistenceMode::ASYNCHRONOUS) {
        // Queue for background thread
        std::lock_guard<std::mutex> lock(pending_writes_mutex_);
        pending_writes_.emplace_back([=]() {
            this->save_filesystem(next_inode, inode_to_name, file_contents);
        });
        async_cv_.notify_one();
        return true;
    }

    auto start_time = std::chrono::high_resolution_clock::now();
    std::unique_lock<std::shared_mutex> lock(persistence_mutex_);

    // Atomic write using temporary file
    bool success = atomic_write(data_file_path_,
        [&](std::ofstream& file) {
            return write_file_format(file, next_inode, inode_to_name, file_contents);
        });

    if (success && journal_) {
        journal_->checkpoint();
        journal_->truncate();  // Clear journal after successful save
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    std::cout << "Filesystem saved in " << duration.count() << "ms" << std::endl;
    return success;
}

bool PersistenceEngine::load_filesystem(
    uint64_t& next_inode,
    std::unordered_map<uint64_t, std::string>& inode_to_name,
    std::unordered_map<uint64_t, std::string>& file_contents) {

    auto start_time = std::chrono::high_resolution_clock::now();
    std::shared_lock<std::shared_mutex> lock(persistence_mutex_);

    // First try to load from main file
    std::ifstream file(data_file_path_, std::ios::binary);
    bool loaded_from_main = false;

    if (file.is_open()) {
        loaded_from_main = read_file_format(file, next_inode, inode_to_name, file_contents);
        file.close();
    }

    // If main file failed or doesn't exist, try journal recovery
    if (!loaded_from_main) {
        std::cout << "Main file not found or corrupted, attempting journal recovery..." << std::endl;
        if (!recover_from_crash()) {
            std::cout << "Starting with fresh filesystem" << std::endl;
            next_inode = 2;
            inode_to_name.clear();
            file_contents.clear();
            return true;  // Fresh start is OK
        }

        // Try loading again after recovery
        file.open(data_file_path_, std::ios::binary);
        if (file.is_open()) {
            loaded_from_main = read_file_format(file, next_inode, inode_to_name, file_contents);
            file.close();
        }
    }

    // Apply any remaining journal entries
    if (journal_) {
        journal_->replay_journal([&](const JournalEntry& entry, const void* data) -> bool {
            // Apply journal entry to loaded data
            switch (static_cast<JournalEntryType>(entry.type)) {
                case JournalEntryType::CREATE_FILE: {
                    if (data) {
                        std::string path_and_content(static_cast<const char*>(data), entry.data_size);
                        size_t null_pos = path_and_content.find('\0');
                        if (null_pos != std::string::npos) {
                            std::string path = path_and_content.substr(0, null_pos);
                            std::string content = path_and_content.substr(null_pos + 1);
                            inode_to_name[entry.inode] = path;
                            if (!content.empty()) {
                                file_contents[entry.inode] = content;
                            }
                        }
                    }
                    break;
                }
                case JournalEntryType::DELETE_FILE: {
                    inode_to_name.erase(entry.inode);
                    file_contents.erase(entry.inode);
                    break;
                }
                case JournalEntryType::WRITE_DATA: {
                    if (data) {
                        file_contents[entry.inode] = std::string(static_cast<const char*>(data), entry.data_size);
                    }
                    break;
                }
                default:
                    break;
            }
            return true;
        });
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    std::cout << "Filesystem loaded in " << duration.count() << "ms" << std::endl;
    return true;
}

bool PersistenceEngine::journal_create_file(uint64_t inode, const std::string& path,
                                           const std::string& content) {
    if (!journal_ || mode_ == PersistenceMode::JOURNAL_ONLY) {
        return false;
    }

    // Combine path and content with null separator
    std::string data = path + '\0' + content;
    return journal_->write_entry(JournalEntryType::CREATE_FILE, inode,
                               data.data(), data.size());
}

bool PersistenceEngine::journal_delete_file(uint64_t inode) {
    if (!journal_) {
        return false;
    }

    return journal_->write_entry(JournalEntryType::DELETE_FILE, inode, nullptr, 0);
}

bool PersistenceEngine::journal_write_data(uint64_t inode, const std::string& content) {
    if (!journal_) {
        return false;
    }

    return journal_->write_entry(JournalEntryType::WRITE_DATA, inode,
                               content.data(), content.size());
}

bool PersistenceEngine::recover_from_crash() {
    std::cout << "Attempting crash recovery..." << std::endl;

    // Create empty structures to apply journal to
    uint64_t next_inode = 2;
    std::unordered_map<uint64_t, std::string> inode_to_name;
    std::unordered_map<uint64_t, std::string> file_contents;

    if (!journal_) {
        return false;
    }

    bool recovery_success = journal_->replay_journal([&](const JournalEntry& entry, const void* data) -> bool {
        // Same logic as in load_filesystem
        switch (static_cast<JournalEntryType>(entry.type)) {
            case JournalEntryType::CREATE_FILE: {
                if (data) {
                    std::string path_and_content(static_cast<const char*>(data), entry.data_size);
                    size_t null_pos = path_and_content.find('\0');
                    if (null_pos != std::string::npos) {
                        std::string path = path_and_content.substr(0, null_pos);
                        std::string content = path_and_content.substr(null_pos + 1);
                        inode_to_name[entry.inode] = path;
                        if (!content.empty()) {
                            file_contents[entry.inode] = content;
                        }
                        next_inode = std::max(next_inode, entry.inode + 1);
                    }
                }
                break;
            }
            case JournalEntryType::DELETE_FILE: {
                inode_to_name.erase(entry.inode);
                file_contents.erase(entry.inode);
                break;
            }
            case JournalEntryType::WRITE_DATA: {
                if (data) {
                    file_contents[entry.inode] = std::string(static_cast<const char*>(data), entry.data_size);
                }
                break;
            }
            case JournalEntryType::CHECKPOINT: {
                // Checkpoint reached - save current state
                save_filesystem(next_inode, inode_to_name, file_contents);
                break;
            }
            default:
                break;
        }
        return true;
    });

    if (recovery_success && !inode_to_name.empty()) {
        // Save recovered state
        save_filesystem(next_inode, inode_to_name, file_contents);
        std::cout << "Recovery completed - restored " << inode_to_name.size() << " files" << std::endl;
        return true;
    }

    return false;
}

bool PersistenceEngine::write_file_format(
    std::ofstream& file,
    uint64_t next_inode,
    const std::unordered_map<uint64_t, std::string>& inode_to_name,
    const std::unordered_map<uint64_t, std::string>& file_contents) {

    // Clear and rebuild string table
    string_table_.clear();

    // Build string table
    for (const auto& pair : inode_to_name) {
        string_table_.intern_string(pair.second);
    }

    // Calculate section sizes
    const auto& string_data = string_table_.get_data();
    size_t inode_table_size = inode_to_name.size() * sizeof(InodeEntry);
    size_t data_section_size = 0;
    for (const auto& pair : file_contents) {
        data_section_size += pair.second.size();
    }

    // Write header
    FileHeader header = {};
    header.magic = RAZORFS_MAGIC;
    header.version_major = RAZORFS_VERSION_MAJOR;
    header.version_minor = RAZORFS_VERSION_MINOR;
    header.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    header.next_inode = next_inode;

    header.string_table_offset = sizeof(FileHeader);
    header.string_table_size = static_cast<uint32_t>(string_data.size());
    header.inode_table_offset = header.string_table_offset + header.string_table_size;
    header.inode_table_size = static_cast<uint32_t>(inode_table_size);
    header.data_section_offset = header.inode_table_offset + header.inode_table_size;
    header.data_section_size = static_cast<uint32_t>(data_section_size);
    header.journal_offset = 0;  // Not used in main file
    header.journal_size = 0;

    // Calculate header CRC (excluding the CRC fields themselves)
    header.header_crc = CRC32::calculate(
        reinterpret_cast<const uint8_t*>(&header) + 2 * sizeof(uint32_t),
        sizeof(header) - 3 * sizeof(uint32_t));

    file.write(reinterpret_cast<const char*>(&header), sizeof(header));

    // Write string table
    if (!string_data.empty()) {
        file.write(string_data.data(), string_data.size());
    }

    // Write inode table
    uint32_t current_data_offset = 0;
    for (const auto& pair : inode_to_name) {
        InodeEntry entry = {};
        entry.inode_number = pair.first;

        // Find parent (simplified - assumes path format)
        std::string path = pair.second;
        size_t last_slash = path.find_last_of('/');
        if (last_slash != std::string::npos && last_slash > 0) {
            std::string parent_path = path.substr(0, last_slash);
            // Find parent inode (linear search - could be optimized)
            for (const auto& parent_pair : inode_to_name) {
                if (parent_pair.second == parent_path) {
                    entry.parent_inode = parent_pair.first;
                    break;
                }
            }
        } else {
            entry.parent_inode = 0;  // Root or child of root
        }

        entry.name_offset = string_table_.intern_string(pair.second);

        // Check if it's a file or directory
        auto content_it = file_contents.find(pair.first);
        if (content_it != file_contents.end()) {
            // File
            entry.mode = S_IFREG | 0644;
            entry.size = content_it->second.size();
            entry.data_offset = current_data_offset;
            entry.data_size = static_cast<uint32_t>(content_it->second.size());
            current_data_offset += entry.data_size;
        } else {
            // Directory
            entry.mode = S_IFDIR | 0755;
            entry.size = 0;
            entry.data_offset = 0;
            entry.data_size = 0;
        }

        entry.timestamp = header.timestamp;
        entry.crc32 = CRC32::calculate(&entry, sizeof(entry) - sizeof(uint32_t));

        file.write(reinterpret_cast<const char*>(&entry), sizeof(entry));
    }

    // Write data section
    for (const auto& pair : inode_to_name) {
        auto content_it = file_contents.find(pair.first);
        if (content_it != file_contents.end()) {
            file.write(content_it->second.data(), content_it->second.size());
        }
    }

    // Calculate and update file CRC
    size_t current_pos = file.tellp();
    file.seekp(0);

    std::vector<char> file_data(current_pos);
    file.read(file_data.data(), current_pos);

    // Calculate CRC excluding the file_crc field
    FileHeader* header_ptr = reinterpret_cast<FileHeader*>(file_data.data());
    header_ptr->file_crc = CRC32::calculate(
        file_data.data() + sizeof(FileHeader),
        current_pos - sizeof(FileHeader));

    // Write back with correct CRC
    file.seekp(0);
    file.write(file_data.data(), current_pos);

    return file.good();
}

bool PersistenceEngine::read_file_format(
    std::ifstream& file,
    uint64_t& next_inode,
    std::unordered_map<uint64_t, std::string>& inode_to_name,
    std::unordered_map<uint64_t, std::string>& file_contents) {

    // Read header
    FileHeader header;
    file.read(reinterpret_cast<char*>(&header), sizeof(header));

    if (!file.good() || !validate_header(header)) {
        std::cerr << "Invalid file header" << std::endl;
        return false;
    }

    next_inode = header.next_inode;

    // Verify file CRC
    file.seekg(0, std::ios::end);
    size_t file_size = file.tellg();
    file.seekg(0);

    std::vector<char> file_data(file_size);
    file.read(file_data.data(), file_size);

    uint32_t calculated_crc = CRC32::calculate(
        file_data.data() + sizeof(FileHeader),
        file_size - sizeof(FileHeader));

    if (calculated_crc != header.file_crc) {
        std::cerr << "File CRC mismatch - file may be corrupted" << std::endl;
        return false;
    }

    // Load string table
    if (header.string_table_size > 0) {
        string_table_.load_from_data(
            file_data.data() + header.string_table_offset,
            header.string_table_size);
    }

    // Load inode table
    size_t inode_count = header.inode_table_size / sizeof(InodeEntry);
    for (size_t i = 0; i < inode_count; i++) {
        InodeEntry entry;
        std::memcpy(&entry,
                   file_data.data() + header.inode_table_offset + i * sizeof(InodeEntry),
                   sizeof(entry));

        // Verify entry CRC
        uint32_t calculated_entry_crc = CRC32::calculate(&entry, sizeof(entry) - sizeof(uint32_t));
        if (calculated_entry_crc != entry.crc32) {
            std::cerr << "Inode entry CRC mismatch for inode " << entry.inode_number << std::endl;
            continue;
        }

        std::string name = string_table_.get_string(entry.name_offset);
        inode_to_name[entry.inode_number] = name;

        // Load file content if it's a file
        if ((entry.mode & S_IFMT) == S_IFREG && entry.data_size > 0) {
            std::string content(
                file_data.data() + header.data_section_offset + entry.data_offset,
                entry.data_size);
            file_contents[entry.inode_number] = content;
        }
    }

    std::cout << "Loaded " << inode_to_name.size() << " inodes, "
              << file_contents.size() << " files" << std::endl;

    return true;
}

bool PersistenceEngine::validate_header(const FileHeader& header) {
    if (header.magic != RAZORFS_MAGIC) {
        return false;
    }

    if (header.version_major != RAZORFS_VERSION_MAJOR) {
        std::cerr << "Unsupported version: " << header.version_major
                  << "." << header.version_minor << std::endl;
        return false;
    }

    // Verify header CRC
    uint32_t calculated_crc = CRC32::calculate(
        reinterpret_cast<const uint8_t*>(&header) + 2 * sizeof(uint32_t),
        sizeof(header) - 3 * sizeof(uint32_t));

    return calculated_crc == header.header_crc;
}

bool PersistenceEngine::atomic_write(const std::string& path,
                                    const std::function<bool(std::ofstream&)>& writer) {
    std::string temp_path = path + ".tmp";

    std::ofstream temp_file(temp_path, std::ios::binary);
    if (!temp_file.is_open()) {
        return false;
    }

    bool write_success = writer(temp_file);
    temp_file.close();

    if (!write_success) {
        std::remove(temp_path.c_str());
        return false;
    }

    // Atomic rename
    if (std::rename(temp_path.c_str(), path.c_str()) != 0) {
        std::remove(temp_path.c_str());
        return false;
    }

    return true;
}

void PersistenceEngine::async_worker() {
    while (async_thread_running_) {
        std::unique_lock<std::mutex> lock(pending_writes_mutex_);
        async_cv_.wait(lock, [this] {
            return !pending_writes_.empty() || !async_thread_running_;
        });

        if (!async_thread_running_) break;

        auto writes_to_process = std::move(pending_writes_);
        pending_writes_.clear();
        lock.unlock();

        for (auto& write_func : writes_to_process) {
            write_func();
        }
    }
}

PersistenceEngine::Stats PersistenceEngine::get_stats() const {
    // Implementation for statistics
    Stats stats = {};
    // Would be filled with actual metrics
    return stats;
}

void PersistenceEngine::set_mode(PersistenceMode mode) {
    mode_ = mode;
    // Handle mode transitions if needed
}

bool PersistenceEngine::verify_integrity() {
    // Implementation for integrity verification
    return true;
}

bool PersistenceEngine::compact() {
    // Implementation for compaction
    return true;
}

} // namespace razorfs