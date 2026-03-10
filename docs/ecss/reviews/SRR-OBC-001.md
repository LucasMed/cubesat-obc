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

**Overall SRR Verdict: CONDITIONALLY PASSED**

Seven (7) Major Issues and ten (10) Minor Issues were identified and require resolution before the PDR baseline is locked. The document set demonstrates engineering maturity well above typical academic CubeSat projects. Requirements are traceable, tests exist, and the FDIR architecture is coherent. However, critical inconsistencies between document revisions, an undefined payload subsystem, an unconfirmed flash budget, and a GPS interface with no functional requirements must be resolved.

Action items are tracked in `AIR-OBC-001`.

---

## 2. Document Baseline Reviewed

| Document | ID | Version | Status |
|---|---|---|---|
| Mission Requirements Doc / ConOps | MRD-OBC-001 | 1.1 | Approved |
| System Requirements Spec | SyRS-OBC-001 | 1.0 | Approved |
| Software Requirements Spec | SRS-OBC-001 | 2.0 | Active |
| Requirements Traceability Matrix | RTM-OBC-001 | — | Active |
| Software V&V Plan | SVVP-OBC-001 | 1.0 | Approved |
| Interface Control Document | ICD-OBC-001 | 1.0 | Released |
| Risk Management Plan | RMP-OBC-001 | 1.0 | Approved |
| FMEA | FMEA-OBC-001 | 0.1 | CDR Baseline |
| Flight Software Design Desc. | FSW-SDD-001 | 0.2 | CDR Baseline |
| Software Architecture Desc. | ARCHITECTURE.md | — | Released |
| libcsp documentation | third_party/libcsp/doc | v1.x/v2.x | Attached |

---

## 3. Major Issues

### MAJ-01 — Document Identifier Inconsistency (SRS internal name vs. project name)

| Field | Value |
|---|---|
| **Location** | `docs/ecss/requirements/SRS-OBC-001.md` header |
| **ECSS Reference** | ECSS-E-ST-40C §5.3 |
| **AIR Reference** | ACT-01 |

**Finding:** The document header for the Software Requirements Specification declares `Document ID: REQ-001 v2.0`. Every other document in the baseline (RTM, SVVP, FSW-SDD, RMP, SyRS) consistently references this document as **`SRS-OBC-001`**. An inconsistent document identifier breaks formal configuration management traceability.

**Required Action:** Update the document header internal ID to `SRS-OBC-001`. Bump version to v2.1 and record the change in the document's change history.

---

### MAJ-02 — libcsp Version Conflict (v1.x vs. v2.x)

| Field | Value |
|---|---|
| **Location** | SyRS-OBC-001 SYS-F-401 vs. ARCHITECTURE.md §7 |
| **ECSS Reference** | ECSS-E-ST-40C §5.4.2, ICD-OBC-001 |
| **AIR Reference** | ACT-02 |

**Finding:** SYS-F-401 states *"The OBC shall run the `libcsp` v1.x stack"*. ARCHITECTURE.md §7 states *"Protocol: CubeSat Space Protocol (CSP) v2"*. The libcsp documentation attached to this review corresponds to the CSP v2.x API (`csp_id_t` pack/unpack model). CSP v1.x and v2.x are protocol-incompatible: they use different frame formats and addressing spaces. A v1.x OBC **cannot interoperate** with a v2.x ground station.

**Required Action (Blocking):** Declare the definitive CSP version. Align SYS-F-401, ARCHITECTURE.md, COMMS-DES-001, and ICD-OBC-001 to the same version. Verify the vendored `third_party/libcsp` source matches the declared version. Assess ground-station tool compatibility before PDR.

---

### MAJ-03 — Flash Footprint Requirement Breach (NFR-6)

| Field | Value |
|---|---|
| **Location** | SRS-OBC-001 NFR-6; build artifact `artifacts/cubesat_obc_pico.uf2` |
| **ECSS Reference** | ECSS-E-ST-40C §5.4.3 |
| **AIR Reference** | ACT-03 |

