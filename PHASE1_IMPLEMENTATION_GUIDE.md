# Phase 1 Implementation Guide

**Goal**: Fix critical bugs and establish foundation for production
**Timeline**: 4-6 weeks
**Status**: Ready to start

---

## Week 1: Fix Persistence Data-Loss Bug

### Day 1-2: Create Test Infrastructure

**Create**: `tests/test_persistence_roundtrip.cpp`

```cpp
#include <gtest/gtest.h>
#include "../src/razorfs_persistence.hpp"
#include "../src/cache_optimized_filesystem.hpp"

class PersistenceRoundtripTest : public ::testing::Test {
protected:
    void SetUp() override {
        temp_dir = "/tmp/razorfs_test_" + std::to_string(getpid());
        fs::create_directory(temp_dir);
    }

    void TearDown() override {
        fs::remove_all(temp_dir);
    }

    std::string temp_dir;
};

TEST_F(PersistenceRoundtripTest, SimpleRoundtrip) {
    using namespace razor_cache_optimized;

    // Create filesystem with test data
    auto fs1 = std::make_unique<OptimizedFilesystemNaryTree<FileMetadata>>();

    // Create test structure
    fs1->create_directory("/", "dir1");
    fs1->create_file("/dir1", "file1.txt", "Hello World");
    fs1->create_file("/dir1", "file2.txt", "Test Data");

    // Save to disk
    razorfs::PersistenceEngine persistence(temp_dir + "/fs.dat");
    ASSERT_TRUE(persistence.save_filesystem(*fs1));

    // Destroy and reload
    fs1.reset();

    auto fs2 = std::make_unique<OptimizedFilesystemNaryTree<FileMetadata>>();
    ASSERT_TRUE(persistence.load_filesystem(*fs2));

    // Verify data integrity
    auto* file1 = fs2->find_by_path("/dir1/file1.txt");
    ASSERT_NE(file1, nullptr);
    EXPECT_EQ(file1->data, "Hello World");

    auto* file2 = fs2->find_by_path("/dir1/file2.txt");
    ASSERT_NE(file2, nullptr);
    EXPECT_EQ(file2->data, "Test Data");
}

TEST_F(PersistenceRoundtripTest, LargeFilesystem) {
    // Test with 10K files
    auto fs = std::make_unique<OptimizedFilesystemNaryTree<FileMetadata>>();

    // Create 100 directories with 100 files each
    for (int d = 0; d < 100; d++) {
        std::string dir_name = "/dir_" + std::to_string(d);
        fs->create_directory("/", "dir_" + std::to_string(d));

        for (int f = 0; f < 100; f++) {
            std::string file_name = "file_" + std::to_string(f) + ".txt";
            std::string content = "Content for " + dir_name + "/" + file_name;
            fs->create_file(dir_name, file_name, content);
        }
    }

    // Save and reload
    razorfs::PersistenceEngine persistence(temp_dir + "/large_fs.dat");
    ASSERT_TRUE(persistence.save_filesystem(*fs));

    auto fs2 = std::make_unique<OptimizedFilesystemNaryTree<FileMetadata>>();
    ASSERT_TRUE(persistence.load_filesystem(*fs2));

    // Verify random files
    for (int i = 0; i < 100; i++) {
        int d = rand() % 100;
        int f = rand() % 100;
        std::string path = "/dir_" + std::to_string(d) + "/file_" + std::to_string(f) + ".txt";
        auto* file = fs2->find_by_path(path);
        ASSERT_NE(file, nullptr) << "Missing file: " << path;
    }
}

TEST_F(PersistenceRoundtripTest, DeepHierarchy) {
    // Test with 20-level deep directory structure
    auto fs = std::make_unique<OptimizedFilesystemNaryTree<FileMetadata>>();

    std::string current_path = "";
    for (int i = 0; i < 20; i++) {
        std::string dir_name = "level_" + std::to_string(i);
        fs->create_directory(current_path.empty() ? "/" : current_path, dir_name);
        current_path += "/" + dir_name;
    }

    // Create file at deepest level
    fs->create_file(current_path, "deep_file.txt", "Deep content");

    // Save and reload
    razorfs::PersistenceEngine persistence(temp_dir + "/deep_fs.dat");
    ASSERT_TRUE(persistence.save_filesystem(*fs));

    auto fs2 = std::make_unique<OptimizedFilesystemNaryTree<FileMetadata>>();
    ASSERT_TRUE(persistence.load_filesystem(*fs2));

    // Verify deep file
    auto* file = fs2->find_by_path(current_path + "/deep_file.txt");
    ASSERT_NE(file, nullptr);
    EXPECT_EQ(file->data, "Deep content");
}
```

