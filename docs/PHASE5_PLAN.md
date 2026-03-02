# Phase 5: Flight Readiness — Plan

**Document ID**: PLAN-005  
**Version**: 1.0  
**Last Updated**: 2026-03-08  
**Branch**: `feature/phase5-flight-ready`  
**Status**: In Progress

---

## Overview

Phase 5 hardens the OBC firmware for flight readiness. All work targets the
**host build** (`PICO_ENABLED=OFF`) until hardware is reconnected; every new
module uses a HAL-stub pattern so the same code compiles and tests on Linux and
runs unmodified on the RP2040.

| Area | Phase 4 state | Phase 5 target |
|---|---|---|
| Watchdog | Not wired | HAL abstraction + health-monitor integration |
| Momentum management | No de-saturation | `B × L` dump in FM_DETUMBLE |
| Magnetometer | Not present | HMC5883L driver stub → EKF yaw observability |
| EKF measurement | 2-state (roll+pitch only) | 3-state (roll+pitch+yaw via mag) |
| Requirements doc | Several TBD/TBC | SR-1, SR-2, NFR-1, NFR-2 closed |

**Duration estimate**: 3–4 weeks  
**Start date**: 2026-03-08  
**Target completion**: Early April 2026  
**Hardware dependency**: None (all host-testable via stubs)

---

## Architecture Impact

### New modules

```
include/
  watchdog_hal.h             ← watchdog_init / watchdog_feed / watchdog_triggered
  momentum_dump.h            ← B×L de-saturation API
  drivers/mag/hmc5883l.h     ← HMC5883L I2C magnetometer driver API
src/services/watchdog/
  watchdog_hal_host.c        ← no-op stub (host)
  watchdog_hal_pico.c        ← hardware/watchdog.h wrapper (PICO_BUILD only)
src/services/adcs/
  momentum_dump.c            ← de-saturation torque computation
src/drivers/mag/
  hmc5883l.c                 ← HAL-backed driver (stub on host)
tests/unit/
  test_watchdog.c            ← T-WDG-01..04
  test_momentum_dump.c       ← T-MTM-01..05
  test_hmc5883l.c            ← T-MAG-01..04
  test_ekf_mag.c             ← T-EKFM-01..06 (magnetometer yaw update)
```

### Modified modules

| File | Change |
|------|--------|
| `src/tasks/health_monitor_task.c` | Call `watchdog_feed()` each step |
| `src/tasks/attitude_control_task.c` | Wire `momentum_dump_step()` in FM_DETUMBLE |
| `src/tasks/sensor_read_task.c` | Read HMC5883L; publish `mag_field[3]` + `mag_valid` to DLA |
| `include/system_state.h` | Add `mag_field[3]`, `mag_valid`, `mag_available` |
| `include/data_layer.h` | Add `data_layer_write_mag()` |
| `src/core/data_layer.c` | Implement `data_layer_write_mag()` |
| `include/ekf.h` / `src/control/ekf.c` | Extend measurement model: M = 3 (add yaw from mag) |
| `src/control/CMakeLists.txt` | No change (ekf.c already there) |
| `tests/unit/CMakeLists.txt` | Add 4 new test targets |
| `docs/requirements/SOFTWARE_REQUIREMENTS.md` | Close SR-1, SR-2, NFR-1, NFR-2, FR-7 |
| `docs/ARCHITECTURE.md` | Phase 5 section |
| `CHANGELOG.md` | Add [0.6.0] Phase 5 section |

---

## Task Breakdown

### PR-16 — Watchdog HAL

**Files**: `include/watchdog_hal.h`, `src/services/watchdog/watchdog_hal_host.c`,
`src/tasks/health_monitor_task.c`, `tests/unit/test_watchdog.c`

**Scope**:
```c
void watchdog_hal_init(uint32_t timeout_ms);   // arm watchdog
void watchdog_hal_feed(void);                   // reset countdown
bool watchdog_hal_triggered(void);             // was last boot a watchdog reset?
```

- Host stub: all no-ops; `watchdog_hal_triggered()` returns `false`
- Pico wrapper: delegates to `watchdog_enable()` / `watchdog_update()` / `watchdog_caused_reboot()`
- `health_monitor_task.c`: call `watchdog_hal_feed()` at the top of each `vHealthMonitorTask_Step()`
- New `CMakeLists`: `watchdog_hal_lib` (host: `watchdog_hal_host.c`; PICO_BUILD: `watchdog_hal_pico.c`)