**Finding:** NFR-6 requires `Flash footprint < 200 KB, SRAM usage < 60 KB`. The current build artifact is listed at **536 KB UF2**, approximately 168% over the stated budget. The requirement is additionally ambiguous — it does not specify the measurement basis (UF2 container vs. raw ELF `.text`+`.data` sections). NFR-6 is marked "TBC" in the SRS with no open item raised.

**Required Action:** (a) Clarify NFR-6 measurement basis using `arm-none-eabi-size` on the linked ELF. (b) Confirm actual flash headroom. (c) Either revise the requirement to reflect a realistic budget or raise a formal open item with a remediation timeline. Record as a risk in RMP-OBC-001.

---

### MAJ-04 — Payload Subsystem Completely Undefined

| Field | Value |
|---|---|
| **Location** | MRD-OBC-001 §2.3 MO-6; FMEA-OBC-001 §1.2 (OI-1); ICD-OBC-001 |
| **ECSS Reference** | ECSS-E-ST-10-06C §5.2, ECSS-M-ST-10C review entry criteria |
| **AIR Reference** | ACT-04 |

**Finding:** Mission success criterion MO-6 requires *"payload rail enabled"* for full success. However, there are zero functional requirements, zero interface definitions, and zero FDIR provisions for a payload subsystem. The FMEA explicitly excludes it ("payload undefined — OI-1"). The payload is referenced only as a power rail in EPS documentation. At SRR, the payload concept of operations must be at minimum notionally described.

**Required Action:** Define the payload at mission concept level (type, power budget, activation conditions, FMM safety interlock). Add ≥5 system-level requirements to SyRS. Add a stub section to ICD-OBC-001. Alternatively, formally descope MO-6 from the mission success criteria with documented rationale.

---

### MAJ-05 — Ground Station Interface Mismatch (IR-4)

| Field | Value |
|---|---|
| **Location** | SRS-OBC-001 IR-4 vs. ICD-OBC-001 §7–8, ARCHITECTURE.md §7 |
| **ECSS Reference** | ECSS-E-ST-40C §5.4.2 |
| **AIR Reference** | ACT-05 |

**Finding:** IR-4 specifies *"Ground station telemetry via WiFi TCP/UDP"*. The implemented and ICD-defined architecture uses **CSP over UART1 with KISS framing** (E22-400M30S LoRa radio). The WiFi interface (`CYW43`) is visible in early architecture diagrams but is absent from the current ICD. This fundamental interface change has never been captured in requirements — IR-4 remains unchanged and marked "TBD".

**Required Action:** Replace IR-4 with an interface requirement aligned to the actual UART1/CSP/KISS/E22-400M30S implementation. Formally document that WiFi telemetry is descoped. Update ARCHITECTURE.md diagram accordingly.

---

### MAJ-06 — GPS Receiver in ICD with No Functional Requirements

| Field | Value |
|---|---|
| **Location** | ICD-OBC-001 §3 (UART0 → NEO-7M GPS); SRS/SyRS (no GPS requirements) |
| **ECSS Reference** | ECSS-E-ST-40C §5.4, ECSS-Q-ST-80C |
| **AIR Reference** | ACT-06 |

**Finding:** A GPS receiver (NEO-7M on UART0) is part of the flight hardware ICD, consuming a hardware UART and drawing power, but there are zero functional requirements in SRS or SyRS for GPS data acquisition, NMEA parsing, orbit determination, or time synchronization. The FMEA does not include GPS failure modes. This is an uncontrolled interface.

**Required Action:** Either (a) add functional requirements for GPS (e.g., time sync, orbit state vector seeding from NMEA) and update FMEA, or (b) formally descope GPS from this release: document UART0 pin assignment as "reserved — GPS future use" and remove from ICD §3 active interface list.

---

### MAJ-07 — FDIR MO-3 Response Time Not Quantified ("within 1 orbit")

| Field | Value |
|---|---|
| **Location** | MRD-OBC-001 §2.3 MO-3 |
| **ECSS Reference** | ECSS-E-ST-10-06C §5.3.1 |
| **AIR Reference** | ACT-07 |

