# Phase 6: Closed-Loop Stability & Architectural Consolidation — Plan

**Document ID**: PLAN-006  
**Version**: 1.0  
**Last Updated**: 2026-03-20  
**Branch**: `feature/phase6-closed-loop`  
**Status**: ✅ Complete  
**Depends on**: Phase 5 complete (`dev` @ `c4e5b4d`)

---

## Overview

Phase 6 closes the gap between "host-testable flight software" and
"verifiable closed-loop control system". It has two parallel tracks:

**Track A — Closed-Loop Stability**  
Verify that the full EKF → LQR → actuator pipeline is stable under
simulated perturbations: initial attitude errors, gyro-bias injection,
magnetometer noise, and momentum saturation events. All verification runs
on the host build; hardware-in-the-loop (HIL) is deferred to Phase 7.

**Track B — Architectural Consolidation**  
Eliminate the remaining technical-debt items identified during Phase 5
review: integration test gaps (T-FMS-01, T-SAFE-01), Euler-angle
singularity risk, fixed LQR gains, and the unfinished flash-backend for
the event logger.

| Area | Phase 5 state | Phase 6 target |
|------|--------------|----------------|
| Closed-loop test | None (unit tests only) | Simulated EKF+LQR loop, settling-time assertion |
| Attitude representation | Euler RPY (small-angle) | Quaternion option wired in, selectable at compile time |
| LQR gains | Fixed (ωn=10, ζ=1) | Gain-scheduling table per flight mode |
| EKF declination | Hard-coded 0.0 rad | Per-mission config via `config.h` |
| Integration tests | 2 done, 4 planned | T-FMS-01 + T-SAFE-01 implemented |
| Event logger backend | Ring buffer only | Flash-backed flush hook (`flash_backend_stub.c`) |
| Line coverage | ~88% (estimated) | ≥ 90% measured via gcovr |
| MISRA C audit | Not performed | cppcheck --addon=misra pass on `src/` |

**Duration estimate**: 4–5 weeks  
**Start date**: 2026-03-15 (pending Phase 5 stabilisation on `dev`)  
**Target completion**: Late April 2026  
**Hardware dependency**: None for Track A+B; optional Pico 2W for Phase 7 HIL

---

## Architecture Impact

### New modules

```
src/control/
  closed_loop_sim.c          ← host-only simulation harness: propagates
                                attitude dynamics, feeds EKF, applies LQR
                                torque, records history for assertions
  lqr_schedule.c             ← gain table keyed by FlightMode_t
include/
  closed_loop_sim.h
  lqr_schedule.h
  quaternion.h               ← q_mult, q_normalize, q_to_euler, euler_to_q
src/control/
  quaternion.c               ← unit-quaternion attitude representation
src/core/
  flash_backend_stub.c       ← weak-symbol flush hook (writes to /tmp on host)
tests/unit/
  test_closed_loop.c         ← T-CLS-01..06
  test_lqr_schedule.c        ← T-LQRS-01..03
  test_quaternion.c          ← T-QAT-01..05
tests/integration/
  test_safe_trigger.c        ← T-SAFE-01
  test_fault_safe.c          ← T-FMS-01
```

### Modified modules

| File | Change |
|------|--------|
| `include/config.h` | Add `OBC_MAG_DECLINATION_RAD` per-mission constant |
| `src/control/ekf.c` | Use `OBC_MAG_DECLINATION_RAD` instead of caller-supplied 0.0 f |
| `src/control/lqr.c` | Accept gain pointer from `lqr_schedule_get(mode)` |
| `src/core/event_logger.c` | Call `flash_backend_flush()` weak hook after ring-buffer wrap |
| `src/tasks/attitude_control_task.c` | Call `lqr_schedule_get(current_mode)` before LQR update |
| `tests/unit/CMakeLists.txt` | Add new test targets |
| `tests/integration/CMakeLists.txt` | Add T-FMS-01, T-SAFE-01 |

---

## Work Breakdown

### PR-21 — Quaternion utility library

**Goal**: Provide unit-quaternion arithmetic as an optional utility.  
Euler RPY stays the primary EKF state representation; quaternions are used
for simulation cross-checks and forward-compat.

**Files**:
- `include/quaternion.h` — `quat_t {w,x,y,z}`, `q_mult`, `q_normalize`,
  `q_conj`, `q_to_euler`, `euler_to_q`, `q_rotate_vec`
- `src/control/quaternion.c` — implementation, all pure math, no RTOS dep
- `tests/unit/test_quaternion.c` — T-QAT-01..05

