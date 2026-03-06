# Documentation Index — CubeSat OBC

**Project**: CubeSat On-Board Computer (RP2350 / Pico 2W)
**Standard**: ECSS-E-ST-40C, ECSS-E-ST-10C, ECSS-Q-ST-80C

This index organizes all project documentation by ECSS review milestone.
Status legend: ✅ Exists and usable · ⚠️ Partial / no formal ECSS ID · ❌ Not yet created

---

## ECSS Milestone Documents (`docs/ecss/`)

### MRR — Mission Requirements Review

| Doc ID | Title | Status | File |
|---|---|---|---|
| MRD-OBC-001 | Mission Requirements Document / ConOps | ❌ | — |
| SMP-OBC-001 | System Management Plan | ⚠️ | see `docs/project/` |
| PAP-OBC-001 | Product Assurance Plan | ❌ | — |

---

### SRR — System Requirements Review

| Doc ID | Title | Status | File |
|---|---|---|---|
| SyRS-OBC-001 | System Requirements Specification | ✅ | [ecss/requirements/SyRS-OBC-001.md](ecss/requirements/SyRS-OBC-001.md) |
| SRS-OBC-001 | Software Requirements Specification | ✅ | [ecss/requirements/SRS-OBC-001.md](ecss/requirements/SRS-OBC-001.md) |
| SDP-OBC-001 | Software Development Plan | ❌ | — |
| SVVP-OBC-001 | Software Verification & Validation Plan | ❌ | — |
| CMP-OBC-001 | Configuration Management Plan | ❌ | — |
| RMP-OBC-001 | Risk Management Plan | ❌ | — |

---

### PDR — Preliminary Design Review *(current milestone)*

| Doc ID | Title | Status | File |
|---|---|---|---|
| SAD-OBC-001 | System Architecture Document | ✅ v1.0 | [ecss/design/SAD-OBC-001.md](ecss/design/SAD-OBC-001.md) |
| ICD-OBC-001 | Interface Control Document | ✅ v1.1 | [ecss/design/ICD-OBC-001.md](ecss/design/ICD-OBC-001.md) |
| BOM-OBC-001 | Bill of Materials | ✅ v1.0.1 | [ecss/design/BOM-OBC-001.md](ecss/design/BOM-OBC-001.md) |
| ADCS-DES-001 | ADCS Design Document | ✅ v1.0 — PDR | [ecss/design/ADCS-DES-001.md](ecss/design/ADCS-DES-001.md) |
| FMM-DES-001 | Flight Mode Manager Design Document | ✅ v0.1 — Draft | [ecss/design/FMM-DES-001.md](ecss/design/FMM-DES-001.md) |
| EPS-DES-001 | Electrical Power System Design Document | ❌ | — |
| COMMS-DES-001 | Communications / TT&C Design Document | ❌ | — |
| OBC-DES-001 | OBC Hardware & CDH Design Document | ❌ | — |
| DL-DES-001 | Data Layer / Storage Design Document | ❌ | — |
| RTM-OBC-001 | Requirements Traceability Matrix | ⚠️ | [ecss/verification/RTM-OBC-001.md](ecss/verification/RTM-OBC-001.md) |
| STP-OBC-001 | Software Test Plan (preliminary) | ⚠️ | [ecss/verification/STP-OBC-001.md](ecss/verification/STP-OBC-001.md) |
| FMEA-OBC-001 | FMEA Preliminary (software faults) | ❌ | — |

---

### CDR — Critical Design Review

| Doc ID | Title | Status | File |
|---|---|---|---|
| SDD-OBC-001 | Software Design Document (all modules) | ⚠️ covered by SAD + DES docs | — |
| POWER-BDG-001 | Power Budget | ❌ | — |
| LINK-BDG-001 | RF Link Budget | ❌ | — |
| FMEA-OBC-002 | FMEA Detailed (HW + SW) | ❌ | — |
| STS-OBC-001 | Software Test Specification | ❌ | — |

