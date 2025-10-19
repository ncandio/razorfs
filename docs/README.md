# RAZORFS Documentation Index

Welcome to the RAZORFS documentation. This directory contains comprehensive technical documentation organized by category.

## 📚 Documentation Structure

### [Architecture](architecture/)
Deep-dive into filesystem design, algorithms, and performance optimizations.

- [Complexity Analysis](architecture/COMPLEXITY_ANALYSIS.md) - Mathematical proof of O(log n)
- [Cache Locality Design](architecture/CACHE_LOCALITY.md) - Cache optimization strategies
- [WAL Design](architecture/WAL_DESIGN.md) - Write-ahead logging architecture
- [Recovery Design](architecture/RECOVERY_DESIGN.md) - ARIES-style crash recovery
- [XAttr Design](architecture/XATTR_DESIGN.md) - Extended attributes implementation
- [Persistence Architecture](architecture/PERSISTENCE_SOLUTION_SUMMARY.md) - Disk-backed storage

### [Features](features/)
Specifications and design documents for filesystem features.

- [Hardlink Design](features/HARDLINK_DESIGN.md) - Reference counting implementation
- [Large File Design](features/LARGE_FILE_DESIGN.md) - Handling files >10MB
- [S3 Integration](features/S3_IMPLEMENTATION_SUMMARY.md) - Cloud storage backend

### [Development](development/)
Guides for developers working on RAZORFS.

- [Development Status](development/STATUS.md) - Current implementation status
- [Production Roadmap](development/PRODUCTION_ROADMAP.md) - Future enhancements
- [Cross-Compilation](development/CROSS_COMPILE.md) - Building for ARM, PowerPC, RISC-V

### [Operations](operations/)
Deployment, testing, and operational procedures.

- [Deployment Guide](operations/DEPLOYMENT_GUIDE.md) - Production deployment instructions
- [Testing Summary](operations/TESTING_SUMMARY.md) - Comprehensive test coverage
- [Fuzzing Guide](operations/FUZZING.md) - Fuzz testing procedures

### [Security](security/)
Security audits, policies, and vulnerability management.

- [Security Audit](security/SECURITY_AUDIT.md) - Security analysis and hardening
- [Security Policy](security/SECURITY_POLICY.md) - Vulnerability reporting

### [CI/CD](ci-cd/)
Continuous integration and delivery documentation.

- [GitHub Actions](ci-cd/GITHUB_ACTIONS.md) - CI/CD pipeline configuration
- [Coverage Report](ci-cd/COVERAGE_REPORT.md) - Code coverage analysis

## 🚀 Quick Start

1. **First Time?** Start with the main [README.md](../README.md)
2. **Want to Deploy?** Check [Deployment Guide](operations/DEPLOYMENT_GUIDE.md)
3. **Contributing?** See [CONTRIBUTING.md](../CONTRIBUTING.md)
4. **Understanding Performance?** Read [Complexity Analysis](architecture/COMPLEXITY_ANALYSIS.md)

## 📖 Reading Order for New Contributors

1. Main [README.md](../README.md) - Project overview
2. [Development Status](development/STATUS.md) - Current state
3. [Complexity Analysis](architecture/COMPLEXITY_ANALYSIS.md) - Core algorithms
4. [Testing Summary](operations/TESTING_SUMMARY.md) - Quality assurance
5. [Production Roadmap](development/PRODUCTION_ROADMAP.md) - Future plans

## 🤝 Contributing

See [CONTRIBUTING.md](../CONTRIBUTING.md) for guidelines on contributing to documentation.

---

**Last Updated**: 2025-10-19
**Documentation Version**: 1.0