**Finding:** MO-3 success criterion states *"FAULT_LEVEL_CRITICAL event triggers FM_SAFE within 1 orbit"*. A LEO orbit is ≈90 minutes. For a FDIR critical response, this is a non-constraint. SYS-F-204 correctly implements `FM_SAFE` transition within the EPS Monitor tick cycle (5 s), making MO-3 trivially satisfiable. The mission metric does not reflect the actual engineering capability.

**Required Action:** Amend MO-3 to specify a meaningful FDIR budget. Recommended: *"FAULT_LEVEL_CRITICAL event triggers FM_SAFE within 10 s (2× Health Monitor period)"*. This reflects the measured behavior and constitutes a verifiable criterion.

---

## 4. Minor Issues

### MIN-01 — Dual Numbering: RTM SYS-REQ-x vs. SyRS SYS-F-xxx

**Finding:** The RTM uses informal identifiers (`SYS-REQ 1`, `SYS-REQ 2`, ...) that do not map 1:1 to the SyRS identifiers (`SYS-F-101`, `SYS-F-201`, etc.). Cross-reference requires manual interpretation.

**Required Action:** Align RTM requirement references to SyRS IDs. Replace `SYS-REQ 1` with `SYS-F-101..106`, etc. **| AIR: ACT-09**

---

### MIN-02 — SYS-F-401 "FreeRTOS SMP" Contradicts SYS-P-003

**Finding:** SYS-F-401 states the CSP stack runs "on FreeRTOS SMP". SYS-P-003 is `[PLANNED]` with SMP currently disabled (`configNUMBER_OF_CORES = 1`). The requirement references a configuration that does not exist.

**Required Action:** Amend SYS-F-401 to "FreeRTOS (single-core; SMP planned per SYS-P-003)". **| AIR: ACT-10**

---

### MIN-03 — Boot Time NFR-7 (TBD/5 s) vs. SyRS-NF-005 (IMPL/10 s) Conflict

**Finding:** SRS NFR-7 targets 5 s boot time with "TBD" status. SyRS-NF-005 sets a 10 s requirement with `[IMPL]` status. Two different values appear in the same baseline.

**Required Action:** Harmonize. SRS NFR-7 must adopt the 10 s budget from SyRS-NF-005 and be updated to `[IMPL]`. **| AIR: ACT-10**

---

### MIN-04 — SYS-F-304 Flash Backend [PLANNED] Blocks MO-6

**Finding:** MO-6 requires Class-A events to survive reset. SYS-F-304 (flash persistence) is `[PLANNED]` with no target version. Without this, MO-6 full-success cannot be demonstrated.

**Required Action:** Add SYS-F-304 to CDR open items with a completion sprint target. Raise a formal risk in RMP-OBC-001. **| AIR: ACT-16**

---

### MIN-05 — Watchdog Timeout Hardware Validation Not Scheduled (SYS-F-214)

**Finding:** SYS-F-214 is `[PLANNED]` with no scheduled hardware test campaign. The watchdog timing window is a safety-critical parameter.

**Required Action:** Add a hardware validation test for the watchdog period to the HIL test campaign (SVVP §10) and assign a test case ID in RTM. **| AIR: ACT-14**

---

### MIN-06 — No RF Link Margin Requirement

**Finding:** MO-4 requires TT&C over 433 MHz LoRa but there is no system requirement for minimum link margin, receive sensitivity, or Eb/N0 threshold at any orbital geometry.

**Required Action:** Add ≥1 system requirement for RF link margin (e.g., ≥6 dB margin at 600 km SSO, 5° elevation). Reference COMMS-DES-001 link budget. **| AIR: ACT-12**

---

### MIN-07 — Task Stack High-Water Marks Not Measured for All Tasks (SYS-NF-006)

**Finding:** SYS-NF-006 is `[PLANNED]`. Only the Heartbeat task HWM is instrumented. Flight-critical tasks (AttitudeControl 20 Hz, SensorRead 10 Hz, TelemetryTask, CommandTask) have unmeasured stack watermarks.