**Run tests**:
```bash
mkdir -p build && cd build
cmake .. -DBUILD_TESTS=ON
make test_persistence_roundtrip
./test_persistence_roundtrip
```

**Expected**: All tests should FAIL initially (documenting the bug)

---

### Day 3-4: Debug and Fix Persistence Bug

**Likely Bug Locations**:

1. **String Table Offset Issue** (`src/razorfs_persistence.cpp`)

```cpp
// BEFORE (buggy):
std::string StringTable::get_string(unsigned int offset) const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (offset >= data_.size()) {
        return "";  // BUG: Silent failure
    }
    return std::string(&data_[offset]);
}

// AFTER (fixed):
std::string StringTable::get_string(unsigned int offset) const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (offset >= data_.size()) {
        throw std::runtime_error("Invalid string offset: " + std::to_string(offset));
    }

    // Ensure null-terminated
    const char* str_start = &data_[offset];
    const char* data_end = data_.data() + data_.size();

    // Find null terminator
    const char* null_pos = std::find(str_start, data_end, '\0');
    if (null_pos == data_end) {
        throw std::runtime_error("String not null-terminated at offset: " + std::to_string(offset));
    }

    return std::string(str_start);
}
```

2. **String Table Reload** (`src/razorfs_persistence.cpp`)

```cpp
// Add validation after loading string table:
void StringTable::load_from_data(const char* data, size_t size) {
    data_.clear();
    string_to_offset_.clear();

    data_.insert(data_.end(), data, data + size);

    // Rebuild string-to-offset map
    size_t offset = 0;
    while (offset < data_.size()) {
        if (data_[offset] == '\0') {
            offset++;
            continue;
        }

        std::string str(&data_[offset]);
        string_to_offset_[str] = offset;
        offset += str.length() + 1;  // +1 for null terminator
    }

    // Validate: ensure last byte is null
    if (!data_.empty() && data_.back() != '\0') {
        throw std::runtime_error("String table not properly null-terminated");
    }
}
```

3. **Inode Map Reconstruction** (`src/razorfs_persistence.cpp`)

```cpp
// In load_filesystem():
bool PersistenceEngine::load_filesystem(auto& filesystem) {
    // ... load inodes ...

    // CRITICAL: Rebuild parent-child relationships
    for (const auto& [inode_num, node] : loaded_nodes) {
        if (node->parent_inode != 0) {
            auto parent = loaded_nodes.find(node->parent_inode);
            if (parent == loaded_nodes.end()) {
                // BUG: Orphaned node
                std::cerr << "ERROR: Node " << inode_num
                         << " has invalid parent " << node->parent_inode << std::endl;
                return false;
            }

            // Add child to parent
            std::string child_name = string_table_.get_string(node->name_offset);
            parent->second->add_child(node, child_name);
        }
    }

    return true;
}
```

4. **Add Checksums** (`src/razorfs_persistence.cpp`)

```cpp
// Compute CRC32 for each section
uint32_t compute_section_crc(const void* data, size_t size) {
    // Use existing CRC32 class
    return CRC32::calculate(data, size);
}

// Before writing:
header.string_table_crc = compute_section_crc(string_table.data(), string_table.size());
header.inode_table_crc = compute_section_crc(inode_data, inode_data_size);
header.data_section_crc = compute_section_crc(data_section, data_section_size);

// After loading:
if (compute_section_crc(loaded_string_table, size) != header.string_table_crc) {
    throw std::runtime_error("String table corruption detected (CRC mismatch)");
}
```

---

### Day 5: Verify Fix

**Run tests again**:
```bash
./test_persistence_roundtrip
```

**Expected**: All tests PASS

**Additional validation**:
```bash
# Stress test: 1000 save/load cycles
./scripts/stress_test_persistence.sh
```

---

## Week 2-4: Implement Production Journaling

### Week 2: Design and Basic Infrastructure

**Create**: `src/razorfs_journal_v2.hpp`

