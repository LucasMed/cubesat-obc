# Software Development Plan

**Document ID**: SDP-OBC-001
**Version**: 1.0
**Date**: 2026-03-07
**Status**: Approved — SRR Baseline
**Standard**: ECSS-E-ST-40C §5.4, ECSS-Q-ST-80C
**Project**: CubeSat OBC Flight Software (RP2350 / Pico 2W)

---

## Table of Contents

1. [Introduction](#1-introduction)
2. [Applicable Documents](#2-applicable-documents)
3. [Acronyms and Definitions](#3-acronyms-and-definitions)
4. [Development Environment](#4-development-environment)
5. [Software Development Standards](#5-software-development-standards)
6. [Development Lifecycle and Phases](#6-development-lifecycle-and-phases)
7. [Version Control and Branching Strategy](#7-version-control-and-branching-strategy)
8. [Build System](#8-build-system)
9. [Continuous Integration](#9-continuous-integration)
10. [Code Review Process](#10-code-review-process)
11. [Release Management](#11-release-management)
12. [Documentation Management](#12-documentation-management)
13. [Open Items](#13-open-items)

---

## 1. Introduction

### 1.1 Purpose

This Software Development Plan (SDP) defines the processes, tools, standards, and
procedures governing the development of the CubeSat OBC flight software. It is a
mandatory SRR deliverable per ECSS-E-ST-40C and governs all software development
activities from SRR to flight delivery.

### 1.2 Scope

This plan covers all software developed for or executed on the CubeSat OBC:

- FreeRTOS-based flight software running on RP2350 (Cortex-M33)
- Simulation and test infrastructure (host x86_64 Linux)
- Build scripts, CI pipelines, and tooling

It does **not** cover:
- Ground software (ground station, mission control)
- Pico SDK or FreeRTOS kernel (third-party, managed as submodules)

### 1.3 Project Context

The OBC executes on a Raspberry Pi Pico 2W (RP2350, dual Cortex-M33 at 133 MHz)
running FreeRTOS `ARM_CM33_NTZ`. The software provides attitude determination and
control (ADCS), telemetry downlink via CSP/KISS/UART, command uplink processing,
fault detection and isolation, flight mode management, and EPS monitoring.

---

## 2. Applicable Documents

| ID | Title | Version |
|---|---|---|
| MRD-OBC-001 | Mission Requirements Document | 1.0 |
| SyRS-OBC-001 | System Requirements Specification | 1.0 |
| SRS-OBC-001 | Software Requirements Specification | 2.0 |
| SVVP-OBC-001 | Software Verification & Validation Plan | 1.0 |
| CMP-OBC-001 | Configuration Management Plan | 1.0 |
| RMP-OBC-001 | Risk Management Plan | 1.0 |
| CODING_STANDARDS.md | CubeSat OBC Coding Standards | 1.0 |
| ECSS-E-ST-40C | Software Engineering Standard | — |
| ECSS-Q-ST-80C | Software Product Assurance | — |

---

## 3. Acronyms and Definitions

| Acronym | Definition |
|---|---|
| SDP | Software Development Plan |
| ECSS | European Cooperation for Space Standardization |
| SRR | System Requirements Review |
| PDR | Preliminary Design Review |
| CDR | Critical Design Review |
| TRR | Test Readiness Review |
| CI | Continuous Integration |
| PR | Pull Request |
| HAL | Hardware Abstraction Layer |
| FMM | Flight Mode Manager |
| ADCS | Attitude Determination and Control System |
| EPS | Electrical Power System |
| CSP | CubeSat Space Protocol |
| KISS | Keep It Simple, Stupid (framing protocol) |
| DLA | Data Layer Abstraction |
| FDIR | Fault Detection, Isolation and Recovery |

---

## 4. Development Environment

### 4.1 Host Development Platform

| Component | Specification |
|---|---|
| OS | Ubuntu 22.04 LTS (native, Docker, or Dev Container) |
| Container image | `docker/Dockerfile.pico` (Ubuntu 22.04 base) |
| Dev Container | VS Code Dev Containers extension supported |

### 4.2 Toolchain — Host (Test Build)

| Tool | Version | Purpose |
|---|---|---|
| GCC | System default (Ubuntu 22.04) | Host test compilation |
| CMake | ≥ 3.13 | Build system |
| Ninja | System default | Build back-end |
| CTest | Bundled with CMake | Test runner |
| Unity | v2.x (submodule) | Unit test framework |
| clang-format | 14 (pinned) | Code formatting |
| clang-tidy | 14 (pinned) | Static analysis |
| cppcheck | System default | Static analysis |

### 4.3 Toolchain — Target (Pico / Embedded)

| Tool | Version | Purpose |
|---|---|---|
| ARM GCC (arm-none-eabi-gcc) | 12.x | Cross-compilation |
| Pico SDK | Submodule (`pico-sdk/`) | RP2350 BSP and HAL |
| FreeRTOS Kernel | Submodule (`third_party/FreeRTOS-Kernel/`) | RTOS |
| libcsp | Submodule (`third_party/libcsp/`) | CubeSat Space Protocol |
| picotool | System / docker installed | UF2 flashing |

### 4.4 CMake Build Configurations

| Build Dir | Mode | Purpose |
|---|---|---|
| `build/` | Host, debug | Unit tests + firmware stub |
| `build_pico/` | Pico target | Flash-ready UF2 artifacts |
| `build_emu/` | Host emulation | Emulator-based system tests |

The build system selects platform-specific code via `PICO_BUILD` CMake variable:

```cmake
if(PICO_BUILD)
    # hardware drivers, CSP router task, watchdog
else()
    # stub implementations for host test
endif()
```

---

## 5. Software Development Standards

### 5.1 Language and Standard

All flight software is written in **C11** (`-std=c11`). Assembly is forbidden
except within third-party BSP code.

### 5.2 Coding Standard

The project follows the `CODING_STANDARDS.md` document (STD-001 v1.0), which
specifies:

- Naming conventions: `module_action()`, `module_name_t`, `MODULE_CONSTANT`
- No dynamic memory allocation (`malloc`/`free`) in flight code
- Static allocation only; FreeRTOS heap used for task stacks at init only
- `stdint.h` types for hardware interfaces
- No recursion permitted

Refer to [ecss/standards/CODING_STANDARDS.md](../standards/CODING_STANDARDS.md)
for the full standard.

### 5.3 MISRA-C Compliance

The project targets a MISRA C:2012 subset. Known deviations are documented in
[ecss/standards/MISRA_DEVIATIONS.md](../standards/MISRA_DEVIATIONS.md) with
rationale, risk assessment, and compensating controls.

### 5.4 Compiler Warnings Policy

All code **must** compile cleanly with:

```
-Wall -Wextra -pedantic -Werror
```

Warnings are treated as errors in the CI pipeline. Suppressions require explicit
`// NOLINT(...)` annotation and review approval.

### 5.5 Commit Message Convention

Commits follow the Conventional Commits format:

```
<type>(<scope>): <subject>

<body>
```

Types: `feat`, `fix`, `docs`, `test`, `refactor`, `chore`, `style`

--- 

## 6. Development Lifecycle and Phases

The project follows a phased development model aligned with ECSS review gates:

| Phase | Milestone | Focus | Status |
|---|---|---|---|
| Phase 1 | — | Task scaffolding, PID, actuators | ✅ Complete |
| Phase 2 | — | Real-time scheduling, sensor integration | ✅ Complete |
| Phase 3 | SRR | Communications subsystem (CSP, KISS, UART) | ✅ Complete |
| Phase 4 | PDR | EKF attitude determination, LQR control | ✅ Complete |
| Phase SA | PDR | Service layer (FMM, EPS, Fault, Logger, DLA) | ✅ Complete |
| Phase 5 | CDR | Subsystem design documentation (CDR deliverables) | 🔄 In progress |
| Phase 6 | TRR | System integration, hardware validation | ❌ Planned |
| Phase 7 | AR/QR | Qualification testing, flight build | ❌ Planned |

### 6.1 Entry Criteria per Review Gate

| Review | Entry Criteria |
|---|---|
| **SRR** | SyRS, SRS, SDP, SVVP, CMP, RMP delivered and reviewed |
| **PDR** | SAD, ICD, BOM delivered; all SRR baselines approved; unit test coverage ≥ 80% |
| **CDR** | All `*-DES-001` subsystem design documents delivered; static analysis clean; 100% unit tests passing |
| **TRR** | All CDR deliverables approved; integration test plan (ITP-OBC-001) written; hardware available |
| **AR/QR** | TRR exit criteria met; qualification tests passed; SCI released |

---

## 7. Version Control and Branching Strategy

### 7.1 Repository

| Property | Value |
|---|---|
| Platform | GitHub |
| Repository | `LucasMed/cubesat-obc` |
| Default protected branch | `main` (stable releases) |
| Active development branch | `dev` |

### 7.2 Branch Model

```
main          ← tagged releases (v0.x.0, v1.0.0)
  └── dev     ← integration branch (CI must pass to merge)
        ├── feature/description   ← new features / subsystems
        ├── fix/description       ← bug fixes
        ├── docs/description      ← documentation-only changes
        ├── test/description      ← test additions
        └── refactor/description  ← code refactoring
```

All branches are created from `dev` and merged back via Pull Request.

### 7.3 Merge Requirements

A PR targeting `dev` requires:

1. CI pipeline passes (build + all tests + clang-format + cppcheck)
2. At least one approving review
3. `CHANGELOG.md` updated
4. All checklist items in the PR template completed

### 7.4 PR Description Generation

The script `scripts/gen_pr_description.sh <base-branch> [--run-tests]` auto-fills
the PR template from commit log, changed files, and test results:

```bash
bash scripts/gen_pr_description.sh dev --run-tests
```

---

## 8. Build System

### 8.1 CMake Structure

```
CMakeLists.txt           ← top-level
config/library.cmake     ← module library definitions
src/                     ← flight software sources
  core/                  ← comm_init, startup
  tasks/                 ← FreeRTOS tasks
  services/              ← FMM, EPS, Fault, Logger, DLA
  drivers/               ← HAL drivers (IMU, UART, I2C)
  actuators/             ← reaction wheels, magnetorquers
  control/               ← PID, LQR, EKF, dynamics
tests/unit/              ← Unity-based unit tests
third_party/             ← FreeRTOS, libcsp, Unity (submodules)
```

### 8.2 Build Commands

```bash
# Configure (host test build)
cmake -B build -DPICO_ENABLED=OFF

# Build
cmake --build build -j$(nproc)

# Run all tests
ctest --test-dir build --output-on-failure

# Static analysis
bash scripts/static_analysis.sh
```

### 8.3 Artifact Outputs

| Artifact | Location | Description |
|---|---|---|
| `cubesat_obc_firmware` | `build/src/` | Host test binary |
| `cubesat_obc_pico.uf2` | `build_pico/src/` | Flash-ready Pico image |
| `test_*` | `build/tests/unit/` | Unit test executables |
| Coverage HTML | `artifacts/coverage/` | Coverage reports (CI) |

---

## 9. Continuous Integration

### 9.1 CI Platform

GitHub Actions, running on `ubuntu-22.04` (pinned to avoid tool version drift).

### 9.2 CI Workflow

The `.github/workflows/ci.yml` pipeline executes on every push to `dev`, `main`,
and all PR branches:

| Step | Tool | Fail Condition |
|---|---|---|
| Checkout + submodules | `actions/checkout` | Checkout error |
| Configure | `cmake -B build -DPICO_ENABLED=OFF` | CMake error |
| Build | `cmake --build build -j4` | Compile error or warning |
| Unit tests | `ctest --output-on-failure` | Any test failure |
| Formatting | `clang-format-14 --dry-run --Werror` | Format violation |
| Static analysis | `cppcheck` / `clang-tidy-14` | Error-level finding |

### 9.3 CI Pinning Policy

All CI Ubuntu-hosted jobs are pinned to `ubuntu-22.04`. The Docker image
(`docker/Dockerfile.pico`) uses `clang-format-14` and `clang-tidy-14` to match.
This prevents formatting disagreements between local and CI environments.

---

## 10. Code Review Process

### 10.1 Mandatory Review

All code changes targeting `dev` or `main` must be reviewed via Pull Request.
Direct pushes to `dev` and `main` are disabled (branch protection).

### 10.2 Review Checklist

Reviewers verify:

- [ ] Code follows `CODING_STANDARDS.md`
- [ ] No dynamic memory allocation in flight code
- [ ] Unit tests added / updated for new functionality
- [ ] All tests pass locally
- [ ] No compiler warnings with `-Wall -Wextra -pedantic`
- [ ] Static analysis clean
- [ ] `CHANGELOG.md` updated
- [ ] No sensitive data included

### 10.3 FDIR-Relevant Changes

Changes to `fault_manager.c`, `flight_mode_manager.c`, or `eps_monitor.c` require
review by the FDIR subsystem responsible engineer and a comment confirming the
fault tree has been re-evaluated.

---

## 11. Release Management

### 11.1 Versioning Scheme

The project follows Semantic Versioning (`MAJOR.MINOR.PATCH`):

| Component | Trigger |
|---|---|
| MAJOR | Incompatible architecture change or flight hardware re-qualification required |
| MINOR | New feature, subsystem, or document merged to `dev` |
| PATCH | Bug fix or documentation correction without functional impact |

### 11.2 Release Procedure

1. Update `CHANGELOG.md` — move `[Unreleased]` to `[M.m.p]`
2. Create PR from `dev` to `main` with release notes
3. CI must pass on `main`
4. Tag release: `git tag -a v<M.m.p> -m "Release v<M.m.p>"`
5. Push tag: `git push origin v<M.m.p>`
6. GitHub Actions generates release artifacts and publishes UF2

### 11.3 Baseline Tags

ECSS baselines are tagged on `main`:

| Baseline | Tag Format | Example |
|---|---|---|
| SRR | `baseline/srr-vX.Y.Z` | `baseline/srr-v0.18.0` |
| PDR | `baseline/pdr-vX.Y.Z` | `baseline/pdr-v0.9.0` |
| CDR | `baseline/cdr-vX.Y.Z` | Planned |

---

## 12. Documentation Management

### 12.1 Documentation Location

All project documentation resides in `docs/ecss/`, organized by ECSS milestone:

```
docs/ecss/
  requirements/    ← MRR/SRR requirements documents
  design/          ← PDR/CDR design documents
  verification/    ← RTM, STP, SVVP, test reports
  standards/       ← coding standards, MISRA deviations
```

### 12.2 Document Lifecycle

Documents follow ECSS document status:

| Status | Meaning |
|---|---|
| Draft | Under development, not reviewed |
| Under Review | PR open, review in progress |
| Approved | PR merged to dev/main; baseline established |
| Superseded | Replaced by a newer version |

### 12.3 Document ID Convention

```
<subsystem>-<type>-<number>
  subsystem : SDP, SVVP, CMP, RMP, ICD, SAD, *-DES, ...
  type      : OBC (on-board computer)
  number    : 001, 002, ...
```

### 12.4 versions

Document versions follow `M.m` where:
- M increments on major restructuring or review gate approval
- m increments on content additions or corrections

---

## 13. Open Items

| ID | Description | Priority | Owner | Status |
|---|---|---|---|---|
| OI-1 | Define formal entry/exit criteria for TRR and AR/QR gates | Medium | PM | Open |
| OI-2 | Formalize coverage target: current threshold informal (80%), needs SDP endorsement | Medium | SW Lead | Open |
| OI-3 | Hardware-in-loop test procedure not yet defined (planned Phase 6) | Low | HW/SW | Open |
