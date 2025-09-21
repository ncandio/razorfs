#pragma once

#include <stdexcept>
#include <string>
#include <system_error>
#include <fstream>
#include <iostream>

namespace RazorFS {

/**
 * Custom exception types for better error handling and diagnostics
 */
class FilesystemException : public std::runtime_error {
public:
    explicit FilesystemException(const std::string& message)
        : std::runtime_error("RazorFS Error: " + message) {}
};

class TreeStructureException : public FilesystemException {
public:
    explicit TreeStructureException(const std::string& message)
        : FilesystemException("Tree Structure Error: " + message) {}
};

class MemoryException : public FilesystemException {
public:
    explicit MemoryException(const std::string& message)
        : FilesystemException("Memory Error: " + message) {}
};

class PersistenceException : public FilesystemException {
public:
    explicit PersistenceException(const std::string& message)
        : FilesystemException("Persistence Error: " + message) {}
};

class ConcurrencyException : public FilesystemException {
public:
    explicit ConcurrencyException(const std::string& message)
        : FilesystemException("Concurrency Error: " + message) {}
};

/**
 * Error handling utilities
 */
class ErrorHandler {
public:
    static void handle_critical_error(const std::string& operation, const std::exception& e) {
        std::cerr << "[CRITICAL] RazorFS operation '" << operation << "' failed: "
                  << e.what() << std::endl;
        // In production, this might trigger filesystem recovery or safe shutdown
    }

    static void handle_recoverable_error(const std::string& operation, const std::string& details) {
        std::cerr << "[WARNING] RazorFS operation '" << operation << "' encountered issue: "
                  << details << std::endl;
        // Log for debugging but continue operation
    }

    static bool validate_path(const std::string& path) {
        if (path.empty()) return false;
        if (path[0] != '/') return false;
        if (path.find("..") != std::string::npos) return false; // Prevent directory traversal
        return true;
    }

    static bool validate_filename(const std::string& name) {
        if (name.empty() || name.length() > 255) return false;
        if (name == "." || name == "..") return false;
        if (name.find('/') != std::string::npos) return false;
        if (name.find('\0') != std::string::npos) return false;
        return true;
    }

    static void safe_file_write(const std::string& filename, const std::function<void(std::ofstream&)>& writer) {
        std::string temp_filename = filename + ".tmp";

        try {
            // Write to temporary file first
            std::ofstream temp_file(temp_filename, std::ios::binary);
            if (!temp_file.is_open()) {
                throw PersistenceException("Failed to create temporary file: " + temp_filename);
            }

            writer(temp_file);
            temp_file.flush();
            temp_file.close();

            // Atomically replace original file
            if (std::rename(temp_filename.c_str(), filename.c_str()) != 0) {
                throw PersistenceException("Failed to atomically replace file: " + filename);
            }

        } catch (const std::exception& e) {
            // Clean up temporary file on error
            std::remove(temp_filename.c_str());
            throw PersistenceException("Safe file write failed: " + std::string(e.what()));
        }
    }
};

/**
 * RAII wrapper for filesystem operations
 */
template<typename T>
class OperationGuard {
private:
    T* resource_;
    std::function<void(T*)> cleanup_;
    bool released_;

public:
    OperationGuard(T* resource, std::function<void(T*)> cleanup)
        : resource_(resource), cleanup_(cleanup), released_(false) {}

    ~OperationGuard() {
        if (!released_ && resource_) {
            try {
                cleanup_(resource_);
            } catch (const std::exception& e) {
                ErrorHandler::handle_critical_error("resource_cleanup", e);
            }
        }
    }

    T* get() { return resource_; }
    void release() { released_ = true; }

    // Non-copyable, movable
    OperationGuard(const OperationGuard&) = delete;
    OperationGuard& operator=(const OperationGuard&) = delete;
    OperationGuard(OperationGuard&& other) noexcept
        : resource_(other.resource_), cleanup_(std::move(other.cleanup_)), released_(other.released_) {
        other.released_ = true;
    }
};

/**
 * Transaction-like wrapper for multi-step operations
 */
class FilesystemTransaction {
private:
    std::vector<std::function<void()>> rollback_actions_;
    bool committed_;

public:
    FilesystemTransaction() : committed_(false) {}

    ~FilesystemTransaction() {
        if (!committed_) {
            rollback();
        }
    }

    void add_rollback_action(std::function<void()> action) {
        rollback_actions_.push_back(action);
    }

    void commit() {
        committed_ = true;
        rollback_actions_.clear();
    }

    void rollback() {
        for (auto it = rollback_actions_.rbegin(); it != rollback_actions_.rend(); ++it) {
            try {
                (*it)();
            } catch (const std::exception& e) {
                ErrorHandler::handle_critical_error("transaction_rollback", e);
            }
        }
        rollback_actions_.clear();
    }
};

} // namespace RazorFS