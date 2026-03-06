# Repository Configuration Guide

This document provides recommended configuration for the GitHub repository.

## Branch Protection Rules

### Protect `main` branch:

1. **Require pull request reviews**
   - Required number of reviewers: 1
   - Require review from code owners: No (optional)
   - Dismiss stale pull request approvals: Yes

2. **Require status checks to pass**
   - GitHub Actions (build, test, cppcheck)
   - Require branches to be up to date: Yes

3. **Require code to be up to date before merging**
   - Yes

4. **Require conversation resolution**
   - Yes (all comments must be resolved)

5. **Allow force pushes**
   - No

## GitHub Settings

### General
- **Description**: "Flight-ready CubeSat OBC firmware using Pico 2W + FreeRTOS"
- **Topics**: `cubesat`, `pico`, `arm-cortex-m`, `freertos`, `spacecraft`, `attitude-control`, `embedded-systems`
- **Template repository**: No
- **Default branch**: `main`

### Collaborators
- Add team members with appropriate roles:
  - Owners: Project leads
  - Maintainers: Active contributors
  - Collaborators: Regular contributors

### Secrets (if needed for CI/CD)
- None required for this public project currently
- If adding private artifacts, add: `GITHUB_TOKEN` (automatic)

### Actions
- **Allow GitHub Actions**: Yes
- **Allow public workflows**: Yes
- **Default branch**: main
- **Configure cache**: Enable cache for build artifacts

### Pages (Optional)
- Enable GitHub Pages for documentation
- Source: GitHub Actions (auto-deploy docs/)
- Domain: (your-org).github.io/cubesat-obc

## Recommended GitHub Labels

Create these labels in Issues/PRs for organization:

| Label | Color | Description |
|-------|-------|-------------|
| `bug` | #d73a49 | Something isn't working |
| `enhancement` | #a2eeef | New feature or request |
| `documentation` | #0075ca | Improvements or additions to documentation |
| `good first issue` | #7057ff | Good for newcomers |
| `help wanted` | #008672 | Extra attention is needed |
| `question` | #d876e3 | Further information is requested |
| `wontfix` | #ffffff | This will not be worked on |
| `duplicate` | #cfd3d7 | Issue or PR already exists |
| `high priority` | #ff0000 | Urgent/blocking |
| `low priority` | #cccccc | Nice-to-have |
| `pico-sdk` | #0099cc | Related to Pico SDK integration |
| `freertos` | #ffcc00 | Related to FreeRTOS |
| `hardware` | #996600 | Hardware-specific issues |
| `testing` | #00aa00 | Test-related changes |

## Recommended GitHub Actions Workflows

### Build & Test (included)
- Triggers: Push to main/develop, PRs
- Runs: CMake build, ctest, static analysis

### Auto-format (optional)
```yaml
name: Code Format Check
on: [pull_request]
jobs:
  format:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - name: Check C formatting
        run: find src include -name "*.c" -o -name "*.h" | xargs clang-format -i && git diff --exit-code
```

### Release (optional)
```yaml
name: Create Release
on:
  push:
    tags:
      - 'v*'
jobs:
  release:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - name: Build firmware
        run: |
          mkdir build && cd build
          cmake ..
          cmake --build .
      - name: Create Release
        uses: softprops/action-gh-release@v1
        with:
          files: build/src/cubesat_obc_firmware.*
```

## Code Owners

Create `.github/CODEOWNERS` file:

```
# Who reviews what

# Documentation
docs/                   @project-lead @maintainer1
CONTRIBUTING.md         @project-lead

# Core control
src/control/           @control-expert @reviewer1
src/dynamics/          @control-expert @reviewer1

# Tasks & RTOS
src/tasks/             @freertos-expert @reviewer2
config/FreeRTOS*       @freertos-expert @reviewer2

# Hardware drivers
src/drivers/           @hardware-expert @reviewer3

# Tests
tests/                 @ control-expert @reviewer1 @reviewer2

# CI/CD
.github/workflows/     @project-lead
```

## Privacy & Security

- ✅ No API keys in repository
- ✅ No credentials in code
- ✅ .gitignore excludes build artifacts
- ✅ License: MIT (public)
- ✅ No confidential hardware specs
- ✅ Project suitable for open-source collaboration

---

## Deployment Checklist

Before making the repository public:

- [ ] README.md is complete and welcoming
- [ ] LICENSE file is present (MIT checked)
- [ ] CONTRIBUTING.md has clear guidelines
- [ ] CHANGELOG.md is initialized
- [ ] Code is free of secrets and sensitive data
- [ ] .gitignore is comprehensive
- [ ] GitHub Actions workflows pass
- [ ] Branch protection enabled on `main`
- [ ] Issue and PR templates configured
- [ ] Project description and topics set
- [ ] Contributing guidelines linked in profile
- [ ] Code of conduct included (optional)

---

**Last Updated:** 2026-02-12
