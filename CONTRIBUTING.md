# Contributing to RAZORFS

Thank you for your interest in contributing to RAZORFS! This document provides guidelines for contributing to the project.

## Development Philosophy

RAZORFS embraces **AI-assisted engineering** with human oversight:

- **AI Tools**: Used for code generation, optimization, documentation, and testing
- **Human Role**: Architecture decisions, strategic direction, validation, and approval
- **Result**: Rapid prototyping with production-quality patterns through collaboration

We encourage exploring AI copilots (Claude Code, GitHub Copilot, etc.) for development tasks while maintaining rigorous human review.

## How to Contribute

### 1. Fork and Clone

```bash
git clone https://github.com/ncandio/razorfs.git
cd razorfs
```

### 2. Set Up Development Environment

**Prerequisites**:
- Linux (Ubuntu 22.04 LTS, Debian 11+, or WSL2)
- GCC 11+ or Clang 14+
- FUSE3 development libraries
- zlib development libraries

**Install Dependencies**:
```bash
sudo apt-get update
sudo apt-get install -y libfuse3-dev zlib1g-dev build-essential cmake
```

### 3. Build the Project

```bash
# Debug build (default)
make clean && make

# Release build (optimized)
make release

# Hardened build (security-optimized)
make hardened
```

### 4. Run Tests

**Always run tests before submitting**:

```bash
# Run full test suite
make test

# Run specific test categories
make test-unit          # Unit tests only
make test-integration   # Integration tests only
make test-static        # Static analysis
make test-valgrind      # Memory leak detection
make test-coverage      # Code coverage
```

**Required**: All 98 tests must pass ✅

### 5. Code Style Guidelines

**C Code Conventions**:
- Follow existing code style (K&R-style braces)
- Use meaningful variable names (no single-letter except loop counters)
- Add comments for complex algorithms
- Keep functions under 100 lines when possible
- Use `snake_case` for functions and variables

**Example**:
```c
// Good
int nary_find_child_mt(struct nary_node *parent, const char *name) {
    // Binary search for child in sorted array
    int left = 0, right = parent->num_children - 1;
    ...
}

// Avoid
int fc(struct nary_node *p, const char *n) { ... }  // Too cryptic
```

**Documentation**:
- Update README.md if adding user-facing features
- Add design docs to `docs/` for architectural changes
- Include inline comments for non-obvious code
- Update CHANGELOG.md with your changes

### 6. Testing Requirements

**Before submitting a PR, ensure**:
- ✅ All 98 existing tests pass
- ✅ New tests added for new functionality
- ✅ Code coverage does not decrease
- ✅ Static analysis (cppcheck) is clean
- ✅ Valgrind reports no memory leaks
- ✅ Sanitizers (ASan, UBSan, TSan) pass

**Adding Tests**:
- Unit tests: `tests/unit/*_test.cpp` (Google Test framework)
- Integration tests: `tests/integration/`
- Shell tests: `tests/shell/`

### 7. Submitting a Pull Request

**PR Checklist**:
- [ ] Code compiles without warnings
- [ ] All tests pass (98/98)
- [ ] Static analysis clean
- [ ] Memory leaks fixed (Valgrind clean)
- [ ] Code coverage maintained or improved
- [ ] Documentation updated
- [ ] CHANGELOG.md updated
- [ ] Commit messages are descriptive

**PR Template**:
```markdown
## Description
Brief description of changes

## Type of Change
- [ ] Bug fix
- [ ] New feature
- [ ] Performance improvement
- [ ] Documentation update
- [ ] Refactoring

## Testing
- All tests pass: Yes/No
- New tests added: Yes/No
- Manual testing performed: Describe

## Checklist
- [ ] Code follows project style guidelines
- [ ] Documentation updated
- [ ] Tests added/updated
- [ ] CHANGELOG.md updated
```

### 8. Commit Message Format

Use descriptive commit messages:

```
[category] Brief description (50 chars or less)

More detailed explanation if needed. Wrap at 72 characters.

- Bullet points are fine
- Explain what changed and why

Fixes #123
```

