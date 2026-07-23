# Documentation Index — CubeSat OBC

**Project**: CubeSat On-Board Computer (RP2350 / Pico 2W)
**Standard**: ECSS-E-ST-40C, ECSS-E-ST-10C, ECSS-Q-ST-80C
**ECSS Cycle**: ✅ **COMPLETE** (SRR→PDR→CDR→TRR→AR→FRR — all 6 gates closed)

This index organizes all project documentation by ECSS review milestone.
Status legend: ✅ Exists and usable · ⚠️ Partial / no formal ECSS ID · ❌ Not yet created · ⏭️ Waived

---

## ECSS Milestone Documents (`docs/ecss/`)

### MRR — Mission Requirements Review

| Doc ID | Title | Status | File |
|---|---|---|---|
| MRD-OBC-001 | Mission Requirements Document / ConOps | ✅ v1.0 — Approved | [ecss/requirements/MRD-OBC-001.md](ecss/requirements/MRD-OBC-001.md) |
| SMP-OBC-001 | System Management Plan | ✅ v1.0 — Approved | [ecss/requirements/SMP-OBC-001.md](ecss/requirements/SMP-OBC-001.md) |
| PAP-OBC-001 | Product Assurance Plan | ✅ v1.0 — Approved | [ecss/requirements/PAP-OBC-001.md](ecss/requirements/PAP-OBC-001.md) |

---

### SRR — System Requirements Review

| Doc ID | Title | Status | File |
|---|---|---|---|
| SyRS-OBC-001 | System Requirements Specification | ✅ | [ecss/requirements/SyRS-OBC-001.md](ecss/requirements/SyRS-OBC-001.md) |
| SRS-OBC-001 | Software Requirements Specification | ✅ | [ecss/requirements/SRS-OBC-001.md](ecss/requirements/SRS-OBC-001.md) |
| SDP-OBC-001 | Software Development Plan | ✅ v1.0 — Approved | [ecss/requirements/SDP-OBC-001.md](ecss/requirements/SDP-OBC-001.md) |
| SVVP-OBC-001 | Software Verification & Validation Plan | ✅ v1.0 — Approved | [ecss/verification/SVVP-OBC-001.md](ecss/verification/SVVP-OBC-001.md) |
| CMP-OBC-001 | Configuration Management Plan | ✅ v1.0 — Approved | [ecss/requirements/CMP-OBC-001.md](ecss/requirements/CMP-OBC-001.md) |
| RMP-OBC-001 | Risk Management Plan | ✅ v1.0 — Approved | [ecss/requirements/RMP-OBC-001.md](ecss/requirements/RMP-OBC-001.md) |

---

### PDR — Preliminary Design Review ✅ CLOSED

| Doc ID | Title | Status | File |
|---|---|---|---|
| SAD-OBC-001 | System Architecture Document | ✅ v1.0 | [ecss/design/SAD-OBC-001.md](ecss/design/SAD-OBC-001.md) |
| ICD-OBC-001 | Interface Control Document | ✅ v1.1 | [ecss/design/ICD-OBC-001.md](ecss/design/ICD-OBC-001.md) |
| BOM-OBC-001 | Bill of Materials | ✅ v1.0.1 | [ecss/design/BOM-OBC-001.md](ecss/design/BOM-OBC-001.md) |
| ADCS-DES-001 | ADCS Design Document | ✅ v1.0 — PDR | [ecss/design/ADCS-DES-001.md](ecss/design/ADCS-DES-001.md) |
| FMM-DES-001 | Flight Mode Manager Design Document | ✅ v0.4 — Phase 7 updated | [ecss/design/FMM-DES-001.md](ecss/design/FMM-DES-001.md) |
| EPS-DES-001 | Electrical Power System Design Document | ✅ v0.1 — Draft | [ecss/design/EPS-DES-001.md](ecss/design/EPS-DES-001.md) |
| FAULT-DES-001 | Fault Manager Design Document | ✅ v0.2 — Draft | [ecss/design/FAULT-DES-001.md](ecss/design/FAULT-DES-001.md) |
| COMMS-DES-001 | Communications / TT&C Design Document | ✅ v0.1 — Draft | [ecss/design/COMMS-DES-001.md](ecss/design/COMMS-DES-001.md) |
| OBC-DES-001 | OBC Hardware & CDH Design Document | ✅ v0.1 — Draft | [ecss/design/OBC-DES-001.md](ecss/design/OBC-DES-001.md) |
| DL-DES-001 | Data Layer / Storage Design Document | ✅ v0.1 — Draft | [ecss/design/DL-DES-001.md](ecss/design/DL-DES-001.md) |
| RTM-OBC-001 | Requirements Traceability Matrix | ⚠️ | [ecss/verification/RTM-OBC-001.md](ecss/verification/RTM-OBC-001.md) |
| STP-OBC-001 / TEST-PLAN-001 | Software Test Plan | ⚠️ **TODO**: exists as `TST-001 v3.0` (321 lines) but uses informal ID and lacks ECSS-E-ST-40C §5.7 structure (no SVVP, entry/exit criteria, test levels). Needs formalisation. | [ecss/verification/STP-OBC-001.md](ecss/verification/STP-OBC-001.md) |
| ADCS-SIM-001 | ADCS Simulation & Verification Document | ✅ v1.0 — Draft | [ecss/design/ADCS-SIM-001.md](ecss/design/ADCS-SIM-001.md) |
| FMEA-OBC-001 | FMEA Preliminary (software faults) | ✅ v0.1 — Draft | [ecss/design/FMEA-OBC-001.md](ecss/design/FMEA-OBC-001.md) |