**Required Action:** Instrument all task HWMs in the hardware test campaign. Apply pass/fail criterion of ≥20% headroom per SYS-NF-006. **| AIR: ACT-14**

---

### MIN-08 — Magnetometer Driver Inconsistency (HMC5883L vs. LIS3MDL)

**Finding:** ICD-OBC-001 §6 specifies `HMC5883L`. ARCHITECTURE.md notes CDR migration to `LIS3MDL` and flags QMC5883L clone risk. FR-11 specifies HMC5883L. The flight hardware decision is not locked.

**Required Action:** Lock the magnetometer part number before CDR. Update FR-11, ICD-OBC-001, and `src/drivers/mag/` filename. Document rationale in BOM-OBC-001. **| AIR: ACT-11**

---

### MIN-09 — Reaction Wheel Performance Not Specified

**Finding:** FR-5 and SYS-F-111 depend on reaction wheel torque capability. No requirement exists for maximum RW speed (rpm), maximum torque (N·m), momentum storage (N·m·s), or saturation threshold. LQR gains are therefore unverifiable at system level.

**Required Action:** Add subsystem requirements for RW performance parameters. Cross-reference `controller_limits.h` and `lqr_schedule.h`. **| AIR: ACT-13**

---

### MIN-10 — FMEA Version 0.1 at CDR Level with Open Items

**Finding:** FMEA-OBC-001 is v0.1 published at CDR baseline with open items not assigned closure dates.

**Required Action:** Review FMEA OI list (§11), assign owner and closure target for each, baseline at v1.0 before QR. **| AIR: ACT-15**

---

## 5. Requirements Quality

| Dimension | Score | Notes |
|---|---|---|
| Uniqueness of ID | 3/5 | Dual numbering (SYS-REQ-x vs. SYS-F-xxx), SRS internal alias `REQ-001`. |
| Completeness | 3/5 | Payload undefined, GPS unmanaged, RF link budget absent, boot time inconsistent. |
| Verifiability | 4/5 | Most requirements are quantifiable with explicit test IDs. MO-3 and NFR-4 are exceptions. |
| Consistency | 3/5 | CSP version conflict (v1.x vs. v2.x), SMP conflict in SYS-F-401, NFR-6 status ambiguous. |
| Traceability (MO→SyRS→SRS) | 3/5 | MO→SyRS mapping is inferred, not explicit. FR→SyRS uses inconsistent IDs. |
| Implementation Status Tracking | 5/5 | `[IMPL]`/`[PLANNED]` tagging in SyRS is exemplary engineering practice. |
| Non-Functional Completeness | 3/5 | Power (NFR-4 TBD), flash (NFR-6 TBC ambiguous), boot time conflicting. |
| **Aggregate** | **3.4 / 5.0** | |

---

## 6. Traceability Analysis

### Vertical Traceability Chain

```
MRD-OBC-001 (MO-x)
    │
    ▼  ← GAP TG-01: No direct MO→SyRS cross-reference table
SyRS-OBC-001 (SYS-F-xxx, SYS-NF-xxx)
    │
    ▼  ← GAP TG-04: SRS uses different numbering scheme
SRS-OBC-001 (FR-x, NFR-x, IR-x, SR-x)
    │
    ▼  ← GOOD: RTM maps FR → Module → Test
RTM-OBC-001
    │
    ▼
Unit / Integration Tests
```

### Identified Traceability Gaps

| ID | Description | Risk |
|---|---|---|
| TG-01 | MO-1..MO-7 not explicitly indexed to SyRS requirement IDs | MO satisfaction cannot be formally demonstrated |
| TG-02 | GPS interface in ICD with zero requirements in SRS/SyRS | Unmanaged scope creep |
| TG-03 | Payload rail in MO-6 with no requirements | Success criterion without traceable verification |
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
| Payload | None ❌ | None ❌ | Excluded ❌ | None ❌ | **UNDEFINED — MAJ-04** |
| GPS | None ❌ | UART0 (ICD only) ❌ | Excluded ❌ | None ❌ | **UNMANAGED — MAJ-06** |
| Ground Station | IR-4 (wrong interface) ❌ | Absent ❌ | N/A | N/A | **MISMATCHED — MAJ-05** |
| Watchdog HAL | SYS-F-211..214 ✅ | GPIO20/TPS3431 ✅ | §7.6 ✅ | HW pending ⚠️ | **PARTIAL — SYS-F-214 [PLANNED]** |

