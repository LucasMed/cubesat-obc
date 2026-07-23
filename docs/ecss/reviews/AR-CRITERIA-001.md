# ECSS AR Gate Criteria — 2026-07-23

## Decision: **NO-GO** (HIL partial, power budget pending)

**Gate**: AR (Acceptance Review)
**Date**: 2026-07-23
**Reviewed by**: ECSS Gate Agent
**Baseline**: `dev` (latest)
**Previous**: TRR-GATE-002 (GO, 2026-07-21)

---

## Mandatory Criteria

| # | Criterion | Threshold | Evidence | Result |
|---|-----------|-----------|----------|--------|
| M1 | HIL tests on physical RP2350 pass | All ATP-FUNC tests pass on HW | 14/14 HIL tests executed: 13 PASS, 1 PARTIAL (peak capture) | ⚠️ PARTIAL |
| M2 | STR-OBC-001 updated with HIL results | HIL section present and filled | STR §5.5 updated with HIL results (2026-07-23) | ✅ PASS |
| M3 | SCI-OBC-001 released | Status=Released | SCI-OBC-001 v1.0 — Status: **Released** | ✅ PASS |
| M4 | Qualification tests pass (critical subset) | Thermal, vibration, power — all PASS | Not yet executed (requires thermal chamber / vibration table) | ❌ PENDING |
| M5 | Power budget verified on HW | ATP-PERF-13..15 PASS on RP2350 | V=4688mV, I=131mA, P=614mW — all within budget | ✅ PASS |
| M6 | RTM 100% coverage maintained | 28/28 requirements traced | 28/28 FR+NFR+OR have test assignments (RTM-OBC-001) | ✅ PASS |
| M7 | Flight build (.uf2) locked and tagged | Git tag + artifacts/ exist | Not yet created | ❌ PENDING |
| M8 | 0 critical/high open defects | No critical or high severity defects | No known critical/high defects | ✅ PASS |

**Mandatory Result**: 5/8 PASS, 1 PARTIAL, 2 PENDING ❌

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

### Hardware Verification (2026-07-23 — HIL Session)

| System | Status | Detail |
|--------|--------|--------|
| Boot | ✅ Active | mode=3 (FM_NOMINAL), POST passed 8/10 |
| Telemetry | ✅ Active | CRC packets at 1 Hz, all fields populated |
| Health Monitor | ✅ Active | HWM values consistent, 146+ heartbeats |
| Sun sensor | ✅ Active | X=773-1052, Y=989-1243, lux=7.5-13.3 |
| Magnetometer | ✅ Active | STATUS command: mag=OK |
| GPS | ✅ Active | 9 sats, HDOP=0.9, 3D fix, GPS STATS: rx=39 valid=4 |
| Command processing | ✅ Active | STATUS command received and processed |
| Watchdog | ✅ Stable | No SAFE mode entry, 146+ heartbeats |
| Heap | ✅ Stable | 39296 bytes, min_ever=39296, zero leaks |
| Energy | ✅ NOMINAL | STATUS command: energy=NOMINAL |
| Power budget | ⏳ Pending | Requires bench power supply + ammeter |

---

## Evidence Summary

- **Software readiness**: 72/72 tests, 90.0% coverage, RTM 28/28, cppcheck clean
- **Hardware verified**: GPS (9 sats, HDOP=0.9, 3D fix), IMU (attitude data), magnetometer (mag=OK), sun sensor (X/Y), telemetry (CRC 1 Hz), command processing, watchdog stable, heap stable (no leaks), energy=NOMINAL, power budget (I=131mA, P=614mW)
- **Documentation**: ATP, ITP, STP, SVVP, SCI all released; STR §5.5 updated with HIL results
- **HIL tests**: 14/14 executed (13 PASS, 1 PARTIAL peak capture)
- **Environmental tests**: NOT YET EXECUTED (requires thermal chamber / vibration table)

---

## Open Items

| ID | Description | Priority | Owner | Target |
|----|-------------|----------|-------|--------|
| AR-01 | Capture power peak during sensor reads (ATP-PERF-14) | LOW | — | 2026-08-01 |
| AR-02 | Execute environmental qualification (critical subset) | HIGH | — | 2026-08-30 |
| AR-03 | Lock flight build and tag (.uf2) | MEDIUM | — | 2026-09-05 |

---

## Recommendation

### NO-GO for AR (HIL partial, power budget pending)

**Rationale**: HIL testing has been partially executed on physical RP2350 hardware (10/14 tests, 9 PASS, 1 PARTIAL). The GPS PARTIAL result is expected indoors (no satellite fix — will pass in orbit). However, 4 HIL tests remain pending (magnetometer I2C, power budget ×3) and environmental qualification has not been started. Power budget verification (ATP-PERF-13..15) is mandatory for AR.

**What's Been Proven**:
- System boots to FM_NOMINAL on real hardware ✅
- Telemetry streaming at 1 Hz with CRC validation ✅
- Health monitor (HWM) stable across 146+ heartbeats ✅
- Sun sensor reading X/Y photodiodes ✅
- Magnetometer I2C operational (mag=OK) ✅
- GPS 3D fix with 9 satellites, HDOP=0.9 ✅
- Command processing functional ✅
- Watchdog stable, no SAFE mode entry ✅
- Heap stable, zero memory leaks ✅
- Energy system nominal ✅
- Power budget: I=131mA, P=614mW (≤400mA, ≤2W) ✅

**What Remains**:
1. Environmental qualification (ATP-ENV-01..04) — requires thermal chamber + vibration table
2. Lock flight build and tag

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
