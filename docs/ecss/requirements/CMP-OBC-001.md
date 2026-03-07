# Configuration Management Plan

**Document ID**: CMP-OBC-001
**Version**: 1.0
**Date**: 2026-03-07
**Status**: Approved — SRR Baseline
**Standard**: ECSS-E-ST-40C §5.5, ECSS-M-ST-40C (Configuration Management)
**Project**: CubeSat OBC Flight Software (RP2350 / Pico 2W)

---

## Table of Contents

1. [Introduction](#1-introduction)
2. [Applicable Documents](#2-applicable-documents)
3. [Acronyms and Definitions](#3-acronyms-and-definitions)
4. [Configuration Items](#4-configuration-items)
5. [Version Control System](#5-version-control-system)
6. [Baseline Management](#6-baseline-management)
7. [Change Control Process](#7-change-control-process)
8. [Build Reproducibility](#8-build-reproducibility)
9. [Submodule Management](#9-submodule-management)
10. [Release and Artefact Management](#10-release-and-artefact-management)
11. [Configuration Audits](#11-configuration-audits)
12. [Open Items](#12-open-items)

---

## 1. Introduction

### 1.1 Purpose

This Configuration Management Plan (CMP) defines the procedures and controls for
identifying, controlling, recording, and auditing changes to all configuration
items (CIs) in the CubeSat OBC project. It ensures that the software baseline
used for testing and flight is known, reproducible, and traceable.

### 1.2 Scope

This plan covers all CIs under CM control:

- Flight software source code (`src/`, `include/`)
- Test software (`tests/`)
- Build system (`CMakeLists.txt`, `config/`, toolchain files)
- Third-party components (`third_party/`, `pico-sdk/` — managed as submodules)
- Configuration files (`FreeRTOSConfig.h`, `pico_pins.h`, `.clang-format`)
- ECSS documentation (`docs/ecss/`)
- CI pipeline definition (`.github/workflows/`)

### 1.3 CM Authority

The CM authority for this project is the Software Lead (or designated maintainer).
All baseline promotions and tag operations require CM authority approval.

---

## 2. Applicable Documents

| ID | Title | Version |
|---|---|---|
| SDP-OBC-001 | Software Development Plan | 1.0 |
| SyRS-OBC-001 | System Requirements Specification | 1.0 |
| CONTRIBUTING.md | Development Workflow and PR Checklist | — |
| CHANGELOG.md | Project Change Log | — |
| ECSS-E-ST-40C | Software Engineering Standard | — |
| ECSS-M-ST-40C | Configuration Management Standard | — |

---

## 3. Acronyms and Definitions

| Acronym | Definition |
|---|---|
| CMP | Configuration Management Plan |
| CI | Configuration Item |
| CM | Configuration Management |
| VCS | Version Control System |
| PR | Pull Request |
| SCI | Software Configuration Index |
| CCB | Change Control Board |
| SHA | Secure Hash Algorithm (Git commit hash) |
| UF2 | USB Flashing Format (Raspberry Pi) |

---

## 4. Configuration Items

### 4.1 Software CIs

| CI ID | Description | Location | Baseline Required |
|---|---|---|---|
| CI-SW-001 | Flight software source (`src/`) | `src/` | Yes |
| CI-SW-002 | Header files (`include/`) | `include/` | Yes |
| CI-SW-003 | Unit test source (`tests/`) | `tests/` | Yes |
| CI-SW-004 | Top-level CMake build system | `CMakeLists.txt`, `config/library.cmake` | Yes |
| CI-SW-005 | FreeRTOS configuration | `config/FreeRTOSConfig.h` | Yes |
| CI-SW-006 | Hardware pin configuration | `config/pico_pins.h` | Yes |
| CI-SW-007 | Clang-format configuration | `.clang-format` | Yes |
| CI-SW-008 | CI pipeline definition | `.github/workflows/ci.yml` | Yes |
| CI-SW-009 | Static analysis scripts | `scripts/static_analysis.sh`, `scripts/gen_pr_description.sh` | Yes |
| CI-SW-010 | Docker build environment | `docker/Dockerfile.pico` | Yes |

### 4.2 Documentation CIs

| CI ID | Description | Location | Baseline Required |
|---|---|---|---|
| CI-DOC-001 | ECSS requirements documents | `docs/ecss/requirements/` | Yes |
| CI-DOC-002 | ECSS design documents | `docs/ecss/design/` | Yes |
| CI-DOC-003 | ECSS verification documents | `docs/ecss/verification/` | Yes |
| CI-DOC-004 | ECSS standards | `docs/ecss/standards/` | Yes |
| CI-DOC-005 | CHANGELOG | `CHANGELOG.md` | Yes |
| CI-DOC-006 | Contributing guide | `CONTRIBUTING.md` | No |

### 4.3 Third-Party CIs (Submodules)

| CI ID | Component | Submodule Path | Version Control |
|---|---|---|---|
| CI-TP-001 | FreeRTOS Kernel | `third_party/FreeRTOS-Kernel/` | Git submodule (pinned SHA) |
| CI-TP-002 | Pico SDK | `pico-sdk/` | Git submodule (pinned SHA) |
| CI-TP-003 | libcsp (CubeSat Space Protocol) | `third_party/libcsp/` | Git submodule (pinned SHA) |
| CI-TP-004 | Unity test framework | `third_party/Unity/` | Git submodule (pinned SHA) |

Third-party submodule SHA hashes are committed in `.gitmodules` and tracked in
the Software Configuration Index (SCI-OBC-001, a future AR/QR deliverable).

---

## 5. Version Control System

### 5.1 Platform

| Property | Value |
|---|---|
| VCS | Git |
| Hosting | GitHub (`LucasMed/cubesat-obc`) |
| Default integration branch | `dev` |
| Stable release branch | `main` |

### 5.2 Repository Structure

```
main          ← tagged, stable releases
  └── dev     ← integration branch; CI-gated
        ├── feature/*   ← in-development features
        ├── fix/*        ← defect corrections
        ├── docs/*       ← documentation
        ├── test/*       ← test additions
        └── refactor/*  ← refactoring
```

Full branching strategy is defined in SDP-OBC-001 §7.

### 5.3 Commit Identification

Each commit is identified by its full SHA-1 hash. All references to specific
software states in test reports and defect logs shall include the full or
abbreviated (7-char minimum) Git SHA.

Example: `a9cb3b0` = SRR baseline for COMMS-DES-001 merge.

### 5.4 Protected Branches

| Branch | Protection |
|---|---|
| `main` | Requires PR + CI pass + CM authority approval |
| `dev` | Requires PR + CI pass + at least one review |

Direct pushes to `main` and `dev` are forbidden.

---

## 6. Baseline Management

### 6.1 Baseline Definition

A **baseline** is a formally approved configuration at a specific ECSS review gate,
identified by a Git tag on `main`.

### 6.2 Baseline Table

| Baseline | Tag | SHA | Date | Status |
|---|---|---|---|---|
| MRR Baseline | `baseline/mrr-v0.16.0` | (future) | Planned | Not yet tagged |
| **SRR Baseline** | `baseline/srr-v0.18.0` | `a9cb3b0` * | 2026-03-07 | **This document** |
| PDR Baseline | `baseline/pdr-vX.Y.Z` | — | Planned | Not yet reached |
| CDR Baseline | `baseline/cdr-vX.Y.Z` | — | Planned | Not yet reached |

\* SRR baseline SHA is the `dev` HEAD at which SRR planning documents are merged.
Exact SHA will be updated when this document's PR is merged.

### 6.3 Baseline Promotion Procedure

1. All SRR-required documents present, reviewed, and merged to `dev`
2. CI green on `dev` HEAD
3. CM authority creates PR from `dev` → `main`
4. PR reviewed and approved
5. Tag applied: `git tag -a baseline/srr-v<version> -m "SRR baseline — <description>"`
6. Tag pushed: `git push origin baseline/srr-v<version>`
7. SCI-OBC-001 snapshot generated (deferred to AR/QR for formal issue)

---

## 7. Change Control Process

### 7.1 Change Categories

| Category | Examples | Review Required |
|---|---|---|
| Class A — Major | Architecture change, new subsystem, ECSS document revision | CCB review + CM authority |
| Class B — Minor | New feature, new test suite, document clarification | PR review (min. 1 approver) |
| Class C — Administrative | Comment, formatting, typo fix | PR review (self-review acceptable) |

### 7.2 Change Workflow

```
Identify change need
        │
        ▼
Create feature/fix/docs branch from dev
        │
        ▼
Implement changes; update CHANGELOG.md
        │
        ▼
Run CI locally (cmake --build + ctest + static analysis)
        │
        ▼
Open Pull Request → CI runs automatically
        │
  ┌─────┴──────┐
  │ CI fails   │ CI passes
  │            │
  ▼            ▼
Fix issues   Code review (min. 1 approver)
              │
              ▼
           Merge to dev
              │
        (at review gate)
              ▼
           Merge dev → main + tag baseline
```

### 7.3 Emergency Changes

Emergency changes (critical defects in a released baseline) follow an accelerated
path:

1. Branch from the affected tag (not from `dev`)
2. Apply targeted fix
3. Review by CM authority (expedited)
4. Tag new patch version
5. Back-merge to `dev`

---

## 8. Build Reproducibility

### 8.1 Reproducibility Requirements

A given Git SHA shall always produce bit-identical binary artifacts when built
with the same toolchain version. The following practices enforce this:

| Practice | Mechanism |
|---|---|
| Pinned submodule SHAs | `.gitmodules` + `git submodule update --init --recursive` |
| Pinned clang-format version | `ubuntu-22.04` runner + `clang-format-14` (CI and Docker) |
| No internet access during build | All dependencies in submodules or system packages |
| Deterministic CMake configuration | No `file(GLOB_RECURSE)` for source files in production targets |

### 8.2 Build Environment Documentation

The canonical build environment is defined by:

- `docker/Dockerfile.pico` — for embedded (Pico) builds
- `.github/workflows/ci.yml` — for host test builds
- `docs/dev/BUILD_GUIDE.md` — for local developer setup

---

## 9. Submodule Management

### 9.1 Policy

Third-party libraries are managed exclusively as Git submodules at pinned SHAs.
No vendor code may be copied directly into the project tree.

### 9.2 Update Procedure

To update a submodule:

1. Review the target library changelog for breaking changes
2. Create a `chore/update-<library>` branch
3. Update submodule SHA: `git -C third_party/<lib> checkout <new-tag>`
4. Stage: `git add third_party/<lib>`
5. Commit: `git commit -m "chore(deps): update <library> to <version>"`
6. Run full test suite — all tests must pass
7. PR → `dev` with description of library changes

### 9.3 Patch Management

Local patches (in `patches/`) are applied via `scripts/apply_patches.sh` at
container/dev-environment initializations. Patches must be regenerated after
submodule updates.

---

## 10. Release and Artefact Management

### 10.1 Release Artifacts

| Artefact | Description | Retained |
|---|---|---|
| `cubesat_obc_pico.uf2` | Flash-ready firmware for RP2350 | Yes (GitHub Release) |
| `obc_full_test.uf2` | Full integration test build | Yes (GitHub Release) |
| Test results (`artifacts/test_results/`) | CTest XML output | Yes (CI artefact) |
| Coverage HTML (`artifacts/coverage/`) | HTML coverage report | Yes (CI artefact) |

### 10.2 Release Naming

Release tags follow: `v<MAJOR>.<MINOR>.<PATCH>`

Pre-release tags: `v<MAJOR>.<MINOR>.<PATCH>-rc.<N>`

### 10.3 Artefact Retention

- GitHub Release assets: indefinite
- CI artifacts: 90 days (configurable in `.github/workflows/ci.yml`)
- Local build directories (`build/`, `build_pico/`): not retained (reproducible
  from source)

---

## 11. Configuration Audits

### 11.1 Functional Configuration Audit (FCA)

Performed before each review gate. Verifies:

- All CIs are correctly identified and at the expected version
- CHANGELOG.md reflects all changes since the last baseline
- All test results are current and traceable to requirements in RTM-OBC-001
- No unresolved critical open items in design documents

### 11.2 Physical Configuration Audit (PCA)

Performed before AR/QR (flight delivery). Verifies:

- Build is reproducible from the tagged SHA
- SCI-OBC-001 accurately lists all third-party SHAs and licenses
- `.uf2` artefact SHA-256 matches the CI-produced artefact
- No unreleased or unstaged changes in the build environment

### 11.3 Audit Schedule

| Audit | Review Gate | Responsibility |
|---|---|---|
| Mini-FCA (checklist) | Every PR merge | PR author + reviewer |
| Full FCA | SRR, PDR, CDR | CM authority |
| PCA | AR/QR | CM authority + HW lead |

---

## 12. Open Items

| ID | Description | Priority | Owner | Status |
|---|---|---|---|---|
| OI-1 | SCI-OBC-001 not yet created (deferred to AR/QR) | Low | CM authority | Open |
| OI-2 | Baseline tags for MRR and SRR not yet applied to `main` (pending first dev→main PR) | High | CM authority | Open |
| OI-3 | `artifacts/` directory in repo tracks pre-built UF2s — should be migrated to GitHub Releases only | Medium | Dev Lead | Open |
| OI-4 | CI artefact retention currently at system default — 90-day limit not yet configured | Low | CI Lead | Open |