```cpp
#pragma once

#include <string>
#include <fstream>
#include <mutex>
#include <vector>
#include <unordered_map>
#include <functional>

namespace razorfs {

enum class JournalOpType : uint8_t {
    BEGIN_TXN = 1,
    CREATE_FILE = 2,
    DELETE_FILE = 3,
    WRITE_DATA = 4,
    CREATE_DIR = 5,
    DELETE_DIR = 6,
    RENAME = 7,
    COMMIT_TXN = 8,
    ABORT_TXN = 9
};

#pragma pack(push, 1)
struct JournalEntryHeader {
    uint32_t magic;              // 0x4A524E4C "JRNL"
    uint64_t txn_id;
    uint64_t sequence;
    JournalOpType op_type;
    uint64_t timestamp;          // microseconds since epoch
    uint64_t inode;
    uint32_t undo_size;
    uint32_t redo_size;
    uint32_t entry_crc;
    uint8_t committed;           // 0 = pending, 1 = committed, 2 = aborted
};
#pragma pack(pop)

class ProductionJournal {
public:
    ProductionJournal(const std::string& journal_path);
    ~ProductionJournal();

    // Transaction management
    uint64_t begin_transaction();
    bool write_entry(uint64_t txn_id, JournalOpType op_type,
                     uint64_t inode,
                     const void* undo_data, size_t undo_size,
                     const void* redo_data, size_t redo_size);
    bool commit_transaction(uint64_t txn_id);
    bool abort_transaction(uint64_t txn_id);

    // Recovery
    bool replay_journal(std::function<bool(const JournalEntryHeader&,
                                           const void* undo_data,
                                           const void* redo_data)> callback);
    bool verify_integrity();

    // Maintenance
    bool checkpoint();  // Apply committed transactions
    bool compact();     // Remove old entries

private:
    std::string journal_path_;
    std::fstream journal_file_;
    std::mutex journal_mutex_;

    uint64_t next_txn_id_;
    uint64_t sequence_number_;

    std::unordered_map<uint64_t, size_t> txn_start_offsets_;

    bool write_header(const JournalEntryHeader& header);
    bool update_commit_status(uint64_t txn_id, uint8_t status);
    uint32_t compute_entry_crc(const JournalEntryHeader& header,
                               const void* undo_data,
                               const void* redo_data);
};

} // namespace razorfs
```

**Create**: `src/razorfs_journal_v2.cpp`

