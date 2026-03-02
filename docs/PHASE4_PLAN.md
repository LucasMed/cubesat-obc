# Phase 4: Advanced Control — Plan

**Document ID**: PLAN-004  
**Version**: 1.0  
**Last Updated**: 2026-03-02  
**Branch**: `feature/phase4-advanced-control`  
**Status**: In Progress

---

## Overview

Phase 4 replaces the current placeholder algorithms with production-grade estimation and control:

| Current (Phases 1–3) | Phase 4 Target |
|---|---|
| Euler integration only (`attitude_dynamics.c`) | Extended Kalman Filter (EKF) fusing accel + gyro |
| Decoupled PID per axis, `(void)rates` ignored | LQR controller using full state feedback |
| No gyro-bias compensation | EKF estimates and corrects gyro bias online |
| No measurement noise model | Tunable Q/R covariance matrices |

**Duration Estimate**: 3–4 weeks  
**Start Date**: 2026-03-02  
**Target Completion**: Late March / Early April 2026

---

## Architecture Impact

### New modules

```
include/
  ekf.h                  ← EKF state, covariance, API
  lqr.h                  ← LQR gain matrix, controller API
src/control/
  ekf.c                  ← EKF predict + update
  lqr.c                  ← LQR attitude controller
tests/unit/
  test_ekf.c             ← EKF unit tests
  test_lqr.c             ← LQR unit tests
```

### Modified modules

| File | Change |
|------|--------|
| `src/dynamics/attitude_dynamics.c` | Upgrade Euler→ RK2 integration |
| `src/tasks/sensor_read_task.c` | Add EKF update step after raw IMU read |
| `src/tasks/attitude_control_task.c` | Switch from PID to LQR (PID kept as fallback) |
| `include/system_state.h` | Add `gyro_bias[3]`, `attitude_var[3]` (EKF output) |
| `tests/unit/CMakeLists.txt` | Add test_ekf, test_lqr targets |
| `docs/ARCHITECTURE.md` | Update control subsystem description |
| `docs/TRACEABILITY_MATRIX.md` | Update FR-2 status |
| `docs/requirements/SOFTWARE_REQUIREMENTS.md` | Update statuses |

---

## Task Breakdown

### PR-11 — RK2 Dynamics Upgrade
**Files**: `src/dynamics/attitude_dynamics.c`  
**Scope**:
- Replace single-step Euler with 2nd-order Runge-Kutta (RK2)
- Keep same API: `attitude_dynamics_step(ad, torque, dt)` — no callers change
- Update `test_dynamics.c` with energy-conservation check

**Acceptance criteria**:
- [ ] Numerical error < 1e-4 rad for 10 s simulation at 50 Hz
- [ ] `test_dynamics` still passes (17/17 total)

---

### PR-12 — EKF Attitude Estimator
**Files**: `include/ekf.h`, `src/control/ekf.c`, `tests/unit/test_ekf.c`  
**Scope**:

State vector (6 × 1):
```
x = [roll, pitch, yaw, bias_x, bias_y, bias_z]ᵀ
```

Process model:
```
x_dot = f(x, u)
  attitude_dot = rates_measured − bias
  bias_dot     = 0  (random walk, driven by Q)
```

Measurement model (accelerometer):
```
z = h(x) = [roll_accel, pitch_accel]  (yaw unobservable from accel alone)
```

Matrices (tunable at compile time via `config.h`):
- `Q` — process noise covariance (6×6, diagonal)
- `R` — measurement noise covariance (2×2, diagonal)
- `P0` — initial state covariance (6×6, identity × σ²)

Public API:
```c
void ekf_init(ekf_t *ekf);
void ekf_predict(ekf_t *ekf, const float gyro[3], float dt);
void ekf_update(ekf_t *ekf, const float accel[3]);
void ekf_get_attitude(const ekf_t *ekf, float att[3]);
void ekf_get_bias(const ekf_t *ekf, float bias[3]);
```

**Acceptance criteria**:
- [ ] Converges to within ±2° of truth within 5 s from cold start (test_ekf)
- [ ] Gyro-bias estimate converges to injected bias ±0.5°/s within 10 s
- [ ] No dynamic memory allocation

---

### PR-13 — LQR Attitude Controller
**Files**: `include/lqr.h`, `src/control/lqr.c`, `tests/unit/test_lqr.c`  
**Scope**:

State vector (6 × 1):
```
x = [roll_err, pitch_err, yaw_err, roll_rate_err, pitch_rate_err, yaw_rate_err]ᵀ
```

Control law:
```
u = −K · x   (K: 3×6 gain matrix, pre-computed offline via MATLAB/Python)
```

Initial gain matrix K: pre-computed for a 1U CubeSat inertia tensor
`I = diag(0.01, 0.01, 0.005) kg·m²` with default Q/R cost weights.

