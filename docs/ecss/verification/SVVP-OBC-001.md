# Software Verification and Validation Plan

**Document ID**: SVVP-OBC-001
**Version**: 1.0
**Date**: 2026-03-07
**Status**: Approved — SRR Baseline
**Standard**: ECSS-E-ST-40C §5.6, ECSS-E-ST-10-02C (Verification)
**Project**: CubeSat OBC Flight Software (RP2350 / Pico 2W)

---

## Table of Contents

1. [Introduction](#1-introduction)
2. [Applicable Documents](#2-applicable-documents)
3. [Acronyms and Definitions](#3-acronyms-and-definitions)
4. [Verification and Validation Strategy](#4-verification-and-validation-strategy)
5. [Verification Levels](#5-verification-levels)
6. [Test Environment](#6-test-environment)
7. [Unit Test Plan](#7-unit-test-plan)
8. [Integration Test Plan](#8-integration-test-plan)
9. [System Test Plan](#9-system-test-plan)
10. [Hardware-in-the-Loop (HIL) Test Plan](#10-hardware-in-the-loop-hil-test-plan)
11. [Static Analysis Plan](#11-static-analysis-plan)
12. [Coverage Requirements](#12-coverage-requirements)
13. [Regression Strategy](#13-regression-strategy)
14. [Verification Traceability](#14-verification-traceability)
15. [Entry and Exit Criteria](#15-entry-and-exit-criteria)
16. [Open Items](#16-open-items)

---

## 1. Introduction

### 1.1 Purpose

This Software Verification and Validation Plan (SVVP) defines the strategy,
methods, environments, and criteria for verifying and validating the CubeSat OBC
flight software against the requirements specified in SRS-OBC-001 and SyRS-OBC-001.

Verification answers: *"Are we building the software correctly?"*  
Validation answers: *"Are we building the correct software?"*

### 1.2 Scope

This document covers verification and validation of:

- All flight software modules (`src/`)
- FreeRTOS task interactions
- Hardware driver interfaces (stub and real)
- Service layer: FMM, EPS, Fault Manager, Logger, DLA, Communications

It does **not** cover:
- Ground software validation
- Third-party library validation (Pico SDK, FreeRTOS kernel, libcsp)
- Pico SDK bootloader / ROM

### 1.3 Relationship to Other Plans

| Plan | Relationship |
|---|---|
| SDP-OBC-001 | SDP governs development process; SVVP governs V&V activities within that process |
| STP-OBC-001 | STP is the operational test plan (test cases and procedures); SVVP is the governing plan |
| RTM-OBC-001 | RTM provides requirement ↔ test traceability; SVVP defines how traceability is maintained |

---

## 2. Applicable Documents

| ID | Title | Version |
|---|---|---|
| SyRS-OBC-001 | System Requirements Specification | 1.0 |
| SRS-OBC-001 | Software Requirements Specification | 2.0 |
| SDP-OBC-001 | Software Development Plan | 1.0 |
| RTM-OBC-001 | Requirements Traceability Matrix | — |
| STP-OBC-001 | Software Test Plan | 3.0 |
| CODING_STANDARDS.md | CubeSat OBC Coding Standards | 1.0 |
| ECSS-E-ST-40C | Software Engineering Standard | — |
| ECSS-E-ST-10-02C | Verification Standard | — |

---

## 3. Acronyms and Definitions

| Acronym | Definition |
|---|---|
| SVVP | Software Verification and Validation Plan |
| V&V | Verification and Validation |
| HIL | Hardware-in-the-Loop |
| SIL | Software-in-the-Loop |
| UTR | Unit Test Report |
| ITR | Integration Test Report |
| STR | System Test Report |
| RTM | Requirements Traceability Matrix |
| CI | Continuous Integration |
| LOC | Lines of Code |
| MC/DC | Modified Condition/Decision Coverage |
| FMM | Flight Mode Manager |
| FDIR | Fault Detection, Isolation and Recovery |
| EPS | Electrical Power System |
| DLA | Data Layer Abstraction |

---

## 4. Verification and Validation Strategy

### 4.1 Overall Approach

Verification is performed at four levels (Section 5) using a combination of:

| Method | ECSS Category | Applied To |
|---|---|---|
| Review / Inspection | Analysis | All design documents, code review via PR |
| Static analysis | Analysis | All `src/` C code; clang-tidy + cppcheck |
| Unit testing | Test | Individual modules and functions |
| Integration testing | Test | Multi-module interactions, FreeRTOS task cooperation |
| System testing | Test | Full firmware image on host emulation |
| HIL testing | Test | Physical RP2350 hardware with IMU/actuator stimuli |

### 4.2 Test-First Principle

New functionality requires unit tests **before or alongside** implementation (not
after). Pull Requests are blocked from merging if tests are missing for changed
modules.

### 4.3 Regression Policy

The full test suite (`ctest --output-on-failure`) is executed on every commit to
any branch in the CI pipeline. A new failure in a previously passing test is a
blocking defect requiring immediate resolution before further work on the branch.

### 4.4 Independence

For safety-critical modules (FMM `flight_mode_manager.c`, Fault Manager
`fault_manager.c`, EPS Monitor `eps_monitor.c`), test cases are reviewed
independently by a second engineer who did not write the implementation.

---

## 5. Verification Levels

### 5.1 Level 1 — Unit Test (UL-1)

| Property | Value |
|---|---|
| Target | Individual C modules / functions |
| Framework | Unity v2.x |
| Execution | `ctest --test-dir build --output-on-failure` |
| Isolation | Module under test + stubs for dependencies |
| Pass criteria | All assertions pass; no memory errors detected |

### 5.2 Level 2 — Integration Test (UL-2)

| Property | Value |
|---|---|
| Target | Interactions between 2+ modules or FreeRTOS tasks |
| Framework | Unity + FreeRTOS task scheduling on host |
| Execution | Dedicated `tests/integration/` (planned Phase 6) |
| Pass criteria | Inter-module contracts hold; task timing within spec |

### 5.3 Level 3 — System Test / SIL (UL-3)

| Property | Value |
|---|---|
| Target | Full firmware image behaviour on host emulator |
| Framework | Host build with emulated hardware stubs |
| Execution | `build_emu/` CMake configuration |
| Pass criteria | End-to-end scenarios: boot → NOMINAL, telemetry TX, mode transitions, fault injection |

### 5.4 Level 4 — Hardware-in-the-Loop (UL-4)

| Property | Value |
|---|---|
| Target | Full firmware on physical RP2350 hardware |
| Framework | Visual / scripted ground verification, UART capture |
| Execution | Manual / semi-automated procedure (ATP-OBC-001) |
| Pass criteria | Hardware functional requirements verified; no anomalies |

---

## 6. Test Environment

### 6.1 Host Environment

| Component | Specification |
|---|---|
| OS | Ubuntu 22.04 LTS |
| CMake | ≥ 3.13 |
| GCC | Ubuntu 22.04 default |
| Unity | `third_party/Unity/` (git submodule) |
| FreeRTOS stubs | Provided in `build/` host configuration |

### 6.2 CI Environment

| Component | Specification |
|---|---|
| Runner | `ubuntu-22.04` (GitHub Actions) |
| clang-format | 14 (pinned) |
| clang-tidy | 14 (pinned) |
| cppcheck | System default |

### 6.3 Hardware Environment (HIL)

| Component | Specification |
|---|---|
| Board | Raspberry Pi Pico 2W (RP2350) |
| RTOS | FreeRTOS `ARM_CM33_NTZ` |
| Comms interface | UART1 (GPIO8/9, 115200 baud) via USB-UART adapter |
| Debug interface | UART0 (GPIO0/1, USB CDC) |
| IMU | MPU-6050 (I2C, 400 kHz) |
| Flash tool | picotool or UF2 drag-and-drop |

---

## 7. Unit Test Plan

### 7.1 Existing Test Suites

The following unit test suites are implemented and passing as of the SRR baseline:

| Suite | File | Coverage | Tests | Status |
|---|---|---|---|---|
| test_pid | `tests/unit/test_pid.c` | FR-3 | 4 | ✅ 4/4 |
| test_dynamics | `tests/unit/test_dynamics.c` | FR-2, SYS-F-102 | 5 (T-DYN-01..05) | ✅ 5/5 |
| test_actuators | `tests/unit/test_actuators.c` | FR-5, FR-6 | 5 | ✅ 5/5 |
| test_ekf | `tests/unit/test_ekf.c` | SYS-F-101..105 | 5 (T-EKF-01..05) | ✅ 5/5 |
| test_fmm | `tests/unit/test_fmm.c` | FR-9, SYS-S-300..305 | 11 (T-FMM-01..11) | ✅ 11/11 |
| test_fault_manager | `tests/unit/test_fault_manager.c` | FR-10 | 12 (T-FM-01..12) | ✅ 12/12 |
| test_eps_monitor | `tests/unit/test_eps_monitor.c` | FR-11 | 12 (T-EPS-01..12) | ✅ 12/12 |
| test_logger | `tests/unit/test_logger.c` | FR-12 | 12 (T-LOG-01..03) | ✅ 12/12 |
| test_health_monitor | `tests/unit/test_health_monitor_task.c` | FR-8 | 3 (T-HM-01..03) | ✅ 3/3 |
| test_attitude_control | `tests/unit/test_attitude_control_task.c` | FR-4, SYS-F-200 | 4 | ✅ 4/4 |
| test_sensor_read | `tests/unit/test_sensor_read_task.c` | FR-1 | 4 | ✅ 4/4 |
| test_comm_init | `tests/unit/test_comm_init.c` | FR-7 comm init | 2 (T-COM-01..02) | ✅ 2/2 |
| test_telemetry | `tests/unit/test_telemetry.c` | FR-7 downlink | 6 (T-TLM-01..06) | ✅ 6/6 |
| test_command | `tests/unit/test_command.c` | FR-7 uplink | 5 (T-CMD-01..05) | ✅ 5/5 |
| test_tasks | `tests/unit/test_tasks.c` | General task tests | multiple | ✅ pass |
| test_dl | `tests/unit/test_dl.c` | DLA layer | multiple | ✅ pass |

**Total**: 29/29 tests passing at SRR baseline (`a9cb3b0`).

### 7.2 Planned Unit Test Additions

| Module | Target Suite | Planned Phase |
|---|---|---|
| `attitude_control.c` (closed-loop) | `test_attitude_control_full.c` | Phase 6 |
| `pico_usart.c` (UART driver) | `test_pico_usart.c` | Phase 6 (HIL) |
| `watchdog.c` | `test_watchdog.c` | Phase 6 |

### 7.3 Unit Test Conventions

- One test file per module under test
- Test IDs follow `T-<MODULE>-<NN>` (e.g., `T-FMM-01`)
- Each test: arrange → act → assert, no shared mutable state between cases
- Stubs provided for hardware interfaces (`mpu6050`, `pico_usart`, etc.)

---

## 8. Integration Test Plan

### 8.1 Scope

Integration tests verify inter-module contracts not captured at unit level:

| Scenario | Modules Involved | Priority |
|---|---|---|
| INT-01: FMM → Fault FDIR | `flight_mode_manager` + `fault_manager` | High |
| INT-02: EPS → FMM SAFE transition | `eps_monitor` + `flight_mode_manager` | High |
| INT-03: TelemetryTask → DLA snapshot | `telemetry_task` + `dla_snapshot` | Medium |
| INT-04: CommandTask → FMM mode change | `command_task` + `flight_mode_manager` | Medium |
| INT-05: Logger → ring buffer thread safety | `logger` (multi-task write) | Medium |

### 8.2 Implementation Status

Integration tests are **planned for Phase 6** (TRR preparation). Infrastructure
in `build_emu/` supports multi-task host execution. No integration test suite
exists at the SRR baseline.

---

## 9. System Test Plan

### 9.1 End-to-End Scenarios

| Scenario | Description | Environment |
|---|---|---|
| SYS-01: Normal boot sequence | BOOT → SAFE → NOMINAL mode progression | SIL (build_emu) |
| SYS-02: Telemetry downlink | 1 Hz HK packet sent to GS; content verified | SIL |
| SYS-03: Command uplink ECHO | CMD_ECHO loopback | SIL |
| SYS-04: CMD SET_MODE | FM transition via command | SIL |
| SYS-05: Fault injection → SAFE | Inject CRITICAL fault → FMM transitions to SAFE | SIL |
| SYS-06: EPS emergency | VBATT < emergency threshold → SAFE | SIL |
| SYS-07: Watchdog expiry | Task deadline miss triggers watchdog | HIL |

### 9.2 Implementation Status

System tests are **planned for Phase 6**. SIL infrastructure (`build_emu/`)
available but test scripts not yet written.

---

## 10. Hardware-in-the-Loop (HIL) Test Plan

### 10.1 HIL Test Requirements

HIL testing is required to verify:

- UART1 physical timing (115200 baud, KISS framing integrity)
- IMU I2C transaction correctness at 400 kHz
- FreeRTOS task jitter (control loop ≤ ±22 µs, validated Phase 2)
- Boot-to-operational time ≤ 5 s (NFR-7)
- Hardware watchdog response

### 10.2 HIL Test Procedure Reference

Detailed HIL test procedures are deferred to **ATP-OBC-001** (Acceptance Test
Procedure), a TRR deliverable. The HIL configuration is defined in Section 6.3.

---

## 11. Static Analysis Plan

### 11.1 Tools

| Tool | Version | Scope | Run Frequency |
|---|---|---|---|
| clang-format | 14 | All `src/*.c`, `include/*.h` | Every commit (CI) |
| clang-tidy | 14 | All `src/*.c` | Every PR (CI) |
| cppcheck | System default | All `src/*.c` | Every PR (CI) |

### 11.2 Formatting Policy

Code must pass `clang-format-14 --dry-run --Werror` against `.clang-format`
configuration. Formatting is enforced in CI and must be clean before PR merge.

### 11.3 Static Analysis Findings Policy

| Severity | Action |
|---|---|
| Error | Blocking — must be resolved before merge |
| Warning | Must be addressed or documented with rationale |
| Information | Reviewed; resolved if practical |

Known false positives are documented in `MISRA_DEVIATIONS.md`.

---

## 12. Coverage Requirements

### 12.1 Coverage Targets

| Level | Metric | Target | Current |
|---|---|---|---|
| Unit | Line coverage | ≥ 80% for all non-driver modules | TBD (CI coverage TBD) |
| Unit | Branch coverage | ≥ 70% | TBD |
| Safety-critical modules | Line + Branch | ≥ 90% (`fault_manager`, `flight_mode_manager`, `eps_monitor`) | TBD |

Note: Coverage collection is implemented in CI (`artifacts/coverage/`). Formal
threshold enforcement planned for Phase 5 (CDR entry criterion).

### 12.2 Exclusions

| Excluded Code | Rationale |
|---|---|
| `src/drivers/` (hardware drivers) | Requires HIL; cannot be exercised on host |
| `third_party/` | Third-party code; not in coverage scope |
| `build_pico/` Pico-only init code | Requires embedded target |

---

## 13. Regression Strategy

### 13.1 Continuous Regression

The full test suite is run on every push to any branch (Section 9.2, SDP-OBC-001).
A regression is any test that passes on the base branch and fails on the PR branch.

### 13.2 Regression Triage

A regression must be triaged within **1 working day**:
1. Determine root cause (code change vs. test fragility)
2. If code defect: fix before merging PR
3. If test fragility: update test with reasoning documented in PR description

### 13.3 Baseline Protection

The `dev` and `main` branches have CI required status checks. A failing CI
pipeline prevents merge regardless of review approvals.

---

## 14. Verification Traceability

Requirement-to-test traceability is maintained in `RTM-OBC-001`. The RTM maps:

```
Requirement (SRS/SyRS) → Implementation Module → Test Case → Pass/Fail Status
```

The RTM is updated when:
- A new requirement is added or modified
- A new test case is added
- A test result changes state

Test case IDs embedded in source code comments (`// Verifies: T-FMM-03`) and
in CI test output provide fine-grained traceability.

---

## 15. Entry and Exit Criteria

### 15.1 Unit Test Phase

| Gate | Entry Criteria | Exit Criteria |
|---|---|---|
| Unit testing begins | Module code complete, stub interfaces defined | 100% unit tests passing; line coverage ≥ 80% |
| PR merge | CI green; all tests pass | — |

### 15.2 Integration Test Phase

| Gate | Entry Criteria | Exit Criteria |
|---|---|---|
| Integration testing begins | All unit tests passing; FreeRTOS HAL stubs available on host | All INT-01..05 scenarios pass |

### 15.3 System Test Phase

| Gate | Entry Criteria | Exit Criteria |
|---|---|---|
| SIL system test | Integration tests passing; `build_emu/` stable | SYS-01..06 pass |
| HIL system test | SIL complete; hardware available; ATP-OBC-001 approved | SYS-07 and all ATP steps pass |

### 15.4 Review Gates

| Review | V&V Exit Condition |
|---|---|
| PDR | All unit tests passing; RTM populated; static analysis clean |
| CDR | Coverage targets met; SYS-01..06 planned and scripted |
| TRR | Integration and system tests passing; ATPs approved |
| AR/QR | HIL tests complete; STR-OBC-001 signed off |

---

## 16. Open Items

| ID | Description | Priority | Owner | Status |
|---|---|---|---|---|
| OI-1 | Formal coverage threshold not yet enforced in CI (80% target informal) | High | CI Lead | Open |
| OI-2 | Integration test suite (`tests/integration/`) not yet created | High | SW Lead | Open |
| OI-3 | SIL emulation scenarios (SYS-01..07) not yet scripted | Medium | SW Lead | Open |
| OI-4 | HIL test procedure (ATP-OBC-001) deferred to TRR | Low | HW/SW | Open |
| OI-5 | MC/DC coverage not yet collected (tool selection pending) | Low | SW Lead | Open |
