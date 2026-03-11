# PDR-OBC-001 — Preliminary Design Review Report
## CubeSat OBC Flight Software — RP2350 / Pico 2W

| Field            | Value                                             |
|------------------|---------------------------------------------------|
| **Document ID**  | PDR-OBC-001                                       |
| **Title**        | Preliminary Design Review Report                   |
| **Project**      | CubeSat OBC — RP2350 / Pico 2W                    |
| **Version**      | 1.0                                               |
| **Status**       | **CLOSED** — All Actions Resolved                 |
| **Review Date**  | 2026-03-10                                        |
| **Reviewer**     | OBC Systems Review Board                          |
| **Review Level** | PDR                                               |
| **Standard**     | ECSS-E-ST-10-02C, ECSS-E-ST-40C, ECSS-Q-ST-80C, ECSS-M-ST-10C |

---

## Change History

| Version | Date       | Author               | Description              |
|---------|------------|----------------------|--------------------------|
| 1.0     | 2026-03-10 | OBC Systems Review Board | Initial PDR issue     |

---

## Table of Contents

1. [Review Summary](#1-review-summary)
2. [Major Architecture Risks](#2-major-architecture-risks)
3. [Minor Design Improvements](#3-minor-design-improvements)
4. [System Architecture Analysis](#4-system-architecture-analysis)
5. [Subsystem Interface Review](#5-subsystem-interface-review)
6. [Data Flow Analysis](#6-data-flow-analysis)
7. [Integration Risk Analysis](#7-integration-risk-analysis)
8. [Verification Strategy](#8-verification-strategy)
9. [ECSS Checklist Result](#9-ecss-checklist-result)
10. [Technical Scoring](#10-technical-scoring)
11. [Flight Readiness Assessment](#11-flight-readiness-assessment)
12. [Recommended Actions](#12-recommended-actions)## 1. Review Summary

This Preliminary Design Review (PDR) evaluated the CubeSat OBC architecture based on the Pico 2W / RP2350 platform and FreeRTOS. The review tracked the progression from preliminary drafts to the final PDR baseline.

---

## 2. Initial Findings (Review Entry)

At the start of the PDR (2026-03-09), several architectural gaps and risks were identified.

### 2.1 Technical Gaps (Initial)
- **Interface Definition**: Subsystem interfaces (ADCS, EPS, COMMS) were identified as high-level but lacking bit-level or register-level detail.
- **Data Flow**: Telemetry acquisition and telecommand validation flows were not formally diagrammed in the system documentation.
- **Resource Constraints**: Discrepancies between heap allocation (60 KB) and estimated stack usage (~82 KB) for the hardware build.
- **Hardware Integration**: I2C pin configuration was inconsistent across header files (`config.h` vs. `pico_pins.h`).

### 2.2 Initial Architecture Risks
- Potential for task priority inversion or starvation without formal timing budget.
- Dependency on external libraries (libcsp) without confirmed host/hardware compatibility fixes.
- Absence of flash storage backend for mission logs.

---

## 3. Resolved Findings (Review Closure)

All technical gaps identified in Section 2 were resolved during the review period.

| RID | Finding | Resolution | Evidence |
|:---:|---------|------------|----------|
| 1 | Incomplete Interfaces | Bit-level definitions for all subsystems added. | `ICD-OBC-001 v1.2` |
| 2 | Missing Data Flows | Command/Telemetry sequence diagrams added. | `SAD-OBC-001 v1.1` |
| 3 | Heap Size Conflict | Heap increased to 128 KB; stacks verified. | `FSW-SDD-001 v0.3` |
| 4 | Pin/HW Inconsistency | I2C pins harmonized to 4/5 (SDA/SCL). | `config.h` (Branch fix) |
| 5 | EKF Representation | Migrated Euler to 7-state Quaternion baseline. | `ADCS-DES-001 v1.1` |
| 6 | Flash Backend | Implemented 4-sector round-robin backend. | `flash_backend.c` |

---

## 4. Final Assessment

### 4.1 Technical Scoring (Initial vs. Final)

| Aspect                      | Initial | Final | Trend |
|-----------------------------|:-------:|:-----:|:-----:|
| Architectural clarity       |    6    |   9   |  ▲    |
| Interface definition        |    5    |   10  |  ▲    |
| Fault robustness            |    4    |   8   |  ▲    |
| Documentation               |    5    |   9   |  ▲    |
| Integration readiness       |    5    |   9   |  ▲    |

### 4.2 Decisión de la Review: **PASS**

The design is now fully baseline-aligned. Traceability to requirements is complete, and major architecture risks have been mitigated by implementation evidence and documentation closure.

---

## 5. Closure Actions

1. RTM-OBC-001 fully links requirements to SW modules. [**RESOLVED**]
2. CPU budget table is added to SDD §7. [**RESOLVED**]
3. Updated documents are baselined in Git tag `PDR_BASELINE` (Commit: `fix/doc-alignment-pdr`). [**READY**]
**READY**]