---

### TRR — Test Readiness Review

| Doc ID | Title | Status | File |
|---|---|---|---|
| ITP-OBC-001 | Integration Test Plan | ❌ | — |
| ATP-OBC-001 | Acceptance Test Procedure | ❌ | — |
| STR-OBC-001 | Software Test Report | ❌ | — |

---

### AR/QR — Acceptance / Qualification Review

| Doc ID | Title | Status | File |
|---|---|---|---|
| SCI-OBC-001 | Software Configuration Index | ❌ | — |
| QTR-OBC-001 | Qualification Test Report | ❌ | — |
| NCR-LOG-001 | Non-Conformance Reports Log | ❌ | — |

---

### ORR/FRR — Operational / Flight Readiness Review

| Doc ID | Title | Status | File |
|---|---|---|---|
| OPS-OBC-001 | Operations Manual | ❌ | — |
| FRR-OBC-001 | Flight Readiness Review Package | ❌ | — |

---

### Standards

| Document | File |
|---|---|
| Coding Standards (MISRA-like) | [ecss/standards/CODING_STANDARDS.md](ecss/standards/CODING_STANDARDS.md) |
| MISRA-C Deviations Log | [ecss/standards/MISRA_DEVIATIONS.md](ecss/standards/MISRA_DEVIATIONS.md) |

---

## Development Guides (`docs/dev/`)

> Internal guides for developers. ECSS equivalent: sections of SDP-OBC-001.

| Document | File |
|---|---|
| Build Guide (SDK setup, CMake, flashing) | [dev/BUILD_GUIDE.md](dev/BUILD_GUIDE.md) |
| Flashing Guide (UF2, SWD, picotool) | [dev/FLASHING_GUIDE.md](dev/FLASHING_GUIDE.md) |
| GitHub / Git Workflow Setup | [dev/GITHUB_SETUP.md](dev/GITHUB_SETUP.md) |
| Internal API / Interface Reference | [dev/INTERFACE_SPEC.md](dev/INTERFACE_SPEC.md) |

---

## Architecture References (`docs/architecture/`)

> Technical context documents. ECSS equivalent: input to SAD-OBC-001 and SDD-OBC-001.

| Document | File |
|---|---|
| System Architecture Overview | [architecture/ARCHITECTURE.md](architecture/ARCHITECTURE.md) |
| Detailed System Design (DES-001 v2.0) | [architecture/SYSTEM_DESIGN.md](architecture/SYSTEM_DESIGN.md) |

---

## Project Management (`docs/project/`)

> Informal planning and tracking documents. ECSS equivalent: SMP-OBC-001 inputs.

| Document | File |
|---|---|
| Project Progress & Roadmap | [project/PROJECT_PROGRESS.md](project/PROJECT_PROGRESS.md) |
| Lessons Learned | [project/LESSON_LEARNED.md](project/LESSON_LEARNED.md) |
| Phase 2 Plan | [project/PHASE2_PLAN.md](project/PHASE2_PLAN.md) |
| Phase 3 Plan | [project/PHASE3_PLAN.md](project/PHASE3_PLAN.md) |
| Phase 3 Communication Spec (input for COMMS-DES-001) | [project/PHASE3_COMM_SPEC.md](project/PHASE3_COMM_SPEC.md) |
| Phase 4 Plan | [project/PHASE4_PLAN.md](project/PHASE4_PLAN.md) |
| Phase 5 Plan | [project/PHASE5_PLAN.md](project/PHASE5_PLAN.md) |
| Phase 6 Plan | [project/PHASE6_PLAN.md](project/PHASE6_PLAN.md) |

---

## Release Notes (`docs/release/`)

| Document | File |
|---|---|
| Release Notes (per version) | [release/RELEASE_NOTES.md](release/RELEASE_NOTES.md) |

---

## Private Documents (`docs/private/`)

Internal documents not for public distribution. Contents are excluded from this index.
