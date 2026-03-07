# SMP-OBC-001 — System Management Plan

| Field            | Value                                             |
|------------------|---------------------------------------------------|
| **Document ID**  | SMP-OBC-001                                       |
| **Title**        | System Management Plan                            |
| **Project**      | CubeSat OBC — RP2350 / Pico 2W                    |
| **Version**      | 1.0                                               |
| **Status**       | Approved — MRR Baseline                           |
| **Date**         | 2026-03-07                                        |
| **Author**       | OBC Project Team                                  |
| **Review Level** | MRR                                               |
| **Standard**     | ECSS-M-ST-10C (Project Planning), ECSS-M-ST-40C   |

---

## Change History

| Version | Date       | Author          | Description                         |
|---------|------------|-----------------|-------------------------------------|
| 1.0     | 2026-03-07 | OBC Project Team | Initial MRR baseline; formalisms project/ documents |

---

## Table of Contents

1. [Introduction](#1-introduction)
2. [Project Organization](#2-project-organization)
3. [Development Model and Lifecycle](#3-development-model-and-lifecycle)
4. [Review Schedule](#4-review-schedule)
5. [Configuration Management](#5-configuration-management)
6. [Branching and Release Policy](#6-branching-and-release-policy)
7. [Build and CI/CD](#7-build-and-cicd)
8. [Issue and Risk Management](#8-issue-and-risk-management)
9. [Documentation Management](#9-documentation-management)
10. [Open Items](#10-open-items)

---

## 1. Introduction

### 1.1 Purpose

This document defines the project management approach for the CubeSat OBC
software development. It covers organization, lifecycle model, review milestones,
configuration management, CI/CD, and documentation control.

### 1.2 Scope

This plan covers all software, documentation, and verification artifacts produced
within the CubeSat OBC project. Hardware procurement management is out of scope
(see `BOM-OBC-001`).

### 1.3 Document Identifier

`SMP-OBC-001 v1.0`

---

## 2. Project Organization

### 2.1 Team and Roles

| Role | Responsibilities |
|------|-----------------|
| Project Lead / Chief Engineer | Architecture decisions, review gate authority, final PR approval |
| Software Engineer | Implementation, unit tests, documentation |
| Verification Engineer | Test planning, RTM maintenance, coverage reporting |

> **Note**: For this project the team is a solo developer with mentor review.
> Role separation is maintained through pull request discipline and formal
> document reviews.

### 2.2 Communication

| Channel | Purpose | Frequency |
|---------|---------|-----------|
| GitHub Issues | Bug reports, feature requests, open items | As needed |
| GitHub Pull Requests | Code and documentation review | Per feature branch |
| CHANGELOG.md | Release notes and unbundled change record | Per PR merge |
| Project docs (`docs/project/`) | Phase plans, progress tracking | Per phase |

---

## 3. Development Model and Lifecycle

### 3.1 Lifecycle Model

The project follows an **incremental development model** with formal ECSS review
gates:

```
MRR → SRR → PDR → CDR → TRR → AR/QR
```

Each gate requires a set of baseline documents to be reviewed and approved before
proceeding.

### 3.2 Development Phases

| Phase | Status | Key Deliverable | Period |
|-------|--------|-----------------|--------|
| Phase 1 — Skeleton | ✅ Complete | FreeRTOS stubs, PID, 3 unit tests | Feb 2026 |
| Phase 2 — Pico SDK | ✅ Complete | I2C drivers, HW timing validation | Feb 2026 |
| Phase 3 — Communication | ✅ Complete | CSP, KISS framing, telemetry/command tasks | Feb–Mar 2026 |
| Spec Alignment | ✅ Complete | PRs 1–10; 17 tests, data layer, FMM, EPS | Mar 2026 |
| Phase 4 — Advanced Control | ✅ Complete | EKF, LQR, RK2; 19 tests | Mar 2026 |
| Phase 5 — Flight Readiness | ✅ Complete | Watchdog, momentum dump, magnetometer, EKF yaw; 23 tests | Mar 2026 |
| Phase 6 — Closed-Loop Stability | ✅ Complete | Quaternion, gain scheduling, closed-loop sim, 91.8% coverage; 29 tests | Mar 2026 |
| Documentation (MRR–CDR) | 🔄 In Progress | ECSS document set | Mar 2026 |
| Phase 7 — Qualification | Planned | Hardware-in-loop, thermal, vibration | Q3 2026 |

### 3.3 Iteration Rhythm

- Feature branches: `feature/<short-name>` from `dev`
- PR review per feature (self-review + mentor review)
- Merge to `dev` on PR pass; merge to `main` on release tag
- Documentation branches follow the same PR flow

---

## 4. Review Schedule

### 4.1 ECSS Review Gates

| Review | Milestone | Status | Key Entry Criteria |
|--------|-----------|--------|--------------------|
| MRR — Mission Requirements Review | MRD-OBC-001, SMP-OBC-001, PAP-OBC-001 | ✅ Current gate | Mission defined; ConOps documented |
| SRR — System Requirements Review | SyRS-OBC-001, SRS-OBC-001 | ✅ Complete | Requirements baselined in v1.0 post Phase 6 |
| PDR — Preliminary Design Review | SAD-OBC-001, ICD-OBC-001, BOM-OBC-001, ADCS-DES-001 | ✅ PASS | Architecture solid; RF correct; ADCS coherent |
| CDR — Critical Design Review | FMM-DES-001, EPS-DES-001, subsystem design docs | 🔄 In Progress | All subsystem designs complete |
| TRR — Test Readiness Review | STP-OBC-001, test reports | Planned | All tests defined; 90% pass rate |
| AR/QR — Acceptance / Qualification | STR-OBC-001 | Planned | Hardware qual complete |

### 4.2 Review Gate Criteria

Each review gate requires:
1. All gate-entry documents present at correct version.
2. All open items from previous gate closed or transferred with plan.
3. All unit tests for completed deliverables passing.
4. No MISRA required/mandatory violations (per `docs/standards/MISRA_DEVIATIONS.md`).

---

## 5. Configuration Management

### 5.1 Configuration Items

| Category | CI Type | Baseline |
|----------|---------|----------|
| Flight software source | Git commit SHA on `main` | Release tag `v0.x.0` |
| Test suite | Git commit SHA on `main` | Same as flight SW |
| ECSS documents | Git commit SHA on `main` | Per document version table |
| Build artifacts (`.uf2`) | `artifacts/` directory | Tagged release |
| Requirements | SyRS-OBC-001, SRS-OBC-001 | v1.0 baseline |

### 5.2 Versioning

| Artefact type | Scheme | Example |
|---------------|--------|---------|
| Software releases | Semantic Versioning (MAJOR.MINOR.PATCH) | `v0.7.0` |
| Documents | MAJOR.MINOR (Major = milestone, Minor = revision) | `v1.2` |
| Hotfixes | PATCH increment | `v0.7.1` |

### 5.3 Change Control

All changes to baselined artifacts require:
1. A feature branch (`feature/<name>` from `dev`).
2. A pull request with at least one reviewer approval.
3. CHANGELOG.md updated in the same PR.
4. For documents: version number and change history row updated.

---

## 6. Branching and Release Policy

```
main  ←── dev  ←── feature/<name>
              ↑               |
              └───────────────┘
                   PR merge
```

| Branch | Purpose | Protected |
|--------|---------|-----------|
| `main` | Stable, production-ready releases | Yes — merge only via PR |
| `dev`  | Active integration; all features merge here | Yes — merge only via PR |
| `feature/*` | Feature/document development; PR to `dev` | No |
| `hotfix/*` | Critical fixes branched from `main` | No |

Tags on `main`:
- `vMAJOR.MINOR.PATCH` — software release
- Document baselines tracked by Git history on `main`

---

## 7. Build and CI/CD

### 7.1 Build Targets

| Target | Command | Description |
|--------|---------|-------------|
| Host (Linux) | `cmake .. && cmake --build .` | Native test build; no hardware needed |
| Pico (ARM) | `cmake -DPICO_ENABLED=ON ..` | Cross-compiled `.uf2` firmware |
| Emulator | `cmake -DBUILD_EMU=ON ..` | QEMU/Wokwi emulation build |

### 7.2 CI Pipeline (GitHub Actions)

| Job | Trigger | Pass Criteria |
|-----|---------|---------------|
| `build-and-test` | Push / PR to `dev` or `main` | Build succeeds; all tests pass |
| `static-analysis` | Push / PR to `dev` or `main` | 0 cppcheck errors; 0 clang-tidy warnings |
| `coverage` | Push to `dev` | Line coverage ≥ 90% on `src/` |

### 7.3 Local Verification

```bash
# Build and test (host)
cd build && cmake .. -DCMAKE_BUILD_TYPE=Debug && cmake --build . && ctest --output-on-failure

# Static analysis
scripts/static_analysis.sh

# Coverage report
cd build && cmake .. -DCMAKE_BUILD_TYPE=Debug -DENABLE_COVERAGE=ON \
    && cmake --build . && ctest && gcovr --html-details coverage.html
```

---

## 8. Issue and Risk Management

### 8.1 Issue Tracking

All open items are tracked via:
- GitHub Issues (tagged `bug`, `enhancement`, `documentation`)
- Per-document OI tables (e.g., `FMM-DES-001 §17`, `EPS-DES-001 §17`)
- `CHANGELOG.md` unreleased section

### 8.2 Risk Register

| ID | Risk | Probability | Impact | Mitigation |
|----|------|-------------|--------|------------|
| R-1 | RP2350 SMP instability on dual-core | Medium | High | Single-core (`configNUMBER_OF_CORES=1`) until validated; SMP as Phase 2 item |
| R-2 | HC-12 Si4463 insufficient link budget for SSO | High | High | Replaced by E22-400M30S (30 dBm); +8.5 dB margin confirmed |
| R-3 | LIS3MDL driver migration (from discontinued HMC5883L) | Low | Medium | LIS3MDL selected as flight-grade; migration planned for Phase 7 |
| R-4 | MISRA compliance gaps in third-party code (FreeRTOS, libcsp) | Medium | Medium | Deviations documented in `docs/standards/MISRA_DEVIATIONS.md` |
| R-5 | Battery sizing insufficient for full 37-min eclipse | Low | High | EPS sized for 37 min; Schmidt-trigger sheds loads at CRITICAL |
| R-6 | Amateur frequency coordination delay | Medium | Medium | Start IARU coordination early; 435–438 MHz reserved band |

---

## 9. Documentation Management

### 9.1 Document Hierarchy

```
docs/
├── README.md                  ← Master index (ECSS milestone table)
├── ecss/
│   ├── requirements/          ← MRD, SyRS, SRS, test plans
│   ├── design/                ← SAD, ICD, BOM, *-DES-001 subsystem designs
│   ├── verification/          ← RTM, test reports
│   └── standards/             ← MISRA deviations, coding standards
├── architecture/              ← ARCHITECTURE.md, SYSTEM_DESIGN.md
├── dev/                       ← BUILD_GUIDE, CODING_STANDARDS, CONTRIBUTING
└── project/                   ← Phase plans, PROJECT_PROGRESS, LESSONS_LEARNED
```

### 9.2 Document Naming Convention

| Convention | Example |
|------------|---------|
| `<SUBSYSTEM>-<TYPE>-<NNN>` | `FMM-DES-001`, `EPS-DES-001` |
| `<TYPE>-OBC-<NNN>` for system-level | `SyRS-OBC-001`, `ICD-OBC-001` |

### 9.3 Document Lifecycle

`Draft → Internal Review → Approved (MRR/PDR/CDR gate) → Controlled`

Documents at "Approved" status require a formal change history entry and PR for
any modification.

---

## 10. Open Items

| OI | Description | Priority | Phase |
|----|-------------|----------|-------|
| OI-1 | Formalize STP-OBC-001 to ECSS-E-ST-40C §5.7 structure (currently informal TST-001) | High | CDR |
| OI-2 | Create FSW-SDD-001 (consolidated FSW design description cross-referencing subsystem DES docs) | High | CDR |
| OI-3 | Create ADCS-SIM-001 (simulation architecture, closed-loop verification results) | Medium | CDR |
| OI-4 | Define formal qualification test plan (thermal cycling, random vibration per GEVS) | Medium | TRR |
