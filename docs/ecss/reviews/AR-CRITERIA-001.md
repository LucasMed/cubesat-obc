# ECSS AR Gate Criteria — 2026-07-23

## Decision: **NO-GO** (AR not started)

**Gate**: AR (Acceptance Review)
**Date**: 2026-07-23
**Reviewed by**: ECSS Gate Agent
**Baseline**: `dev` (latest)
**Previous**: TRR-GATE-002 (GO, 2026-07-21)

---

## Mandatory Criteria

| # | Criterion | Threshold | Evidence | Result |
|---|-----------|-----------|----------|--------|
| M1 | HIL tests on physical RP2350 pass | All ATP-FUNC tests pass on HW | Not yet executed | ❌ PENDING |
| M2 | STR-OBC-001 updated with HIL results | HIL section present and filled | STR §4.5 HIL Test Results: TBD | ❌ PENDING |
| M3 | SCI-OBC-001 released | Status=Released | SCI-OBC-001 v1.0 — Status: **Released** | ✅ PASS |
| M4 | Qualification tests pass (critical subset) | Thermal, vibration, power — all PASS | Not yet executed | ❌ PENDING |
| M5 | Power budget verified on HW | ATP-PERF-13..15 PASS on RP2350 | Not yet executed | ❌ PENDING |
| M6 | RTM 100% coverage maintained | 28/28 requirements traced | 28/28 FR+NFR+OR have test assignments (RTM-OBC-001) | ✅ PASS |
| M7 | Flight build (.uf2) locked and tagged | Git tag + artifacts/ exist | Not yet created | ❌ PENDING |
| M8 | 0 critical/high open defects | No critical or high severity defects | No known critical/high defects | ✅ PASS |

**Mandatory Result**: 3/8 PASS, 5 PENDING ❌

---

## Conditional Criteria

| # | Criterion | Evidence | Result |
|---|-----------|----------|--------|
| C1 | Qualification test procedure exists | QUAL-OBC-001 v1.0 — Status: **Released** | ✅ RESOLVED |

---

## Environmental Test Requirements

### Critical Subset (Mandatory for AR)

| Test ID | Test Name | Standard | Acceptance Criteria |
|---------|-----------|----------|---------------------|
| ATP-ENV-01 | Thermal cycling (-20°C to +50°C, 5 cycles) | ECSS-E-ST-10-02C | All functional tests pass post-cycling |
| ATP-ENV-02 | Vibration (random, ≥14.1 g_rms) | ECSS-E-ST-10-02C | No mechanical damage, all tests pass |
| ATP-ENV-03 | Power supply variation (3.0V–5.5V) | ECSS-E-ST-10-02C | Operation at voltage extremes |
| ATP-ENV-04 | Post-environmental functional verification | ECSS-E-ST-10-02C | All ATP-FUNC tests pass |

### Nice-to-Have (Not Blocking AR)

| Test ID | Test Name | Notes |
|---------|-----------|-------|
| ATP-ENV-05..14 | EMI/EMC, radiation, mission-profile | If lab available |

---

## Current State

### Software Readiness

| Metric | Current | Threshold | Status |
|--------|---------|-----------|--------|
| Unit tests | 72/72 PASSING | All pass | ✅ |
| Line coverage | 90.0% | ≥ 90% | ✅ |
| Function coverage | 90.6% | ≥ 90% | ✅ |
| RTM coverage | 28/28 | 100% | ✅ |
| Static analysis | 0 issues | 0 | ✅ |
| CDR checklist | 27/27 PASS | ≥ 80% | ✅ |

### Hardware Verification (2026-07-15)

| System | Status | Detail |
|--------|--------|--------|
| GPS | ✅ Active | 11 satellites, 3D fix |
| FR-19 guard | ✅ Verified | 44s/23s deltas correctly rejected |
| IMU (MPU6050) | ✅ Detected | ID=0x70 |
| Magnetometer | ✅ Detected | QMC5883L at 0x2C |
| Sun sensor | ✅ Active | X/Y photodiode at 1 Hz |
| RTC (DS3231) | ✅ Active | Timekeeping operational |
| W25Q64 Flash | ✅ Detected | M=EF T=40 C=17 |
| Battery | ✅ 4280 mV | Nominal |
| Heap | ✅ Stable | 39296 bytes, no leaks |
| WDT | ✅ Stable | No SAFE mode entry |

---

## Evidence Summary

- **Software readiness**: 72/72 tests, 90.0% coverage, RTM 28/28, cppcheck clean
- **Hardware verified**: GPS, IMU, magnetometer, sun sensor, RTC, flash, telemetry
- **Documentation**: ATP, ITP, STP, SVVP, SCI all released
- **HIL tests**: NOT YET EXECUTED
- **Environmental tests**: NOT YET EXECUTED

---

## Open Items

| ID | Description | Priority | Owner | Target |
|----|-------------|----------|-------|--------|
| AR-01 | Execute HIL tests on physical RP2350 | HIGH | — | 2026-08-15 |
| AR-02 | Execute environmental qualification (critical subset) | HIGH | — | 2026-08-30 |
| AR-03 | Update STR with HIL results | HIGH | — | 2026-09-01 |
| AR-04 | Lock flight build and tag | MEDIUM | — | 2026-09-05 |

---

## Recommendation

### NO-GO for AR (not started)

**Rationale**: AR is a QUALIFICATION gate — it proves flight software works on real hardware under real conditions. While software readiness is excellent (72/72 tests, 90.0% coverage, all documentation in place), the HIL and environmental tests have not been executed yet.

**Next Steps**:
1. Execute HIL tests on physical RP2350 (ATP-FUNC-xx, ATP-PERF-xx)
2. Execute environmental qualification (ATP-ENV-01..04)
3. Update STR with results
4. Lock flight build
5. Re-evaluate AR gate

---

## Gate Tracking

| Gate | Status | Decision | Date |
|------|--------|----------|------|
| SRR | ✅ CLOSED | GO | 2026-03-10 |
| PDR | ✅ CLOSED | GO | 2026-03-10 |
| CDR | ✅ CLOSED | GO | 2026-07-14 |
| TRR | ✅ CLOSED | GO | 2026-07-21 |
| **AR** | **❌ NOT STARTED** | **NO-GO** | **2026-07-23** |
| FRR | ❌ NOT STARTED | — | — |
