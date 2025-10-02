#pragma once

#include <stdexcept>
#include <string>
#include <cerrno>

namespace razorfs {

/**
 * Error codes for RazorFS operations
 */
enum class ErrorCode {
    // Filesystem errors
    FILE_NOT_FOUND,
    FILE_EXISTS,
    PERMISSION_DENIED,
    NOT_A_DIRECTORY,
    IS_A_DIRECTORY,
    DIRECTORY_NOT_EMPTY,

    // I/O errors
    IO_ERROR,
    DISK_FULL,
    READ_ONLY,

    // Corruption errors
    CORRUPTED_METADATA,
    CORRUPTED_DATA,
    INVALID_CHECKSUM,
    INVALID_OFFSET,

    // Internal errors
    OUT_OF_MEMORY,
    INTERNAL_ERROR,
    NOT_IMPLEMENTED,
    INVALID_ARGUMENT
};

/**
 * Base exception class for all RazorFS errors
 */
class FilesystemError : public std::runtime_error {
private:
    ErrorCode code_;
    std::string path_;

public:
    FilesystemError(ErrorCode code, const std::string& msg,
                   const std::string& path = "")
        : std::runtime_error(msg), code_(code), path_(path) {}

    ErrorCode code() const noexcept { return code_; }
    const std::string& path() const noexcept { return path_; }
};

/**
 * Specific exception types
 */
class FileNotFoundError : public FilesystemError {
public:
    explicit FileNotFoundError(const std::string& path)
        : FilesystemError(ErrorCode::FILE_NOT_FOUND,
                         "File not found: " + path, path) {}
};

class FileExistsError : public FilesystemError {
public:
    explicit FileExistsError(const std::string& path)
        : FilesystemError(ErrorCode::FILE_EXISTS,
                         "File already exists: " + path, path) {}
};

class NotADirectoryError : public FilesystemError {
public:
    explicit NotADirectoryError(const std::string& path)
        : FilesystemError(ErrorCode::NOT_A_DIRECTORY,
                         "Not a directory: " + path, path) {}
};

class IsADirectoryError : public FilesystemError {
public:
    explicit IsADirectoryError(const std::string& path)
        : FilesystemError(ErrorCode::IS_A_DIRECTORY,
                         "Is a directory: " + path, path) {}
};

class CorruptionError : public FilesystemError {
public:
    explicit CorruptionError(const std::string& msg, const std::string& path = "")
        : FilesystemError(ErrorCode::CORRUPTED_METADATA, msg, path) {}
};

class StringTableException : public FilesystemError {
public:
    explicit StringTableException(const std::string& msg)
        : FilesystemError(ErrorCode::INVALID_OFFSET, msg) {}
};

class IOError : public FilesystemError {
public:
    explicit IOError(const std::string& msg, const std::string& path = "")
        : FilesystemError(ErrorCode::IO_ERROR, msg, path) {}
};

/**
 * Convert ErrorCode to errno value
 */
inline int to_errno(ErrorCode code) noexcept {
    switch (code) {
        case ErrorCode::FILE_NOT_FOUND: return ENOENT;
        case ErrorCode::FILE_EXISTS: return EEXIST;
        case ErrorCode::PERMISSION_DENIED: return EACCES;
        case ErrorCode::NOT_A_DIRECTORY: return ENOTDIR;
        case ErrorCode::IS_A_DIRECTORY: return EISDIR;
        case ErrorCode::DIRECTORY_NOT_EMPTY: return ENOTEMPTY;
        case ErrorCode::IO_ERROR: return EIO;
        case ErrorCode::DISK_FULL: return ENOSPC;
        case ErrorCode::READ_ONLY: return EROFS;
        case ErrorCode::CORRUPTED_METADATA:
        case ErrorCode::CORRUPTED_DATA:
        case ErrorCode::INVALID_CHECKSUM:
        case ErrorCode::INVALID_OFFSET:
            return EIO;
        case ErrorCode::OUT_OF_MEMORY: return ENOMEM;
        case ErrorCode::INVALID_ARGUMENT: return EINVAL;
        default: return EIO;
    }
}

} // namespace razorfs