---

### CDR — Critical Design Review

| Doc ID | Title | Status | File |
|---|---|---|---|
| FSW-SDD-001 | Flight Software Design Description | ✅ v0.1 — Draft | [ecss/design/FSW-SDD-001.md](ecss/design/FSW-SDD-001.md) |
| POWER-BDG-001 | Power Budget | ✅ v0.3 — CDR Baseline + Phase 7 payload | [ecss/design/POWER-BDG-001.md](ecss/design/POWER-BDG-001.md) |
| LINK-BDG-001 | RF Link Budget | ✅ v0.2 — CDR Baseline | [ecss/design/LINK-BDG-001.md](ecss/design/LINK-BDG-001.md) |
| PAYLOAD-SPEC-001 | Scientific Payload Specification (PLS-001) | ✅ v0.1 — Phase 7 Draft | [ecss/design/PAYLOAD-SPEC-001.md](ecss/design/PAYLOAD-SPEC-001.md) |
| FMEA-OBC-001 | FMEA Preliminary (software faults) | ✅ v0.1 — Draft | [ecss/design/FMEA-OBC-001.md](ecss/design/FMEA-OBC-001.md) |
| FMEA-OBC-002 | FMEA Detailed (HW + SW) | ❌ | — |
| STS-OBC-001 | Software Test Specification | ❌ | — |

---

### TRR — Test Readiness Review

| Doc ID | Title | Status | File |
|---|---|---|---|
| ITP-OBC-001 | Integration Test Plan | ✅ v1.0 — Released | [ecss/test_plans/ITP-OBC-001.md](ecss/test_plans/ITP-OBC-001.md) |
| ATP-OBC-001 | Acceptance Test Procedure | ✅ v1.0 — Released | [ecss/test_plans/ATP-OBC-001.md](ecss/test_plans/ATP-OBC-001.md) |
| STR-OBC-001 | Software Test Report | ✅ v1.0 — Released | [ecss/test_plans/STR-OBC-001.md](ecss/test_plans/STR-OBC-001.md) |

---

### AR/QR — Acceptance / Qualification Review ✅ CLOSED (W-001)

| Doc ID | Title | Status | File |
|---|---|---|---|
| SCI-OBC-001 | Software Configuration Index | ✅ v1.0 — Released | [ecss/configuration/SCI-OBC-001.md](ecss/configuration/SCI-OBC-001.md) |
| AR-CRITERIA-001 | AR Gate Criteria | ✅ GO (W-001) | [ecss/reviews/AR-CRITERIA-001.md](ecss/reviews/AR-CRITERIA-001.md) |
| QUAL-OBC-001 | Qualification Test Procedure | ✅ v1.0 — Released | [ecss/test_plans/QUAL-OBC-001.md](ecss/test_plans/QUAL-OBC-001.md) |
| QTR-OBC-001 | Qualification Test Report | ⏭️ Waived (W-001) | — |
| NCR-LOG-001 | Non-Conformance Reports Log | ⏭️ Waived (W-001) | — |

---

### ORR/FRR — Operational / Flight Readiness Review ✅ CLOSED (W-002)

| Doc ID | Title | Status | File |
|---|---|---|---|
| FRR-GATE-001 | FRR Gate | ✅ N/A (W-002) | [ecss/reviews/FRR-GATE-001.md](ecss/reviews/FRR-GATE-001.md) |
| OPS-OBC-001 | Operations Manual | ⏭️ Not applicable (no flight mission) | — |
| FRR-OBC-001 | Flight Readiness Review Package | ⏭️ Not applicable (W-002) | — |

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

## Telemetry & Ground Station (`docs/telemetry/`)

> Telemetry protocol and ground station communication specifications.

| Document | File |
|---|---|
| Telemetry Protocol Specification | [telemetry/PROTOCOL.md](telemetry/PROTOCOL.md) |

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
| Phase 7 Plan — Payload Integration | [project/PHASE7_PAYLOAD_PLAN.md](project/PHASE7_PAYLOAD_PLAN.md) |

---

## Release Notes (`docs/release/`)

| Document | File |
|---|---|
| Release Notes (per version) | [release/RELEASE_NOTES.md](release/RELEASE_NOTES.md) |

---

## Private Documents (`docs/private/`)

Internal documents not for public distribution. Contents are excluded from this index.
