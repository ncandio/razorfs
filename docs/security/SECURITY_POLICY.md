# Security Policy

## Supported Versions

The following versions of RazorFS are currently being supported with security updates:

| Version | Supported          |
| ------- | ------------------ |
| 1.x.x   | :white_check_mark: (Development) |
| < 1.0   | :x:                |

## Reporting a Vulnerability

**RazorFS is currently in alpha development and NOT recommended for production use.**

If you discover a security vulnerability, please report it responsibly by:

1. **Email**: Contact the maintainers at nicoliberatoc@gmail.com
2. **GitHub Issues**: Use the "security vulnerability" issue template (if available)
3. **Do not** publicly disclose the vulnerability until it has been addressed

### Expected Response Time
- **Acknowledgment**: Within 48 hours
- **Initial assessment**: Within 1 week
- **Fix timeline**: Depends on severity and complexity

## Security Design Principles

RazorFS implements several security measures at the architecture level:

### 1. Path Traversal Protection
- Rejects `..` path components
- Validates all path components for control characters
- Implements bounds checking on path parsing

### 2. Input Validation
- Validates file names for null bytes and control characters
- Implements maximum filename length constraints
- Checks for potential buffer overflow conditions

### 3. Concurrency Protection
- Implements per-inode locking to prevent race conditions
- Uses thread-safe data structures
- Maintains lock ordering to prevent deadlocks

### 4. Memory Safety
- Uses safe string functions where possible
- Implements bounds checking for array access
- Validates pointers before dereferencing

## Known Security Limitations

⚠️ **IMPORTANT**: RazorFS has several known security limitations that make it unsuitable for production use:

1. **No journaling/recovery**: Power failures can result in filesystem corruption
2. **Shared memory persistence**: Not crash-safe by design
3. **Limited testing**: Insufficient fuzzing and security testing performed
4. **No access control**: No support for advanced permission models
5. **No encryption**: All data stored in plaintext

## Security Testing

The project includes automated security testing through GitHub Actions:

- **CodeQL Analysis**: Static analysis for security vulnerabilities
- **Memory Sanitizers**: Detection of memory safety issues
- **Dependency Scanning**: Vulnerability detection in dependencies
- **Fuzz Testing**: Automated testing with malformed inputs
- **Hardening Checks**: Verification of security compilation flags

## Security Updates

Security updates are released as part of the regular development cycle. Users are encouraged to keep their versions up to date.

## Contact

For security-related inquiries:
- **Email**: nicoliberatoc@gmail.com
- **GitHub**: https://github.com/ncandio/razorfs