**Tests** (`test_watchdog.c` — T-WDG-01..04):
- T-WDG-01: `watchdog_hal_init()` does not crash on host
- T-WDG-02: `watchdog_hal_feed()` can be called N times without error
- T-WDG-03: `watchdog_hal_triggered()` returns `false` on host
- T-WDG-04: `vHealthMonitorTask_Step()` calls `watchdog_hal_feed()` exactly once per step

**Acceptance criteria**:
- [ ] Existing 19/19 tests still pass
- [ ] 4 new watchdog tests pass
- [ ] `health_monitor_task_test` count increases from 3 → 4

---

### PR-17 — Momentum Dumping (FM_DETUMBLE)

**Files**: `include/momentum_dump.h`, `src/services/adcs/momentum_dump.c`,
`src/tasks/attitude_control_task.c`, `tests/unit/test_momentum_dump.c`

**Scope**:

De-saturation algorithm (`B × L` cross-product):
```
τ_mtq = −k_dump × (B̂ × L_rw)    [N·m / A·m²]
```
Where `B̂` is unit magnetic field vector, `L_rw` is total reaction-wheel angular
momentum. Output is a magnetic dipole command passed to `magnetorquer_set_dipole()`.

```c
typedef struct { float L[3]; } momentum_state_t;   // RW angular momentum [kg·m²/s]

void momentum_dump_init(momentum_dump_t *md, float k_dump);
void momentum_dump_step(momentum_dump_t *md,
                        const float B[3],           // body-frame B-field [T]
                        const float L_rw[3],        // RW momentum [kg·m²/s]
                        float dipole_cmd[3]);        // output [A·m²]
bool momentum_dump_needed(const float L_rw[3], float threshold);
```

- `attitude_control_task.c`: in FM_DETUMBLE, read `B_field` (from DLA, populated by PR-19)
  and reaction-wheel momentum (estimated from integrated torque); call `momentum_dump_step()`;
  pass `dipole_cmd` to `magnetorquer_set_dipole()`

**Tests** (`test_momentum_dump.c` — T-MTM-01..05):
- T-MTM-01: Zero momentum → zero dipole command
- T-MTM-02: Momentum along +Z, B along +X → dipole orthogonal to both
- T-MTM-03: `momentum_dump_needed()` returns false when `|L| < threshold`
- T-MTM-04: `momentum_dump_needed()` returns true when `|L| ≥ threshold`
- T-MTM-05: Cross-product linearity — doubling `L_rw` doubles dipole magnitude

**Acceptance criteria**:
- [ ] 5 new momentum dump tests pass
- [ ] Existing 23 tests still pass

---

### PR-18 — HMC5883L Magnetometer Driver Stub

**Files**: `include/drivers/mag/hmc5883l.h`, `src/drivers/mag/hmc5883l.c`,
`include/system_state.h`, `include/data_layer.h`, `src/core/data_layer.c`,
`src/tasks/sensor_read_task.c`, `tests/unit/test_hmc5883l.c`

**Scope**:

```c
int  hmc5883l_init(void);                        // returns 0 on success
int  hmc5883l_read(float field_uT[3]);           // returns 0, populates µT values
```

- Host stub: `hmc5883l_init()` always returns 0; `hmc5883l_read()` returns a
  fixed vector `{25.0f, 0.0f, 42.0f}` µT (test-injectable via weak symbol)
- Pico: full I2C implementation (same pattern as `mpu6050.c`)
- `system_state_t`: add `float mag_field[3]`, `bool mag_available`, `bool mag_valid`
- `data_layer_write_mag(const float field_uT[3])` — sets `mag_valid = true`
- `sensor_read_task.c`: after IMU read, read magnetometer and write to DLA

**Tests** (`test_hmc5883l.c` — T-MAG-01..04):
- T-MAG-01: `hmc5883l_read()` populates all 3 field components as finite values
- T-MAG-02: `data_layer_write_mag()` sets `mag_valid = true` in snapshot
- T-MAG-03: Sensor skipped when `mag_available == false`
- T-MAG-04: Driver failure leaves `mag_valid = false`