**Categories**:
- `feat`: New feature
- `fix`: Bug fix
- `perf`: Performance improvement
- `docs`: Documentation
- `test`: Testing
- `refactor`: Code refactoring
- `ci`: CI/CD changes

**Examples**:
```
feat: Add binary search for O(log n) child lookup

Replaces linear search with binary search on sorted children arrays.
Achieves 4x speedup for path operations on large directories.

- Hybrid threshold at 8 children (linear vs binary)
- Maintains sorted order during insert/delete
- Includes comprehensive unit tests

Fixes #45
```

## Types of Contributions

### 🐛 Bug Reports

**Use GitHub Issues** with:
- Clear description of the bug
- Steps to reproduce
- Expected vs actual behavior
- System information (OS, kernel, FUSE version)
- Relevant logs or error messages

### ✨ Feature Requests

**Before proposing a feature**:
1. Check existing issues/PRs
2. Discuss on GitHub Discussions if major change
3. Review [PRODUCTION_ROADMAP.md](docs/development/PRODUCTION_ROADMAP.md)

### 📚 Documentation

Documentation improvements are always welcome:
- Fix typos or unclear sections
- Add examples or clarifications
- Improve code comments
- Translate documentation (future)

### 🧪 Testing

Help improve test coverage:
- Add unit tests for uncovered code
- Create integration tests for real-world scenarios
- Add benchmark tests for performance validation
- Report test failures or flaky tests

### 🔒 Security

**Responsible Disclosure**:
- Do NOT open public issues for security vulnerabilities
- Email: nicoliberatoc@gmail.com
- Include detailed description and proof-of-concept
- Allow time for patching before public disclosure

See [SECURITY.md](SECURITY.md) for full policy.

## Development Workflow

### Branch Strategy

- `main` - Stable releases
- `develop` - Integration branch (if adopted)
- `feature/*` - New features
- `fix/*` - Bug fixes
- `perf/*` - Performance improvements

### Review Process

1. Submit PR against `main`
2. Automated tests run via GitHub Actions
3. Code review by maintainers
4. Address feedback and update PR
5. Merge when approved and tests pass

### CI/CD Pipeline

Every commit triggers:
- Build (GCC + Clang, Debug + Release)
- Unit tests (98 tests)
- Static analysis (cppcheck, clang-tidy)
- Dynamic analysis (Valgrind)
- Sanitizers (ASan, UBSan, TSan)
- Code coverage reporting

## Getting Help

- **Documentation**: Start with [docs/README.md](docs/README.md)
- **GitHub Discussions**: Ask questions, share ideas
- **GitHub Issues**: Report bugs, request features
- **Email**: nicoliberatoc@gmail.com

## Code of Conduct

### Our Standards

- Be respectful and inclusive
- Welcome newcomers and diverse perspectives
- Focus on technical merit
- Accept constructive criticism gracefully
- Prioritize the project's best interests

### Unacceptable Behavior

- Harassment or discrimination
- Trolling or insulting comments
- Personal or political attacks
- Publishing private information
- Spam or off-topic discussions

### Enforcement

Violations may result in:
1. Warning
2. Temporary ban
3. Permanent ban

Report issues to: nicoliberatoc@gmail.com

## Recognition

Contributors are recognized in:
- Git commit history
- Release notes in CHANGELOG.md
- Main README.md (for significant contributions)

## License

By contributing, you agree that your contributions will be licensed under the BSD-3-Clause License. See [LICENSE](LICENSE) for details.

---

## Additional Resources

- [README.md](README.md) - Project overview
- [docs/README.md](docs/README.md) - Documentation index
- [docs/development/STATUS.md](docs/development/STATUS.md) - Development status
- [docs/development/PRODUCTION_ROADMAP.md](docs/development/PRODUCTION_ROADMAP.md) - Future plans
- [CHANGELOG.md](CHANGELOG.md) - Version history

---

**Thank you for contributing to RAZORFS!** 🚀

Your contributions help advance AI-assisted systems programming and filesystem research.
