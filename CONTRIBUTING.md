# Contributing to CubeSat OBC

Thank you for your interest in contributing to the CubeSat OBC project! This document provides guidelines and instructions for contributing.

## Code of Conduct

- Be respectful and professional in all interactions
- Provide constructive feedback
- Focus on the code, not the person
- Help each other learn and grow

## Getting Started

### Prerequisites

- Linux, macOS, or WSL (on Windows)
- `cmake` (3.13+)
- `gcc` or `clang` (C11 support)
- `git`
- Optional: `cppcheck` for static analysis

### Setup Development Environment

```bash
# Clone the repository
git clone https://github.com/yourusername/cubesat-obc.git
cd cubesat-obc

# Create build directory
mkdir -p build && cd build

# Configure
cmake ..

# Build
cmake --build . -- -j$(nproc)

# Run tests
ctest --output-on-failure
```

### For Pico Hardware Development

```bash
# Export Pico SDK path
export PICO_SDK_PATH=/path/to/pico-sdk

# Reconfigure and build
cmake ..
cmake --build .
```

## Development Workflow

### 1. Create a Feature Branch

```bash
git checkout -b feature/your-feature-name
```

**Branch naming conventions:**
- `feature/description` - New features
- `fix/description` - Bug fixes
- `docs/description` - Documentation updates
- `test/description` - Test additions
- `refactor/description` - Code reorganization

### 2. Make Your Changes

- Follow the [Coding Standards](docs/CODING_STANDARDS.md)
- Write clear, concise commit messages
- Keep commits atomic (one logical change per commit)
- Add/update tests for new functionality

### 3. Run Tests Locally

```bash
cd build
ctest --output-on-failure

# Run static analysis
../scripts/static_analysis.sh

# Check build without warnings
cmake --build . -- VERBOSE=1
```

### 4. Commit and Push

```bash
git add .
git commit -m "feat: description of change"
git push origin feature/your-feature-name
```

**Commit message format:**
```
<type>: <subject>

<body (optional)>

<footer (optional)>
```

**Types:**
- `feat:` New feature
- `fix:` Bug fix
- `docs:` Documentation
- `test:` Test addition/modification
- `refactor:` Code reorganization without behavior change
- `chore:` Build, CI, dependency updates
- `style:` Code style (whitespace, formatting)

### 5. Create a Pull Request

- Provide a clear title and description
- Link related issues with `Fixes #123`
- Ensure CI passes (GitHub Actions)
- Request review from team members

## Pull Request Checklist

- [ ] Code follows [Coding Standards](docs/CODING_STANDARDS.md)
- [ ] Tests pass locally: `ctest --output-on-failure`
- [ ] No compiler warnings: build with `-Wall -Wextra -pedantic`
- [ ] Static analysis passes: `scripts/static_analysis.sh`
- [ ] Commit messages are clear and descriptive
- [ ] Documentation updated (if applicable)
- [ ] No hardcoded secrets or sensitive data
- [ ] New features include unit tests
- [ ] Changelog updated: `CHANGELOG.md`

## Code Style

### General Rules (MISRA-like)

1. **Naming Conventions**
   - Functions: `module_function_name()`
   - Constants: `MODULE_CONSTANT`
   - Types: `module_type_t`
   - Global variables: `g_module_var`
   - Static variables: `s_static_var`

2. **Memory Safety**
   - No dynamic allocation in flight code
   - Explicit bounds checking on arrays/buffers
   - Check null pointers before dereferencing

3. **Compiler Flags**
   - Always compile with: `-Wall -Wextra -pedantic`
   - Treat warnings as errors in release builds: `-Werror`

4. **Formatting**
   - Indentation: 4 spaces (no tabs)
   - Max line length: 100 characters
   - Braces: K&R style (1TBS)

### Example Function

```c
// good_function.c
#include "module.h"
#include <stdio.h>

// Compute checksum with bounds checking
static uint32_t compute_checksum(const uint8_t *data, size_t length) {
    uint32_t checksum = 0U;
    
    if (data == NULL || length == 0U) {
        return 0U;
    }
    
    for (size_t i = 0U; i < length; i++) {
        checksum += (uint32_t)data[i];
    }
    
    return checksum;
}
```

## Testing

### Unit Tests

- Location: `tests/unit/`
- Use framework: C (custom test harness for now)
- Target coverage: 80%+ for non-critical, 100% for critical paths

### Running Tests

```bash
# All tests
ctest --test-dir build --output-on-failure

# Specific test
ctest --test-dir build --output-on-failure -R test_name

# Verbose output
ctest --test-dir build --output-on-failure --verbose
```

### Adding New Tests

1. Create test file in `tests/unit/`
2. Add executable to `tests/unit/CMakeLists.txt`
3. Include headers and link dependencies
4. Expand `add_test()` calls in CMake

Example:
```cmake
add_executable(test_myfeature test_myfeature.c ../../src/path/myfeature.c)
target_include_directories(test_myfeature PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../include)
add_test(NAME myfeature_test COMMAND test_myfeature)
```

## Documentation

### What Needs Documentation

- Public APIs (headers)
- Architecture changes
- Complex algorithms
- Configuration options
- Build/deployment procedures

### Where to Document

- **Code**: Inline comments for "why", not "what"
- **Headers**: Function signatures with pre/post conditions
- **docs/**: High-level design, guides, standards
- **README.md**: Quick start, project overview

### Documentation Format

- Markdown for docs
- Doxygen comments for code (optional)
- Clear, concise language
- Examples where helpful

## Reporting Issues

Use GitHub Issues with:

1. **Title**: Clear, specific
2. **Description**: What, expected vs. actual
3. **Steps to Reproduce**: How to trigger
4. **Environment**: OS, compiler, build command
5. **Attachments**: Logs, test outputs (if public)

Template:
```markdown
**Description:**
Brief description of the issue.

**Steps to Reproduce:**
1. Step one
2. Step two

**Expected Behavior:**
What should happen

**Actual Behavior:**
What actually happens

**Environment:**
- OS: Linux/macOS/Windows
- Compiler: GCC 13.x
- CMake: 3.x
```

## Project Structure

```
cubesat-obc/
├── src/              # Source code (drivers, tasks, control, etc.)
├── include/          # Public headers
├── tests/            # Unit tests
├── docs/             # Documentation
├── scripts/          # Build and utility scripts
├── config/           # Configuration files (FreeRTOS, etc.)
└── CMakeLists.txt    # Build system
```

## CI/CD Pipeline

GitHub Actions automatically:
1. Builds on push/PR
2. Runs all tests
3. Reports coverage
4. Checks for compiler warnings

**View results:** GitHub Actions tab on PR

## Releases

Versioning follows [Semantic Versioning](https://semver.org/):

- **MAJOR.MINOR.PATCH** (e.g., `1.0.0`)
- MAJOR: Breaking changes
- MINOR: New features (backward compatible)
- PATCH: Bug fixes

Update:
1. `CHANGELOG.md`
2. Tag in git: `git tag v1.0.0`
3. Create GitHub Release

## Questions or Discussions?

- **Issues**: Bug reports and feature requests
- **Discussions**: Questions, ideas, design feedback
- **Email**: Contact project maintainers

## Resources

- [Coding Standards](docs/CODING_STANDARDS.md)
- [Build Guide](docs/BUILD_GUIDE.md)
- [Architecture](docs/ARCHITECTURE.md) (coming soon)
- [FreeRTOS Docs](https://www.freertos.org/Documentation/161204_FreeRTOS_Reference_Manual_V10.0.0.pdf)

---

Thank you for contributing! 🚀
