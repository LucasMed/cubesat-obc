# ECSS TRR Gate Report — 2026-07-15

## Decision: **GO-IF**

**Gate**: TRR (Test Readiness Review)
**Date**: 2026-07-15
**Reviewed by**: ECSS Gate Agent
**Baseline**: `dev` (567a59c)

---

## Mandatory Criteria

| # | Criterion | Threshold | Evidence | Result |
|---|-----------|-----------|----------|--------|
| M1 | RTM 100% coverage for current phase | All Phase 1-7 reqs traced | 28/28 FR+NFR+OR have test assignments (RTM-OBC-001) | ✅ PASS |
| M2 | All planned unit tests exist | All drivers have tests | 63 unit test files, all key drivers covered | ✅ PASS |
| M3 | All planned integration tests exist | Integration tests exist | 5 integration test files (deploy_seq, fault_safe, i2c_mpu6050, payload_integration, safe_trigger) | ✅ PASS |
| M4 | ITP-OBC-001 approved | Status=Released | ITP-OBC-001 v1.0 — Status: **Released** | ✅ PASS |
| M5 | ATP-OBC-001 approved | Status=Released | ATP-OBC-001 v1.0 — Status: **Released** | ✅ PASS |
| M6 | STP-OBC-001 approved | Status=Released | STP-OBC-001 v1.0 — Status: **Released** | ✅ PASS |
| M7 | SVVP-OBC-001 updated | Current phase covered | SVVP-OBC-001 v1.0 — Status: **Approved — CDR Baseline** | ✅ PASS |
| M8 | CDR checklist all PASS | ≥ 80% | CDR-GATE-001: 27/27 (100%) PASS | ✅ PASS |

**Mandatory Result**: 8/8 PASS ✅

---

## Conditional Criteria

| # | Criterion | Evidence | Result |
|---|-----------|----------|--------|
| C1 | All CHANGELOG unreleased features have RTM entry | `image_count` tracking (T-PLD-INT-04) and sun_sensor (no SRS requirement) have gaps | ⚠️ RATIONALE NEEDED |

---

## Current State

### Tests: 68/68 PASSING (100%)

All 68 CTest executables pass on host build:

- **Unit tests**: 63 files — covering all 7 driver modules, all core services
- **Integration tests**: 5 files — deploy sequence, fault/safe FDIR chain, I2C, payload
- **Static analysis**: cppcheck — 0 issues
- **Platform**: `dev` branch, host build, Debug + ASan/UBSan

### Code Coverage

| Metric | Current | CDR Baseline | Threshold |
|--------|---------|-------------|-----------|
| Lines | **87.8%** | 87.8% | ≥ 90% |
| Functions | **84.8%** | 84.8% | ≥ 90% |
| Branches | **78.4%** | — | — |

Coverage unchanged since CDR. No improvement, no regression.

### Key Coverage Gaps

| File | Line Coverage | Reason |
|------|-------------|--------|
| `tasks/health_monitor_task.c` | **33%** | FreeRTOS task wrapper + safety net — all logic tested via `test_health_monitor_task.c` |
| `tasks/attitude_control_task.c` | **56%** | Most uncovered lines are FreeRTOS task loop blocks tested via task test |
| `tasks/sensor_read_task.c` | **79%** | Sensor HAL init paths not exercised on host build |
| `actuators/pwm_hal.c` | **0%** | Hardware-only — Pico PWM HAL, untestable on host |
| `core/boot_info.c` | **0%** | Runs at startup — covered by POST test |
| `drivers/imu/mpu6050.c` | **40%** | HAL register ops — I2C hardware init skipped on host |

### Known Limitations (carried from CDR GO-IF)

1. **Coverage below 90%** — documented in CDR closure rationale (OI-4, OI-7 deferred)
2. **All `test_i2c_hardware` Pico targets** — require physical hardware, not runnable on host

---

## Evidence Summary

- **CDR checklist**: 27/27 items PASS (100%)
- **Test coverage**: 87.8% lines / 84.8% functions / 78.4% branches
- **Tests**: 68/68 passing (100%)
- **RTM coverage**: 28/28 requirements traced (100%)
- **Static analysis**: cppcheck — 0 issues
- **CHANGELOG unreleased gaps**: 2 items need attention (see below)

---

## Open Items

| ID | Description | Priority | Owner | Target |
|----|-------------|----------|-------|--------|
| T1 | **T-PLD-INT-04** new integration test not in RTM | LOW | — | Add to RTM §Integration Tests |
| T2 | **Sun sensor** has no SRS requirement — `test_sun_sensor.c` has no RTM trace | LOW | — | Create FR or document descope |
| T3 | **Coverage 87.8% < 90%** — carry-over from CDR GO-IF | MEDIUM | — | Accept as CDR condition, add task entry tests |
| T4 | **FR-19 ±500ms guard** not implemented in `ds3231.c` | MEDIUM | — | Add time-diff check or accept with rationale |
| T5 | **RTM count stale**: summary says "65 CTest" but actual is 68; "7 OR" but table shows 4 | LOW | — | Update RTM summary numbers |

---

## Recommendation

### GO-IF for TRR

**Rationale**: All 8 mandatory criteria PASS. The two conditional items (T-PLD-INT-04, sun sensor) are LOW priority and can be tracked as post-TRR action items. The coverage gap is carried from CDR GO-IF and accepted as a known limitation per the CDR closure.

**Conditions for Gate Proceed**:

1. **T-PLD-INT-04** added to RTM-OBC-001 integration test section before ATP
2. **Sun sensor** either gets an SRS requirement or is formally descoped with rationale
3. **FR-19 ±500ms guard** either implemented or accepted with a documented deviation
4. **RTM summary counts** updated to match reality (68 tests, 4 OR)
5. **Coverage < 90%** accepted as CDR carry-over — no new requirement added

### Blocking Items: NONE

---

## Gate Tracking

| Gate | Status | Decision | Date |
|------|--------|----------|------|
| SRR | ✅ CLOSED | GO | 2026-03-10 |
| PDR | ✅ CLOSED | GO | 2026-03-10 |
| CDR | ✅ CLOSED | GO-IF | 2026-07-14 |
| **TRR** | **🔍 THIS REVIEW** | **GO-IF** | **2026-07-15** |
| AR | ❌ NOT STARTED | — | — |
| FRR | ❌ NOT STARTED | — | — |