```cpp
#include "razorfs_journal_v2.hpp"
#include <chrono>
#include <unistd.h>
#include <iostream>

namespace razorfs {

constexpr uint32_t JOURNAL_MAGIC = 0x4A524E4C;  // "JRNL"

ProductionJournal::ProductionJournal(const std::string& journal_path)
    : journal_path_(journal_path)
    , next_txn_id_(1)
    , sequence_number_(0)
{
    // Open journal file for append
    journal_file_.open(journal_path_,
                       std::ios::in | std::ios::out | std::ios::binary | std::ios::app);

    if (!journal_file_.is_open()) {
        // Create new journal
        journal_file_.open(journal_path_,
                          std::ios::out | std::ios::binary);
        journal_file_.close();
        journal_file_.open(journal_path_,
                          std::ios::in | std::ios::out | std::ios::binary);
    }

    // Recover next_txn_id from existing journal
    if (journal_file_.is_open()) {
        // Scan journal to find highest txn_id
        journal_file_.seekg(0, std::ios::end);
        size_t file_size = journal_file_.tellg();
        journal_file_.seekg(0, std::ios::beg);

        uint64_t max_txn_id = 0;
        while (journal_file_.tellg() < file_size) {
            JournalEntryHeader header;
            journal_file_.read(reinterpret_cast<char*>(&header), sizeof(header));

            if (header.magic == JOURNAL_MAGIC) {
                max_txn_id = std::max(max_txn_id, header.txn_id);
                sequence_number_ = std::max(sequence_number_, header.sequence);

                // Skip undo/redo data
                journal_file_.seekg(header.undo_size + header.redo_size, std::ios::cur);
            }
        }

        next_txn_id_ = max_txn_id + 1;
        sequence_number_++;
    }
}

ProductionJournal::~ProductionJournal() {
    if (journal_file_.is_open()) {
        journal_file_.close();
    }
}

uint64_t ProductionJournal::begin_transaction() {
    std::lock_guard<std::mutex> lock(journal_mutex_);

    uint64_t txn_id = next_txn_id_++;

    JournalEntryHeader header = {};
    header.magic = JOURNAL_MAGIC;
    header.txn_id = txn_id;
    header.sequence = sequence_number_++;
    header.op_type = JournalOpType::BEGIN_TXN;
    header.timestamp = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    header.committed = 0;

    // Record start offset for this transaction
    txn_start_offsets_[txn_id] = journal_file_.tellp();

    write_header(header);

    // CRITICAL: fsync to ensure BEGIN_TXN is on disk
    journal_file_.flush();
    fsync(fileno(fdopen(dup(fileno(journal_file_.rdbuf()->__file())), "r+")));

    return txn_id;
}

bool ProductionJournal::write_entry(uint64_t txn_id, JournalOpType op_type,
                                    uint64_t inode,
                                    const void* undo_data, size_t undo_size,
                                    const void* redo_data, size_t redo_size) {
    std::lock_guard<std::mutex> lock(journal_mutex_);

    JournalEntryHeader header = {};
    header.magic = JOURNAL_MAGIC;
    header.txn_id = txn_id;
    header.sequence = sequence_number_++;
    header.op_type = op_type;
    header.timestamp = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    header.inode = inode;
    header.undo_size = undo_size;
    header.redo_size = redo_size;
    header.committed = 0;

    // Compute CRC
    header.entry_crc = compute_entry_crc(header, undo_data, redo_data);

    // Write header
    if (!write_header(header)) return false;

    // Write undo data
    if (undo_size > 0) {
        journal_file_.write(reinterpret_cast<const char*>(undo_data), undo_size);
    }

    // Write redo data
    if (redo_size > 0) {
        journal_file_.write(reinterpret_cast<const char*>(redo_data), redo_size);
    }

    // CRITICAL: fsync to ensure entry is on disk before applying changes
    journal_file_.flush();
    fsync(fileno(fdopen(dup(fileno(journal_file_.rdbuf()->__file())), "r+")));

    return true;
}

bool ProductionJournal::commit_transaction(uint64_t txn_id) {
    std::lock_guard<std::mutex> lock(journal_mutex_);

    JournalEntryHeader header = {};
    header.magic = JOURNAL_MAGIC;
    header.txn_id = txn_id;
    header.sequence = sequence_number_++;
    header.op_type = JournalOpType::COMMIT_TXN;
    header.timestamp = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    header.committed = 1;

    write_header(header);

    // CRITICAL: fsync to ensure commit is on disk
    journal_file_.flush();
    fsync(fileno(fdopen(dup(fileno(journal_file_.rdbuf()->__file())), "r+")));

    // Remove from pending transactions
    txn_start_offsets_.erase(txn_id);

    return true;
}

bool ProductionJournal::replay_journal(
    std::function<bool(const JournalEntryHeader&,
                       const void* undo_data,
                       const void* redo_data)> callback) {

    std::lock_guard<std::mutex> lock(journal_mutex_);

    journal_file_.seekg(0, std::ios::end);
    size_t file_size = journal_file_.tellg();
    journal_file_.seekg(0, std::ios::beg);

    std::unordered_map<uint64_t, bool> txn_committed;
    std::vector<JournalEntryHeader> entries;
    std::vector<std::vector<uint8_t>> undo_data_list;
    std::vector<std::vector<uint8_t>> redo_data_list;

    // First pass: determine which transactions are committed
    while (journal_file_.tellg() < file_size) {
        JournalEntryHeader header;
        journal_file_.read(reinterpret_cast<char*>(&header), sizeof(header));

        if (header.magic != JOURNAL_MAGIC) {
            std::cerr << "Invalid journal entry magic" << std::endl;
            return false;
        }

        if (header.op_type == JournalOpType::COMMIT_TXN) {
            txn_committed[header.txn_id] = true;
        } else if (header.op_type == JournalOpType::ABORT_TXN) {
            txn_committed[header.txn_id] = false;
        } else {
            // Read undo/redo data
            std::vector<uint8_t> undo_data(header.undo_size);
            std::vector<uint8_t> redo_data(header.redo_size);

            if (header.undo_size > 0) {
                journal_file_.read(reinterpret_cast<char*>(undo_data.data()),
                                  header.undo_size);
            }
            if (header.redo_size > 0) {
                journal_file_.read(reinterpret_cast<char*>(redo_data.data()),
                                  header.redo_size);
            }

            // Verify CRC
            uint32_t computed_crc = compute_entry_crc(header,
                                                      undo_data.data(),
                                                      redo_data.data());
            if (computed_crc != header.entry_crc) {
                std::cerr << "CRC mismatch for entry " << header.sequence << std::endl;
                return false;
            }

            entries.push_back(header);
            undo_data_list.push_back(std::move(undo_data));
            redo_data_list.push_back(std::move(redo_data));
        }
    }

    // Second pass: replay committed transactions
    for (size_t i = 0; i < entries.size(); i++) {
        const auto& header = entries[i];

        if (txn_committed[header.txn_id]) {
            // Apply redo
            if (!callback(header, undo_data_list[i].data(), redo_data_list[i].data())) {
                std::cerr << "Failed to replay entry " << header.sequence << std::endl;
                return false;
            }
        }
        // Ignore uncommitted transactions (they are implicitly aborted)
    }

    return true;
}

// ... additional helper methods ...

} // namespace razorfs
```