**Tests (T-QAT-01..05)**:
| ID | Description |
|----|-------------|
| T-QAT-01 | `euler_to_q` → `q_to_euler` round-trip within ±1 µrad |
| T-QAT-02 | `q_mult` identity: `q ⊗ q⁻¹ = [1,0,0,0]` |
| T-QAT-03 | `q_normalize` output has unit norm |
| T-QAT-04 | `q_rotate_vec` rotates [1,0,0] by 90° yaw → [0,1,0] |
| T-QAT-05 | Singularity-free at pitch = ±90° (vs. Euler gimbal lock) |

---

### PR-22 — Configurable magnetic declination

**Goal**: Replace hard-coded `0.0f` declination in `ekf_update_mag()` call
sites with `OBC_MAG_DECLINATION_RAD` from `config.h`.

**Files**:
- `include/config.h` — add `#define OBC_MAG_DECLINATION_RAD 0.0f` with
  comment referencing [NOAA IGRF](https://www.ngdc.noaa.gov/geomag/calculators/magcalc.shtml)
- `src/tasks/sensor_read_task.c` — pass `OBC_MAG_DECLINATION_RAD`
- `tests/unit/test_ekf_mag.c` — add T-EKFM-07: non-zero declination shifts
  measured yaw by the expected offset

**Tests (T-EKFM-07)**:
| ID | Description |
|----|-------------|
| T-EKFM-07 | Declination +10° → `yaw_meas` offset +10° vs. zero-declination |

---

### PR-23 — LQR gain scheduling

**Goal**: Select gain matrix based on current flight mode so that detumble
(FM_DETUMBLE) uses aggressive gains and science mode (FM_SCIENCE) uses
low-torque smooth gains.

**Files**:
- `include/lqr_schedule.h` — `lqr_schedule_init()`,
  `lqr_schedule_get(FlightMode_t) → const float(*)[6]`
- `src/control/lqr_schedule.c` — 3-entry table: NOMINAL, DETUMBLE, default
- `src/tasks/attitude_control_task.c` — call `lqr_schedule_get` before
  `lqr_update()`
- `tests/unit/test_lqr_schedule.c` — T-LQRS-01..03

**Tests (T-LQRS-01..03)**:
| ID | Description |
|----|-------------|
| T-LQRS-01 | FM_NOMINAL returns default gains (ωn=10, ζ=1) |
| T-LQRS-02 | FM_DETUMBLE returns high-bandwidth gains |
| T-LQRS-03 | Unknown mode falls back to default gains (no crash) |

---

### PR-24 — Closed-loop simulation harness + stability tests

**Goal**: Integrate the full EKF → LQR pipeline in a deterministic host
simulation and assert closed-loop stability metrics.

**Simulation model**:
```
x0 = {roll=30°, pitch=20°, yaw=45°, bx=0.01, by=-0.01, bz=0.005}
For each step k (dt=0.05 s, 200 steps = 10 s):
  1. ekf_predict(&ekf, gyro_true + noise, dt)
  2. ekf_update(&ekf, accel_from_attitude)
  3. ekf_update_mag(&ekf, mag_from_yaw, OBC_MAG_DECLINATION_RAD)
  4. lqr_update(&lqr, ekf.x, torque)
  5. attitude_dynamics_step(torque, dt)   ← RK2
  6. record error norm ‖x‖
```

**Tests (T-CLS-01..06)**:
| ID | Description | Pass criterion |
|----|-------------|----------------|
| T-CLS-01 | Roll error < 2° after 10 s from 30° initial | ‖e_r‖ < 2° |
| T-CLS-02 | Pitch error < 2° after 10 s from 20° initial | ‖e_p‖ < 2° |
| T-CLS-03 | Yaw error < 5° after 10 s from 45° initial | ‖e_y‖ < 5° |
| T-CLS-04 | Gyro bias converges: ‖b_est - b_true‖ < 0.005 rad/s after 20 s | bias error |
| T-CLS-05 | Torque saturates at ≤ τ_max = 1e-3 N·m (no actuator overload) | |
| T-CLS-06 | Momentum dump triggered when ‖h‖ > threshold during detumble | |

---

### PR-25 — Integration tests: T-FMS-01 + T-SAFE-01

**Goal**: Close the two highest-priority integration test gaps from the
traceability matrix.

**T-FMS-01 — Fault → SAFE mode end-to-end** (`tests/integration/test_fault_safe.c`):
```
data_layer_init()
fault_manager_init()
flight_mode_manager_init()
// inject CRITICAL fault
fault_manager_report(FAULT_IMU_FAILURE, FAULT_SEVERITY_CRITICAL)
fault_manager_tick()
CHECK(data_layer_get_flight_mode() == FM_SAFE)
CHECK(data_layer_get_fault_active(FAULT_IMU_FAILURE))
```

**T-SAFE-01 — Autonomous SAFE-mode re-entry** (`tests/integration/test_safe_trigger.c`):
```
// Simulate watchdog-expired condition (HAL stub sets triggered flag)
watchdog_hal_set_triggered(true)   // test-only injection
vHealthMonitorTask_Step()
CHECK(data_layer_get_flight_mode() == FM_SAFE)
CHECK(event_logger_count_by_severity(LOG_CRITICAL) >= 1)
```

---

### PR-26 — Flash-backend stub + event logger flush hook

**Goal**: Wire `event_logger_flush()` to a weak `flash_backend_flush()`
function; host stub writes to a temp file; Pico build uses flash_range_program.

**Files**:
- `include/flash_backend.h` — `flash_backend_flush(const uint8_t*, size_t)`
- `src/core/flash_backend_stub.c` — writes to `/tmp/obc_log.bin` on host
- `src/core/event_logger.c` — call `flash_backend_flush()` on ring wrap

---

### PR-27 — Coverage measurement + MISRA audit

**Goal**: Measure line coverage with gcovr and run cppcheck MISRA addon on
all `src/` files; fix any violations found.

```bash
# Coverage
cmake -S . -B build_cov -DPICO_ENABLED=OFF -DCMAKE_C_FLAGS="--coverage"
cmake --build build_cov --parallel 4
cd build_cov && ctest
gcovr -r ../src . --html --html-details -o coverage.html
# Target: ≥ 90% line coverage on src/control/, src/core/, src/services/

# MISRA
cppcheck --addon=misra --suppress=missingIncludeSystem \
  --enable=all src/ 2>&1 | grep -v "^$"
```

**Exit criteria**: 0 MISRA required/mandatory violations; advisory violations
documented with rationale in `docs/standards/MISRA_DEVIATIONS.md`.

---

## Test Plan Summary

| PR | New Tests | Total after |
|----|-----------|-------------|
| PR-21 | T-QAT-01..05 (5) | 28 |
| PR-22 | T-EKFM-07 (1) | 29 |
| PR-23 | T-LQRS-01..03 (3) | 32 |
| PR-24 | T-CLS-01..06 (6) | 38 |
| PR-25 | T-FMS-01, T-SAFE-01 (2 integration) | 40 |
| PR-26 | logger flush test (1) | 41 |
| PR-27 | coverage + audit (no new test target) | 41 |

**Target**: 41 CTest targets, all passing; line coverage ≥ 90%; 0 MISRA mandatory violations.

---

## Requirements Traceability

| PR | Requirement closed |
|----|-------------------|
| PR-21 | FR-2 (attitude singularity-free representation) |
| PR-22 | FR-2 (full magnetic declination model) |
| PR-23 | FR-4 (mode-dependent control gains) |
| PR-24 | FR-3 + FR-4 (closed-loop stability verified analytically) |
| PR-25 | SYS-REQ-4 (fault-tolerant auto safe-mode, T-FMS-01 + T-SAFE-01) |
| PR-26 | FR-9 (persistent event logging with flash backend) |
| PR-27 | NFR-3 (≥ 90% line coverage), NFR-5 (MISRA C compliance) |

---

## Definition of Done

- [x] All 41 CTest targets pass on `dev` (host build, `PICO_ENABLED=OFF`)
- [x] gcovr line coverage ≥ 90% on `src/control/`, `src/core/`, `src/services/`
- [x] cppcheck MISRA addon: 0 required/mandatory violations
- [x] clang-tidy + clang-format-14 clean on all new/modified files
- [x] Integration tests T-FMS-01 + T-SAFE-01 pass
- [x] Closed-loop simulation: all T-CLS-01..06 pass with margins ≥ 20%
- [x] `CHANGELOG.md` updated with `[0.7.0]` section
- [x] `RELEASE_NOTES.md` v0.7.0 section added
- [x] `TRACEABILITY_MATRIX.md` updated (version 2.3)
- [x] This document status set to **Complete**

---

## Risks & Mitigations

| Risk | Likelihood | Impact | Mitigation |
|------|-----------|--------|-----------|
| Closed-loop sim shows instability for some initial conditions | Medium | High | Tune Q/R EKF matrices; adjust LQR gain via `lqr_schedule` before asserting |
| MISRA audit reveals widespread advisory violations | Low | Medium | Log all advisory deviations in `MISRA_DEVIATIONS.md`; fix required/mandatory only |
| Quaternion migration breaks existing EKF tests | Low | Medium | Quaternion lib is additive only; EKF state stays Euler in Phase 6 |
| Flash backend not testable without Pico hardware | N/A | Low | Host stub writes to `/tmp`; real test deferred to Phase 7 HIL |

---

**Last Updated**: 2026-03-12  
**Author**: SW Team  
**Reviewed by**: TBD
