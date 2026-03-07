# PAP-OBC-001 — Product Assurance Plan

| Field            | Value                                             |
|------------------|---------------------------------------------------|
| **Document ID**  | PAP-OBC-001                                       |
| **Title**        | Product Assurance Plan                            |
| **Project**      | CubeSat OBC — RP2350 / Pico 2W                    |
| **Version**      | 1.0                                               |
| **Status**       | Approved — MRR Baseline                           |
| **Date**         | 2026-03-07                                        |
| **Author**       | OBC Project Team                                  |
| **Review Level** | MRR                                               |
| **Standard**     | ECSS-Q-ST-80C (Software Product Assurance), ECSS-Q-ST-10C |

---

## Change History

| Version | Date       | Author          | Description                          |
|---------|------------|-----------------|--------------------------------------|
| 1.0     | 2026-03-07 | OBC Project Team | Initial MRR baseline                 |

---

## Table of Contents

1. [Introduction](#1-introduction)
2. [Software Quality Standards](#2-software-quality-standards)
3. [Coding Standards and Static Analysis](#3-coding-standards-and-static-analysis)
4. [MISRA Compliance](#4-misra-compliance)
5. [Test and Verification Policy](#5-test-and-verification-policy)
6. [Code Coverage Requirements](#6-code-coverage-requirements)
7. [Configuration and Version Control](#7-configuration-and-version-control)
8. [Documentation Assurance](#8-documentation-assurance)
9. [Non-Conformance Management](#9-non-conformance-management)
10. [Software Safety Classification](#10-software-safety-classification)
11. [Open Items](#11-open-items)

---

## 1. Introduction

### 1.1 Purpose

This Product Assurance Plan (PAP) defines the quality assurance activities,
standards compliance requirements, verification policy, and non-conformance
handling process for the CubeSat OBC flight software. It ensures that all
software deliverables meet the requirements of ECSS-Q-ST-80C and the project's
own quality gates.

### 1.2 Scope

Applies to all flight software source code, test code, build scripts, and
documentation produced in the CubeSat OBC project.

Third-party components (FreeRTOS kernel, libcsp, pico-sdk) are subject to the
derogation policy defined in §4.3.

### 1.3 Document Identifier

`PAP-OBC-001 v1.0`

---

## 2. Software Quality Standards

### 2.1 Applicable Standards

| Standard | Applies To | Current Status |
|----------|------------|----------------|
| ECSS-Q-ST-80C | Software product assurance | ✅ Compliance target |
| ECSS-E-ST-40C | Software engineering | ✅ Doc structure aligned |
| MISRA-C:2012 | Source code (C11) | ✅ 0 required/mandatory violations |
| IEC 61508 (SIL reference) | Task priorities, determinism | ✅ Compliant patterns used |
| NASA SWE-130 | Modular design, test automation | ✅ Applied |

### 2.2 Software Class

Per ECSS-Q-ST-80C Table 1, the OBC flight software is classified as:

| Attribute | Classification |
|-----------|---------------|
| Software class | **Class B** — non-critical (loss of mission possible but no safety risk to humans) |
| Rationale | 1U CubeSat; no human safety implications; potential mission loss if OBC fails |
| Consequence of failure | Loss of mission objectives; satellite becomes non-operational |

---

## 3. Coding Standards and Static Analysis

### 3.1 Language and Compiler

| Attribute | Value |
|-----------|-------|
| Language  | C11 (ISO/IEC 9899:2011) |
| Compiler  | `arm-none-eabi-gcc` (flight); `gcc` (host tests) |
| Warning flags | `-Wall -Wextra -pedantic -Werror` |
| C++ | Not permitted in flight code |

### 3.2 Formatting

All source files shall be formatted with `clang-format-14` using the project
`.clang-format` configuration. CI enforces format compliance on every PR.

### 3.3 Static Analysis Tools

| Tool | Purpose | Pass Criteria |
|------|---------|---------------|
| `cppcheck --addon=misra` | MISRA-C:2012 compliance check | 0 required, 0 mandatory violations |
| `clang-tidy` | C++ lint rules applied to C (bounds, concurrency) | 0 warnings on flight code |
| `clang-format-14` | Format compliance | No diff after formatting |

Static analysis is run on every PR via `scripts/static_analysis.sh` and the
GitHub Actions `static-analysis` job.

### 3.4 Coding Rules Summary

Key rules from `docs/ecss/standards/CODING_STANDARDS.md`:

| Rule | Description |
|------|-------------|
| No dynamic memory in flight paths | `malloc`/`free` prohibited after `system_init()` |
| All public functions documented | Doxygen header required; parameter pre/post-conditions stated |
| Weak-symbol HAL pattern | Hardware access via `__attribute__((weak))` stubs; overridden per target |
| Defensive null-checks at system boundaries | User input and external APIs validated at entry point |
| No direct register access | All hardware through HAL functions |
| Consistent naming | `module_verb_noun()` convention |

---

## 4. MISRA Compliance

### 4.1 Current Status

Reference: `docs/ecss/standards/MISRA_DEVIATIONS.md`

| Category | Violations |
|----------|------------|
| Required | **0** |
| Mandatory | **0** |
| Advisory (documented deviations) | 12 permitted deviations (D1–D12) |

No required or mandatory MISRA-C:2012 violations are present in the flight
software as of the Phase 6 baseline.

### 4.2 Permitted Advisory Deviations

Advisory deviations are accepted and documented. Major categories:

| Code | Rule | Rationale |
|------|------|-----------|
| D1–D5 | Rule 21.6 — `stdio` in `comm_init.c` | Required for USB CDC debug output; not used in flight paths |
| D6 | Rule 15.5 — single exit point | Industry standard code; not safety-critical exit paths |
| D7 | Rule 12.3 — comma operator | Limited to `for` loop initialisers (idiomatic C) |
| D8 | Rule 8.9 — storage class specifier | Static constants; advisory only |
| D9 | Rule 5.9 — inner scope identifier | Unambiguous; local scope only |

Full deviation justifications are in `docs/ecss/standards/MISRA_DEVIATIONS.md`.

### 4.3 Third-Party Code Derogation

Third-party components (FreeRTOS, libcsp, pico-sdk) are not assessed for
MISRA compliance. They are treated as:

- Isolated behind well-defined interfaces
- Unchanged from upstream releases (no modification of third-party source)
- Covered by their own upstream quality processes

MISRA analysis is configured to exclude `third_party/` and `pico-sdk/` directories.

---

## 5. Test and Verification Policy

### 5.1 Test Levels

| Level | Description | Tools |
|-------|-------------|-------|
| Unit tests | Per-module isolation tests | CTest + custom C test harness |
| Integration tests | Cross-module interaction (e.g., EKF → LQR closed loop) | CTest |
| System tests | End-to-end on host build | CTest |
| Qualification tests | Hardware-in-loop, thermal, vibration | Phase 2/3 |

### 5.2 Test Requirements

| Requirement | Value |
|-------------|-------|
| All unit tests must pass before PR merge | Mandatory |
| Test names shall follow `T-<MODULE>-<NN>` convention | Mandatory |
| Each requirement in SyRS-OBC-001 shall have ≥ 1 test | Mandatory (tracked in RTM-OBC-001) |
| Tests shall be deterministic and independent (no inter-test state) | Mandatory |
| Host tests shall run without physical hardware | Mandatory |

### 5.3 Current Test Status

| Suite | Tests | Status |
|-------|-------|--------|
| Unit tests | 27 | ✅ 27/27 passing |
| Integration tests | 2 | ✅ 2/2 passing |
| System tests | 0 | Planned (TRR) |
| **Total** | **29** | **29/29 passing** |

```bash
# Run full test suite
cd build && ctest --output-on-failure
```

---

## 6. Code Coverage Requirements

### 6.1 Coverage Target

| Scope | Minimum Target | Current (Phase 6) |
|-------|---------------|-------------------|
| `src/control/` | ≥ 90% line | 91.8% |
| `src/core/`    | ≥ 90% line | 91.8% |
| `src/services/`| ≥ 90% line | 91.8% |
| Overall        | ≥ 90% line | 91.8% ✅ |

### 6.2 Coverage Measurement

Coverage is measured with `gcov`/`gcovr` on the host build with
`-DENABLE_COVERAGE=ON`:

```bash
cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug -DENABLE_COVERAGE=ON
cmake --build .
ctest
gcovr --root .. --filter "src/" --html-details coverage/index.html
```

### 6.3 Coverage Exclusions

The following are explicitly excluded from coverage measurement:

| Exclusion | Rationale |
|-----------|-----------|
| `third_party/` | Third-party code, not OBC source |
| `pico-sdk/` | SDK code, not OBC source |
| `src/drivers/pico/` | Hardware-only paths, untestable on host |
| `__attribute__((weak))` HAL stubs | Replaced by test overrides; default path untestable by design |

---

## 7. Configuration and Version Control

### 7.1 Repository

| Attribute | Value |
|-----------|-------|
| Platform | GitHub — `LucasMed/cubesat-obc` |
| Primary branch | `main` (stable releases) |
| Integration branch | `dev` (active development) |
| Feature branches | `feature/<name>` |

### 7.2 Commit and PR Requirements

| Requirement | Enforcement |
|-------------|-------------|
| All commits must have descriptive messages (Conventional Commits format) | PR reviewer check |
| No direct push to `main` or `dev` | GitHub branch protection |
| Every PR must include CHANGELOG.md update | PR checklist |
| All CI checks must pass before merge | GitHub CI required status checks |
| No secrets, credentials, or API keys in commits | Pre-commit hook + PR review |

### 7.3 Release Tagging

Releases are tagged on `main` as `vMAJOR.MINOR.PATCH`. The `.uf2` artefact
is committed to `artifacts/` on tagged releases.

---

## 8. Documentation Assurance

### 8.1 Document Control Requirements

| Requirement | Description |
|-------------|-------------|
| All ECSS deliverable documents shall have a unique ID and version | Enforced by naming convention (§9.2 of SMP-OBC-001) |
| Documents shall include a change history table | Required in every ECSS document |
| Document status shall be one of: Draft / Approved / Controlled | Status field in document header |
| Approved documents require a PR for any change | Git history provides full audit trail |

### 8.2 Traceability

Requirements traceability (MIS → SYS → SW → Test) is maintained in
`docs/ecss/verification/RTM-OBC-001.md`. The RTM shall be updated with every
new requirement or test added.

### 8.3 Review Records

PR descriptions serve as formal review records. For ECSS review gate documents,
the PR body shall include:
- Document revision summary
- Checklist verification
- Open items list

---

## 9. Non-Conformance Management

### 9.1 Definition

A non-conformance (NC) is any deviation from a stated requirement, coding rule,
or test pass criterion that is not covered by an approved deviation record.

### 9.2 Process

```
NC detected
    │
    ├─ Minor (advisory MISRA, style) → create deviation record (D-xxx)
    │         → document in MISRA_DEVIATIONS.md → close
    │
    └─ Major (required/mandatory violation, failing test, broken interface)
              → create GitHub Issue tagged `non-conformance`
              → root-cause analysis
              → fix in feature branch PR
              → verify fix closes issue
              → update RTM if requirement affected
```

### 9.3 Current Open Non-Conformance

None. All MISRA required/mandatory violations resolved at Phase 6 baseline.

---

## 10. Software Safety Classification

### 10.1 Safety Assessment

The OBC software controls no safety-critical human systems. Failure modes are
bounded to:

| Failure | Consequence | Mitigation |
|---------|-------------|------------|
| Attitude control failure | Loss of pointing; mission objective failure | FM_SAFE entry; MTQ-only fallback |
| EPS failure | Battery depletion; loss of OBC | Load shedding; FM_SAFE; watchdog recovery |
| Communication failure | No ground contact for 1+ passes | Autonomous FDIR; scheduled re-attempt |
| Flash corruption | Loss of persistent log | Watchdog reset; CRC validation on read |

### 10.2 Fail-Safe States

The system is designed so that all failure paths lead to a defined safe state:

- Any `FAULT_LEVEL_CRITICAL` → `FM_SAFE` (minimal ops, TT&C listen)
- Watchdog timeout → hardware reset → `FM_BOOT` → `FM_SAFE`
- EPS EMERGENCY → load shed + `FM_SAFE`

There is no undefined-behavior exit from the state machine.

---

## 11. Open Items

| OI | Description | Priority | Phase |
|----|-------------|----------|-------|
| OI-1 | Define formal FMEA (FMEA-OBC-001) for software fault modes at CDR | High | CDR |
| OI-2 | Establish formal radiation tolerance analysis (SEU rate at 600 km SSO) | Medium | TRR |
| OI-3 | Define qualification test procedure for thermal cycling and vibration (MIS-E-001/002) | Medium | TRR |
| OI-4 | Implement formal failure reporting log for ground analysis (distinct from debug logger) | Low | Phase 2 |