---

### Week 3: Integration with Filesystem Operations

**Modify**: `fuse/razorfs_fuse.cpp` to use journaling

```cpp
// Example: Journaled write operation
static int razorfs_write(const char *path, const char *buf, size_t size,
                         off_t offset, struct fuse_file_info *fi) {
    try {
        // Begin transaction
        uint64_t txn_id = journal->begin_transaction();

        // Get current inode
        auto* node = filesystem->find_by_path(path);
        if (!node) {
            journal->abort_transaction(txn_id);
            return -ENOENT;
        }

        // Prepare undo data (old content)
        std::string old_data = block_manager.get_file_data(node->inode_number);

        // Prepare redo data (new content)
        std::string new_data = old_data;
        new_data.replace(offset, size, buf, size);

        // Write journal entry
        journal->write_entry(txn_id, JournalOpType::WRITE_DATA,
                            node->inode_number,
                            old_data.data(), old_data.size(),
                            new_data.data(), new_data.size());

        // Apply changes in memory
        int bytes_written = block_manager.write_blocks(node->inode_number,
                                                       buf, size, offset);

        // Commit transaction
        journal->commit_transaction(txn_id);

        return bytes_written;

    } catch (const std::exception& e) {
        std::cerr << "Write failed: " << e.what() << std::endl;
        return -EIO;
    }
}
```

---

## Week 5-6: Fine-Grained Locking

### Implementation Steps

1. **Add per-node mutex** to `CacheOptimizedNode`:

```cpp
struct CacheOptimizedNode {
    // Existing fields...

    // Add fine-grained locking
    mutable std::shared_mutex node_mutex;
};
```

2. **Remove global tree_mutex** and replace with selective locking

3. **Implement lock ordering** to prevent deadlocks:

```cpp
// Always acquire locks in this order:
// 1. inode_map_mutex_ (if needed)
// 2. Parent node_mutex
// 3. Child node_mutex

void rename_file(const std::string& old_path, const std::string& new_path) {
    auto* old_node = find_by_path(old_path);
    auto* new_parent = find_by_path(get_parent_path(new_path));
    auto* old_parent = find_by_inode(old_node->parent_inode);

    // Acquire locks in order (prevent deadlock)
    std::vector<CacheOptimizedNode*> nodes = {old_parent, new_parent, old_node};
    std::sort(nodes.begin(), nodes.end(),
              [](auto* a, auto* b) { return a->inode_number < b->inode_number; });

    std::vector<std::unique_lock<std::shared_mutex>> locks;
    for (auto* node : nodes) {
        locks.emplace_back(node->node_mutex);
    }

    // Now safe to modify
    // ...
}
```

---

## Testing Strategy

**Create comprehensive test suite**:

```bash
tests/
├── unit/
│   ├── test_journal_basic.cpp
│   ├── test_journal_recovery.cpp
│   ├── test_persistence_roundtrip.cpp
│   └── test_locking.cpp
├── integration/
│   ├── test_crash_recovery.cpp
│   └── test_concurrent_operations.cpp
└── stress/
    ├── stress_test_crash.sh
    └── stress_test_concurrent.sh
```

**Run continuously**:
```bash
# CI/CD pipeline
make test
make stress-test
make crash-test
```

---

## Success Criteria for Phase 1

- [ ] All persistence round-trip tests pass (1000+ cycles)
- [ ] Journal replay works correctly after simulated crashes
- [ ] No deadlocks in 48-hour concurrent stress test
- [ ] Code coverage > 70%
- [ ] Zero data loss in any test scenario

---

## Next Steps

After completing Phase 1:
1. Tag release v1.0.0-beta
2. Begin Phase 2 (Performance Optimization)
3. Start beta testing program

---

**Questions?** Open a GitHub discussion or contact nicoliberatoc@gmail.com