---

## 8. Verification Feasibility

| Requirement | Method | Feasibility | Risk |
|---|---|---|---|
| SYS-F-101..106 EKF | Simulation + HIL | High ✅ | None — simulation demonstrated |
| SYS-F-111 LQR gains | Closed-loop simulation | High ✅ | None — `closed_loop_sim.h` exists |
| SYS-F-114 B-dot detumble | Simulation + Helmholtz | Medium ⚠️ | Requires magnetic field emulation for full hardware demo |
| SYS-F-201..205 EPS | HIL (bench PSU) | High ✅ | Voltage threshold measurable with bench supply |
| SYS-F-211..213 WDT | HIL (task suspension) | Medium ⚠️ | Requires deliberate task hang test |
| SYS-F-214 WDT window | HIL measurement | Medium ⚠️ | Implementation [PLANNED]; no test scheduled |
| SYS-F-304 Flash persist | HIL (power cycle) | High ✅ | Implementation [PLANNED] — blocks MO-6 |
| SYS-F-401..407 CSP | Integration test | Medium ⚠️ | Requires resolved CSP version (MAJ-02) and RF hardware |
| NFR-4 Power (< 2 W) | Hardware measurement | Medium ⚠️ | No scheduled test, no measurement setup defined in SVVP |
| NFR-6 Flash size | `arm-none-eabi-size` | High ✅ | Requires ELF measurement; NFR-6 measurement basis unclear |
| MO-4 TT&C | End-to-end RF test | Low ⚠️ | Link budget not defined, ground station not documented |
| MO-3 FDIR timing | HIL fault injection | High ✅ | After MO-3 re-quantification (MAJ-07) |

---

## 9. ECSS Checklist Result

| Check Item | ECSS Reference | Result | Notes |
|---|---|---|---|
| Mission objectives with measurable criteria | ECSS-E-ST-10-06C §5.2 | ✅ PASS | MO-1..7 with 3-level success table |
| All requirements uniquely identified | ECSS-E-ST-40C §5.3 | ⚠️ PARTIAL | Dual ID scheme (TG-04); SRS internal alias (MAJ-01) |
| Requirements are verifiable | ECSS-E-ST-10-02C §5 | ⚠️ PARTIAL | MO-3 not quantified (MAJ-07); NFR-4 TBD |
| Interface requirements for all subsystems | ECSS-E-ST-40C §5.4 | ❌ FAIL | GPS undefined (MAJ-06); Payload undefined (MAJ-04); ground segment mismatch (MAJ-05) |
| SW requirements traceable to system requirements | ECSS-E-ST-40C §5.4.1 | ⚠️ PARTIAL | MO→SyRS gap (TG-01); inconsistent IDs (MIN-01) |
| Verification plan exists | ECSS-E-ST-10-02C §5.4 | ✅ PASS | SVVP-OBC-001 v1.0 |
| Resource budgets within allocation | ECSS-E-ST-40C §5.4.3 | ⚠️ PARTIAL | Flash overrun unresolved (MAJ-03); power TBD (NFR-4) |
| FDIR requirements defined | ECSS-E-ST-40C §5.4.4 | ✅ PASS | Complete FDIR decision matrix in SyRS §9 |
| Software management plan exists | ECSS-M-ST-10C | ✅ PASS | SMP-OBC-001 v1.0 |
| Risk management plan exists | ECSS-M-ST-80C | ✅ PASS | RMP-OBC-001 v1.0 |
| FMEA exists | ECSS-Q-ST-30-02C | ⚠️ PARTIAL | v0.1 CDR baseline; payload excluded (OI-1) |
| Coding standards defined | ECSS-Q-ST-80C | ✅ PASS | `CODING_STANDARDS.md`; MISRA deviations logged |

