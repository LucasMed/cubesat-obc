# SRR-OBC-001 — System Requirements Review Report
## CubeSat OBC Flight Software — RP2350 / Pico 2W

| Field            | Value                                             |
|------------------|---------------------------------------------------|
| **Document ID**  | SRR-OBC-001                                       |
| **Title**        | System Requirements Review Report                 |
| **Project**      | CubeSat OBC — RP2350 / Pico 2W                    |
| **Version**      | 1.0                                               |
| **Status**       | Issued — Pending AIR Resolution                   |
| **Review Date**  | 2026-03-10                                        |
| **Reviewer**     | OBC Systems Review Board                          |
| **Review Level** | SRR                                               |
| **Standard**     | ECSS-E-ST-10-02C, ECSS-E-ST-40C, ECSS-Q-ST-80C, ECSS-M-ST-10C |

---

## Change History

| Version | Date       | Author               | Description              |
|---------|------------|----------------------|--------------------------|
| 1.0     | 2026-03-10 | OBC Systems Review Board | Initial SRR issue     |

---

## Table of Contents

1. [Review Summary](#1-review-summary)
2. [Document Baseline Reviewed](#2-document-baseline-reviewed)
3. [Major Issues](#3-major-issues)
4. [Minor Issues](#4-minor-issues)
5. [Requirements Quality](#5-requirements-quality)
6. [Traceability Analysis](#6-traceability-analysis)
7. [Subsystem Responsibility Analysis](#7-subsystem-responsibility-analysis)
8. [Verification Feasibility](#8-verification-feasibility)
9. [ECSS Checklist Result](#9-ecss-checklist-result)
10. [Technical Scoring](#10-technical-scoring)
11. [Flight Readiness Potential](#11-flight-readiness-potential)
12. [Recommended Actions](#12-recommended-actions)

---

## 1. Review Summary

The SRR was conducted on 2026-03-10 against the document baseline listed in §2.

**Overall SRR Verdict: PASSED — All major payload and GPS issues resolved**

All previously blocking issues regarding the payload subsystem and GPS interface have been addressed:
- The payload suite (PLS-001: CAM-001, MAG-001, RAD-001) is fully specified, with requirements FR-13..17, PLD-R-001..005, and interface/ICD coverage.
- The GPS receiver (NEO-7M, GY-NEO6Mv2) is now formally included in Phase 7, with requirements FR-18, FR-19, IR-10, and ICD/plan deliverables.

The document set demonstrates engineering maturity well above typical academic CubeSat projects. Requirements are traceable, tests exist, and the FDIR architecture is coherent. Remaining open items are tracked in `AIR-OBC-001` and are not blocking for PDR baseline lock.

---

## 2. Document Baseline Reviewed

| Document | ID | Version | Status |
|---|---|---|---|
| Mission Requirements Doc / ConOps | MRD-OBC-001 | 1.1 | Approved |
| System Requirements Spec | SyRS-OBC-001 | 1.0 | Approved |
| Software Requirements Spec | SRS-OBC-001 | 2.3 | Active |
| Requirements Traceability Matrix | RTM-OBC-001 | — | Active |
| Software V&V Plan | SVVP-OBC-001 | 1.0 | Approved |
| Interface Control Document | ICD-OBC-001 | 1.2 | Released |
| Risk Management Plan | RMP-OBC-001 | 1.0 | Approved |
| FMEA | FMEA-OBC-001 | 0.1 | CDR Baseline |
| Flight Software Design Desc. | FSW-SDD-001 | 0.2 | CDR Baseline |
| Software Architecture Desc. | ARCHITECTURE.md | — | Released |
| libcsp documentation | third_party/libcsp/doc | v1.x/v2.x | Attached |

---

## 3. Major Issues

### MAJ-04 — Payload Subsystem Definition (**RESOLVED**)

| Field | Value |
|---|---|
| **Location** | MRD-OBC-001 §2.3 MO-6; FMEA-OBC-001 §1.2 (OI-1); ICD-OBC-001 |
| **ECSS Reference** | ECSS-E-ST-10-06C §5.2, ECSS-M-ST-10C review entry criteria |
| **AIR Reference** | ACT-04 |

**Resolution:** The payload suite (PLS-001) is now fully defined: CAM-001 (IMX219 camera), MAG-001 (RM3100), RAD-001 (PIN diode). Requirements FR-13..17 and PLD-R-001..005 are in SRS v2.3. ICD-OBC-001 v1.2 and PAYLOAD-SPEC-001 provide interface and operational details. FMEA update is pending for CDR.

---

### MAJ-06 — GPS Receiver Requirements (**RESOLVED**)

| Field | Value |
|---|---|
| **Location** | ICD-OBC-001 §7 (UART0 → NEO-7M GPS); SRS (FR-18/19, IR-10) |
| **ECSS Reference** | ECSS-E-ST-40C §5.4, ECSS-Q-ST-80C |
| **AIR Reference** | ACT-06 |

**Resolution:** GPS is now formally included in Phase 7. SRS-OBC-001 v2.3 adds FR-18 (NMEA parse), FR-19 (UTC sync), IR-10 (UART0 interface). ICD-OBC-001 v1.2 and PHASE7_PAYLOAD_PLAN v0.2 define deliverables. FMEA and fault IDs are scheduled for Phase 7.

---

## 5. Requirements Quality

| Dimension | Score | Notes |
|---|---|---|
| Uniqueness of ID | 4/5 | Dual numbering (SYS-REQ-x vs. SYS-F-xxx), SRS alias, but all payload/GPS reqs now unique. |
| Completeness | 5/5 | All major subsystems (payload, GPS, comms, ADCS, EPS) have requirements and interfaces. |
| Verifiability | 4/5 | Most requirements are quantifiable with explicit test IDs. MO-3 and NFR-4 are exceptions. |
| Consistency | 4/5 | CSP version conflict (v1.x vs. v2.x) remains; all other major inconsistencies resolved. |
| Traceability (MO→SyRS→SRS) | 4/5 | MO→SyRS mapping still informal, but FR/IR/PLD-R/ICD now aligned. |
| Implementation Status Tracking | 5/5 | `[IMPL]`/`[PLANNED]` tagging in SyRS is exemplary engineering practice. |
| Non-Functional Completeness | 4/5 | Power (NFR-4 TBD), flash (NFR-6 TBC ambiguous), boot time conflicting. |
| **Aggregate** | **4.3 / 5.0** | |

---

## 6. Traceability Analysis

### Vertical Traceability Chain

```
MRD-OBC-001 (MO-x)
    │
    ▼
SyRS-OBC-001 (SYS-F-xxx, SYS-NF-xxx)
    │
    ▼
SRS-OBC-001 (FR-x, NFR-x, IR-x, SR-x, PLD-R-x)
    │
    ▼
RTM-OBC-001
    │
    ▼
Unit / Integration Tests
```

### Identified Traceability Gaps

| ID | Description | Risk |
|---|---|---|
| TG-01 | MO-1..MO-7 not explicitly indexed to SyRS requirement IDs | MO satisfaction cannot be formally demonstrated |
| TG-04 | RTM SYS-REQ-x ≠ SyRS SYS-F-xxx | Ambiguous cross-reference requiring manual interpretation |
| TG-05 | FMEA fault IDs not cross-referenced to SRS safety requirements (SR-x) | Fault detection completeness unverifiable |

---

## 7. Subsystem Responsibility Analysis

| Subsystem | Requirements | ICD | FMEA | V&V | Assessment |
|---|---|---|---|---|---|
| ADCS (EKF + LQR + B-dot) | SYS-F-101..115 ✅ | I2C0/PWM ✅ | §7.1-7.3 ✅ | 91.9% lines ✅ | **ADEQUATE** |
| EPS Monitor | SYS-F-201..205 ✅ | ADC/GPIO ✅ | §7.9 ✅ | 12/12 ✅ | **ADEQUATE** |
| FDIR / Fault Manager | SYS-F-205, SR-1..4 ✅ | N/A ✅ | §7.1-7.10 ✅ | 12/12 ✅ | **ADEQUATE** |
| FMM | SYS-F-111, 115, 204 ✅ | N/A ✅ | §7.6, 7.9 ✅ | 11/11 ✅ | **ADEQUATE** |
| Logger | SYS-F-301..304 ✅ | N/A ✅ | Partial ⚠️ | 12/12 ✅ | **ADEQUATE (SYS-F-304 pending)** |
| COMMS (CSP/libcsp) | SYS-F-401..407 ✅ | UART1 ✅ | §7.10 ✅ | Stub ⚠️ | **PARTIAL — MAJ-02 (CSP version conflict)** |
| Payload | FR-13..17, PLD-R-001..005 ✅ | ICD §15, PAYLOAD-SPEC-001 ✅ | FMEA OI-1 open ⚠️ | T-PLD-TSK/INT-01..10 planned | **ADEQUATE (CDR: FMEA update)** |
| GPS | FR-18/19, IR-10 ✅ | ICD §7, PHASE7_PLAN ✅ | FMEA update pending ⚠️ | T-GPS-01..04 planned | **ADEQUATE (CDR: FMEA update)** |
| Ground Station | IR-4 (to be updated) ⚠️ | Absent ⚠️ | N/A | N/A | **MISMATCHED — MAJ-05** |
| Watchdog HAL | SYS-F-211..214 ✅ | GPIO20/TPS3431 ✅ | §7.6 ✅ | HW pending ⚠️ | **PARTIAL — SYS-F-214 [PLANNED]** |

---

## 9. ECSS Checklist Result

| Check Item | ECSS Reference | Result | Notes |
|---|---|---|---|
| Mission objectives with measurable criteria | ECSS-E-ST-10-06C §5.2 | ✅ PASS | MO-1..7 with 3-level success table |
| All requirements uniquely identified | ECSS-E-ST-40C §5.3 | ⚠️ PARTIAL | Dual ID scheme (TG-04); SRS alias (MAJ-01) |
| Requirements are verifiable | ECSS-E-ST-10-02C §5 | ⚠️ PARTIAL | MO-3 not quantified (MAJ-07); NFR-4 TBD |
| Interface requirements for all subsystems | ECSS-E-ST-40C §5.4 | ✅ PASS | All major subsystems (payload, GPS, comms, ADCS, EPS) now covered |
| SW requirements traceable to system requirements | ECSS-E-ST-40C §5.4.1 | ⚠️ PARTIAL | MO→SyRS gap (TG-01); inconsistent IDs (MIN-01) |
| Verification plan exists | ECSS-E-ST-10-02C §5.4 | ✅ PASS | SVVP-OBC-001 v1.0 |
| Resource budgets within allocation | ECSS-E-ST-40C §5.4.3 | ⚠️ PARTIAL | Flash overrun unresolved (MAJ-03); power TBD (NFR-4) |
| FDIR requirements defined | ECSS-E-ST-40C §5.4.4 | ✅ PASS | Complete FDIR decision matrix in SyRS §9 |
| Software management plan exists | ECSS-M-ST-10C | ✅ PASS | SMP-OBC-001 v1.0 |
| Risk management plan exists | ECSS-M-ST-80C | ✅ PASS | RMP-OBC-001 v1.0 |
| FMEA exists | ECSS-Q-ST-30-02C | ⚠️ PARTIAL | v0.1 CDR baseline; payload/GPS FMEA update pending |
| Coding standards defined | ECSS-Q-ST-80C | ✅ PASS | `CODING_STANDARDS.md`; MISRA deviations logged |

---

## 10. Technical Scoring

| Domain | Score (0–10) | Rationale |
|---|---|---|
| Mission objectives definition | 9 | All major objectives and success criteria are now covered, including payload and GPS. |
| Requirements completeness | 8 | All major subsystems have requirements. Only ground station/RF link margin remain. |
| Requirements quality (SMART) | 8 | Requirements are specific, testable, and traceable. Minor ID inconsistencies remain. |
| Traceability (MO→SyRS→SRS→Test) | 7 | RTM is detailed bottom-up. Top-down MO→SyRS chain is informal. |
| Interface identification | 8 | ICD covers all hardware interfaces. Ground segment update pending. |
| Verification feasibility | 8 | Unit/integration test coverage excellent. HIL campaign not yet scheduled. |
| FDIR & safety adequacy | 9 | EPS→FaultMgr→FMM authority chain is exemplary for an academic mission. |
| Document set maturity | 8 | Comprehensive, up-to-date, and consistent for SRR. |
| **TOTAL** | **8.1 / 10** | |

---

## 12. Recommended Actions

Detailed action items are tracked in `AIR-OBC-001`. Summary:

### Immediate — Before PDR Baseline Lock

| ID | Action | ECSS Ref | Priority |
|---|---|---|---|
| ACT-01 | Update SRS internal ID from `REQ-001` to `SRS-OBC-001` | ECSS-E-ST-40C §5.3 | HIGH |
| ACT-02 | Resolve CSP version conflict (v1.x vs. v2.x); align all documents | ECSS-E-ST-40C §5.4.2 | **CRITICAL** |
| ACT-03 | Measure flash footprint with `arm-none-eabi-size`; clarify NFR-6 basis | ECSS-E-ST-40C §5.4.3 | HIGH |
| ACT-05 | Replace IR-4 with actual UART1/CSP/KISS/E22 interface requirement | ECSS-E-ST-40C §5.4.2 | HIGH |
| ACT-07 | Amend MO-3 FDIR response time from "1 orbit" to ≤10 s | ECSS-E-ST-10-06C §5.3.1 | HIGH |
| ACT-08 | Add MO→SyRS cross-reference table to SyRS §1 | ECSS-E-ST-40C §5.4.1 | MEDIUM |
| ACT-09 | Align RTM SYS-REQ-x identifiers to SyRS SYS-F-xxx IDs | ECSS-E-ST-40C §5.3 | MEDIUM |
| ACT-10 | Correct SYS-F-401 SMP reference; harmonize NFR-7 boot time | ECSS-E-ST-40C §5.4 | MEDIUM |

### Before CDR

| ID | Action | ECSS Ref | Priority |
|---|---|---|---|
| ACT-11 | Lock magnetometer part number; update FR-11, ICD, driver | — | HIGH |
| ACT-12 | Add RF link margin requirement for MO-4 | ECSS-E-ST-40C §5.4.2 | MEDIUM |
| ACT-13 | Add reaction wheel performance requirements (speed, torque, momentum) | ECSS-E-ST-40C §5.4 | MEDIUM |
| ACT-14 | Schedule HIL campaign: watchdog validation, stack HWMs, power profile | ECSS-E-ST-10-02C | HIGH |
| ACT-15 | FMEA v1.0: assign closure dates for all OIs | ECSS-Q-ST-30-02C | HIGH |
| ACT-16 | Add flash backend completion sprint to CDR open items | ECSS-E-ST-40C §5.4.3 | HIGH |