Public API:
```c
void lqr_init(lqr_t *lqr);
void lqr_set_gains(lqr_t *lqr, const float K[3][6]);
void lqr_compute(lqr_t *lqr,
                 const float att_err[3],
                 const float rate_err[3],
                 float torque_cmd[3]);
```

Controller selection in `attitude_control_task.c`:
- `FLIGHT_MODE_NOMINAL` → LQR
- `FLIGHT_MODE_DETUMBLE` → PID (high-rate de-tumble, simpler)
- `FLIGHT_MODE_SAFE` / `FLIGHT_MODE_BOOT` → zero torque

**Acceptance criteria**:
- [ ] LQR stabilises 30° initial error to < 1° within 30 s in simulation
- [ ] Zero steady-state error for constant disturbance torque ≤ 1e-4 N·m
- [ ] `test_lqr` passes; 17+N/17+N total tests

---

### PR-14 — Sensor Fusion Integration
**Files**: `src/tasks/sensor_read_task.c`, `include/system_state.h`, `include/data_layer.h`  
**Scope**:
- Add `gyro_bias[3]` and `att_uncertainty[3]` fields to `system_state_t`
- `sensor_read_task` calls `ekf_predict()` + `ekf_update()` after every IMU read
- Write EKF-estimated attitude back to DLA (replaces raw accel-integrated attitude)
- Add `imu_ekf_valid` flag to `dl_snapshot_t`

**Acceptance criteria**:
- [ ] EKF runs within `sensor_read_task` 10 Hz budget (<50 ms)
- [ ] All existing DLA-based tests still pass
- [ ] `test_sensor_read_task` updated for EKF path

---

### PR-15 — Attitude Control Task wired to LQR + Documentation
**Files**: `src/tasks/attitude_control_task.c`, docs updates  
**Scope**:
- Replace `attitude_ctrl_update()` PID call with `lqr_compute()` in NOMINAL mode
- Keep `attitude_ctrl_update()` for DETUMBLE (high angular rates)
- Update `ARCHITECTURE.md`, `TRACEABILITY_MATRIX.md`, `SOFTWARE_REQUIREMENTS.md`
- Update `CHANGELOG.md` with Phase 4 entry

**Acceptance criteria**:
- [ ] All 17+ tests still pass
- [ ] Static analysis (clang-format + clang-tidy + cppcheck) clean
- [ ] `FR-2` marked complete in traceability matrix

---

## Dependency Graph

```
PR-11 (RK2)
    │
    ▼
PR-12 (EKF)
    │
    ├──▶ PR-14 (Sensor fusion)
    │
PR-13 (LQR)
    │
    └──▶ PR-15 (Task wiring + docs)
              ↑
         PR-14 (required for EKF-estimated attitude in task)
```

PRs 11 and 13 are independent in implementation, but 14 and 15 both depend on 12 and 13 respectively.

---

## What Is NOT in Phase 4

| Item | Rationale | Phase |
|------|-----------|-------|
| Magnetometer integration for yaw | Hardware not available on Pico 2W | Phase 5 / optional |
| Star tracker | Not applicable to 1U budget | Out of scope |
| Hardware watchdog | Safety feature, separate concern | Phase 5 |
| WiFi telemetry | Communication concern | Phase 5 |
| Reaction wheel real PWM | Hardware driver, separate concern | Phase 5 |

---

## Verification Strategy

| PR | Test type | Tool |
|----|-----------|------|
| PR-11 | Unit — energy conservation | `test_dynamics` |
| PR-12 | Unit — convergence, bias estimation | `test_ekf` (new) |
| PR-13 | Unit — stability, zero steady-state error | `test_lqr` (new) |
| PR-14 | Unit — EKF in task loop, DLA write | `test_sensor_read_task` (updated) |
| PR-15 | Unit — mode-selected controller dispatch | `test_attitude_control_task` (updated) |
| All | Static analysis | CI: clang-format-14, clang-tidy-14, cppcheck |

---

## Pre-requisites Check

| Item | Status |
|------|--------|
| 17/17 unit tests passing on `dev` | ✅ |
| CI pipeline green (format + tidy + cppcheck) | ✅ |
| `data_layer.h` DLA API stable | ✅ |
| `system_state.h` attitude in radians (SPEC-2-DLA §2.4) | ✅ |
| `sensor_read_task.c` already calls DLA write | ✅ |
| `attitude_control_task.c` already reads DLA snapshot | ✅ |
| `flight_mode_manager.c` provides mode for controller selection | ✅ |
| No dynamic memory allocation policy enforced | ✅ |
| Static 6×6 matrix ops — no BLAS/LAPACK needed | ✅ (plain C arrays) |