---

## 10. Technical Scoring

| Domain | Score (0–10) | Rationale |
|---|---|---|
| Mission objectives definition | 8 | Clear, measurable, three-level success criteria. Deductions for MO-3 loose timing and undefined payload in MO-6. |
| Requirements completeness | 6 | Good ADCS/FDIR/FMM/EPS coverage. Gaps in payload, GPS, ground station, and RF link budget. |
| Requirements quality (SMART) | 6 | Most requirements are specific and testable. Inconsistencies in IDs and versions lower the score. |
| Traceability (MO→SyRS→SRS→Test) | 6 | RTM is detailed bottom-up. Top-down MO→SyRS chain is informal. |
| Interface identification | 5 | ICD solid for hardware interfaces. Ground segment, payload, and GPS are uncontrolled. |
| Verification feasibility | 7 | Unit test coverage excellent (91.9%). HIL campaign not yet scheduled. |
| FDIR & safety adequacy | 9 | EPS→FaultMgr→FMM authority chain is exemplary for an academic mission. |
| Document set maturity | 7 | Comprehensive set for SRR; CSP version conflict and SRS alias are configuration management gaps. |
| **TOTAL** | **6.8 / 10** | |

---

## 11. Flight Readiness Potential

This software demonstrates **above-average flight readiness potential** for an academic CubeSat mission at SRR stage.

### Strengths

- Deterministic, statically-allocated RTOS architecture with measured ±43 µs task jitter (within 10 ms spec).
- Coherent and tested FDIR chain with Schmitt-trigger EPS hysteresis and single-authority FMM transition path.
- 91.9% line coverage with CI-enforced MISRA compliance, cppcheck, and clang-tidy.
- 15+ formal engineering documents — output exceeding typical academic teams.
- `[IMPL]`/`[PLANNED]` requirement tagging in SyRS is honest and traceable.

### Gaps Before Flight Qualification

- Payload definition and associated safety requirements mandatory before CDR.
- CSP version must be locked and end-to-end ground-station interoperability verified.
- Flash size budget requires ELF binary measurement.
- Hardware validation campaign (watchdog, stack HWMs, power profile) must be completed and scheduled.

---

## 12. Recommended Actions

Detailed action items are tracked in `AIR-OBC-001`. Summary:

### Immediate — Before PDR Baseline Lock

| ID | Action | ECSS Ref | Priority |
|---|---|---|---|
| ACT-01 | Update SRS internal ID from `REQ-001` to `SRS-OBC-001` | ECSS-E-ST-40C §5.3 | HIGH |
| ACT-02 | Resolve CSP version conflict (v1.x vs. v2.x); align all documents | ECSS-E-ST-40C §5.4.2 | **CRITICAL** |
| ACT-03 | Measure flash footprint with `arm-none-eabi-size`; clarify NFR-6 basis | ECSS-E-ST-40C §5.4.3 | HIGH |
| ACT-04 | Define payload concept; add ≥5 SyRS requirements and ICD stub | ECSS-E-ST-10-06C §5.2 | HIGH |
| ACT-05 | Replace IR-4 with actual UART1/CSP/KISS/E22 interface requirement | ECSS-E-ST-40C §5.4.2 | HIGH |
| ACT-06 | Decide GPS scope: add requirements or formally descope | ECSS-E-ST-40C §5.4 | HIGH |
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
| ACT-15 | Baseline FMEA at v1.0 with all OIs assigned and closed | ECSS-Q-ST-30-02C | MEDIUM |
| ACT-16 | Implement and test SYS-F-304 Flash Backend (blocks MO-6) | ECSS-E-ST-40C §5.4 | HIGH |

---

*Review conducted per ECSS-M-ST-10C §5.3 SRR criteria. This review covers OBC flight software scope only. Launch vehicle, ground segment infrastructure, and RF propagation environment are out of scope. Next milestone: PDR — recommended no earlier than 4 weeks after resolution of MAJ-01 through MAJ-07.*

*Reference action tracking: `AIR-OBC-001`*