**Acceptance criteria**:
- [ ] 4 new mag tests pass
- [ ] sensor_read_task test count increases (mag path covered)

---

### PR-19 — EKF Yaw Update via Magnetometer

**Files**: `include/ekf.h`, `src/control/ekf.c`, `tests/unit/test_ekf_mag.c`

**Scope**:

Extend EKF measurement model from M=2 (roll+pitch) to M=3 (roll+pitch+yaw):
```
z[2] = atan2(By_body, Bx_body)   ← yaw from level-corrected mag
H[2] = [0, 0, 1, 0, 0, 0]        ← only yaw state observed
```

New API call:
```c
void ekf_update_mag(ekf_t *ekf, const float mag_field_uT[3], float declination_rad);
```

Called from `sensor_read_task` after `ekf_update()` when `mag_valid == true`.

**Tests** (`test_ekf_mag.c` — T-EKFM-01..06):
- T-EKFM-01: `ekf_update_mag()` does not corrupt roll/pitch estimates
- T-EKFM-02: Yaw covariance P[2][2] decreases after `ekf_update_mag()` call
- T-EKFM-03: Known B-field (North = +X) with zero declination → yaw ≈ 0 rad
- T-EKFM-04: Known B-field (90° rotated) → yaw ≈ π/2 rad (within 0.01 rad)
- T-EKFM-05: Degenerate B-field (near-zero horizontal component) → update skipped
- T-EKFM-06: `imu_ekf_valid` true only when at least one `ekf_update` or `ekf_update_mag` has run

**Acceptance criteria**:
- [ ] 6 new EKF-mag tests pass
- [ ] Existing T-EKF-01..06 still pass (no regression)

---

### PR-20 — Flight Readiness Docs + Requirements Close-Out

**Files**: `docs/requirements/SOFTWARE_REQUIREMENTS.md`, `docs/ARCHITECTURE.md`,
`CHANGELOG.md`, `docs/release/RELEASE_NOTES.md`, `docs/PROJECT_PROGRESS.md`,
`docs/TRACEABILITY_MATRIX.md`, `docs/test_plans/TEST_PLANS.md`

**Scope**:
- Close SR-1 (watchdog — PR-16), SR-2 (safe mode — already FM_SAFE via FMM)
- Close NFR-1 (20 Hz control loop determinism — verified host timing), NFR-2 (10 Hz sensor read)
- Update FR-7 status to "stub complete, hardware pending"
- Add Phase 5 row to traceability matrix (T-WDG, T-MTM, T-MAG, T-EKFM)
- `RELEASE_NOTES.md` v0.6.0 candidate
- `CHANGELOG.md` [0.6.0] section

---

## PR Schedule

| PR | Title | Est. effort | Dependencies |
|----|-------|-------------|--------------|
| PR-16 | Watchdog HAL + health monitor | 2 days | — |
| PR-17 | Momentum dumping (DETUMBLE) | 3 days | PR-16 |
| PR-18 | HMC5883L driver stub + DLA | 2 days | — |
| PR-19 | EKF yaw update (magnetometer) | 3 days | PR-18 |
| PR-20 | Docs finalization + requirements | 1 day | PR-16..19 |

---

## Test Count Projection

| After PR | Total tests |
|----------|-------------|
| PR-15 (current) | 19/19 |
| PR-16 | 23/23 (+4 watchdog) |
| PR-17 | 28/28 (+5 momentum dump) |
| PR-18 | 32/32 (+4 magnetometer) |
| PR-19 | 38/38 (+6 EKF-mag) |

---

## Open Questions

1. **Magnetic declination**: Use a fixed compile-time constant for now
   (`MAG_DECLINATION_RAD` in `config.h`) — runtime update left for Phase 6.
2. **RW momentum tracking**: PR-17 will integrate torque command over time to
   estimate `L_rw`. Actual encoder feedback deferred to hardware integration phase.
3. **Pico watchdog timeout**: Default 2000 ms (2 s) — health monitor runs at 1 Hz,
   so 2 s gives one missed beat before reset.

---

**Last Updated**: 2026-03-08  
**Author**: OBC Development Team
