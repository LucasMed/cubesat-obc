# Release Notes

**Last Updated**: 2026-03-01

---

## v0.1.0 — Skeleton Release (2026-02-12)

**Status**: ✅ Released  
**Branch**: `main`

### Highlights
- Initial skeleton implementation of CubeSat OBC flight software
- FreeRTOS task framework with 4 concurrent tasks (stub on host)
- PID controller, attitude dynamics, and actuator models
- 3 unit tests passing (100%)
- CI/CD via GitHub Actions

### Components
| Component | Status |
|-----------|--------|
| Architecture | ✅ ECSS-Q-ST-80C compliant |
| FreeRTOS Tasks | ✅ Stubs on host |
| Control System | ✅ PID + dynamics |
| Unit Tests | ✅ 3/3 passing |
| Documentation | ✅ Complete |

---

## v0.2.0 — Pico SDK Integration (2026-02-20)

**Status**: ✅ Released  
**Branch**: `dev`

### Highlights
- Pico SDK fully integrated with CMake
- Real FreeRTOS SMP kernel on RP2350 (dual-core Cortex-M33)
- I2C drivers for MPU6050 (6-DOF IMU) and temperature sensor
- Hardware validation on Pico 2W (jitter ±22µs @ 20 Hz)
- USB CDC + UART diagnostics

### Components
| Component | Status |
|-----------|--------|
| Pico SDK Setup | ✅ Blink test verified on HW |
| FreeRTOS SMP Port | ✅ Integrated, LED validated |
| I2C Drivers (MPU6050 + temp) | ✅ Verified |
| System Integration | ✅ End-to-end control loop stable |
| HW Validation | ✅ Jitter ±22µs confirmed |

---

## v0.3.0 — Communication & Telemetry (2026-02-22)

**Status**: ✅ Released  
**Branch**: `dev`

### Highlights
- `libcsp` compiled for FreeRTOS SMP on RP2350
- `pico_usart` bridge — CSP over UART1 via KISS framing
- `telemetry_task` emits packed binary packets at 1 Hz
- `command_task` handles Echo and Reboot commands on Port 20
- 64% project line coverage; `tests/unit/test_telemetry.c`, `test_command.c`, `test_comm_init.c` passing

---

## v0.4.0 — Spec-Review Alignment — PRs 1–10 (2026-03-01)

**Status**: ✅ Released (10/10 PRs committed)  
**Branch**: `feature/spec-review-alignment`

### Highlights
- Closed all SPEC-2 v2.0 gaps identified in the REVISION_PHASE: Data Layer, FMM, Fault Manager, EPS Monitor, Logger, Task migrations
- 17/17 unit tests passing (was 6/6); 0 regressions after each PR
- All new components follow `__attribute__((weak))` HAL pattern for host testability
- Schmidt-trigger hysteresis in EPS Monitor — avoids flapping on boundary voltages
- Unified 320-entry ring logger with Class-A (critical) eviction protection
- Sensor gyro data now stored in rad/s throughout the pipeline (was deg/s)
- Telemetry Task migrated to DLA with FM guard (FM_SAFE → HK-only) and energy state in flags bits[3:2]
- Health Monitor now calls `fault_manager_tick()` and `eps_monitor_tick()` on every step
- `obc_main.c` initialises `fault_manager` and `eps_monitor` before task creation

### Components
| PR | Description | Tests | Commit |
|----|-------------|-------|--------|
| PR-1 | Foundation Types (7 headers) | types_test: ✅ | `76bf234` |
| PR-2 | Data Layer Abstraction | data_layer_test: ✅ | `42709b4` |
| PR-3 | Flight Mode Manager | fmm_test: 11/11 ✅ | `36831b7` |
| PR-4 | Fault Manager | fault_manager_test: 12/12 ✅ | `f71d021` |
| PR-5 | EPS Monitor | eps_monitor_test: 12/12 ✅ | `2ea9440` |
| PR-6 | Persistent Logger | logger_test: 12/12 ✅ | `02d779e` |
| PR-7 | Sensor Read Task → DLA | sensor_read_task_test: 7/7 ✅ | `1763bdd` |
| PR-8 | Attitude Control Task → DLA | attitude_control_task_test: 8/8 ✅ | `9592c7b` |
| PR-9 | Telemetry Task → DLA + FM guard + energy flags | telemetry_test: 6/6 ✅ | `4b89aee` |
| PR-10 | Health Monitor tick wiring + `obc_main` init | health_monitor_task_test: 3/3 ✅ | `e59b91b` |

---

## v0.5.0 — Advanced Control (2026-03-08)

**Status**: ✅ Released (5/5 PRs committed)  
**Branch**: `feature/phase4-advanced-control`

### Highlights
- 6-state EKF `x = [roll, pitch, yaw, bx, by, bz]` with gyro-bias estimation and
  accelerometer measurement update (PR-12)
- RK2 midpoint integrator replaces Euler for attitude dynamics (PR-11)
- LQR full-state controller `u = -Kx` with ωn=10 rad/s, ζ=1 default gains (PR-13)
- EKF wired into `sensor_read_task`; LQR/PID dispatch in `attitude_control_task` (PR-14..15)
- 19 unit tests, all passing

### Components
| PR | Description | Tests | Commit |
|----|-------------|-------|--------|
| PR-11 | RK2 dynamics integrator | dynamics_test: 5/5 ✅ | `93a8559` |
| PR-12 | EKF estimator (6-state) | ekf_test: 6/6 ✅ | `5812ba1` |
| PR-13 | LQR controller | lqr_test: 7/7 ✅ | `916672d` |
| PR-14 | Sensor fusion — EKF in sensor_read_task | sensor_read_task_test: 10/10 ✅ | `96f969d` |
| PR-15 | LQR/PID dispatch | attitude_control_task_test: 11/11 ✅ | `254bde1` |

---

## v0.6.0 — Flight Readiness Hardware Abstraction (2026-03-12)

**Status**: ✅ Released (4/4 feature PRs + docs committed)  
**Branch**: `feature/phase5-flight-ready`

### Highlights
- Hardware watchdog kick wired into `vHealthMonitorTask_Step()` via weak-symbol HAL
  (PR-16); completes SYS-REQ-4 (fault-tolerant safe-mode re-entry)
- Momentum dump algorithm with FM_DETUMBLE guard and B-dot duty-cycle law (PR-17)
- HMC5883L magnetometer driver with I²C HAL stub; mag data published to DLA (PR-18)
- EKF yaw now observable: tilt-compensated scalar yaw update `ekf_update_mag()` via
  `H=[0,0,1,0,0,0]`; convergence verified <5° in 10 s simulation (PR-19)
- 23 unit tests, all 23/23 passing
- All components host-testable (PICO_ENABLED=OFF), no hardware required

### Components
| PR | Description | Tests | Commit |
|----|-------------|-------|--------|
| PR-16 | Watchdog HAL (kick + enable + HAL stub) | watchdog_test: 5/5 ✅ | `64d5f0e` |
| PR-17 | Momentum dump (B-dot, FM guard, DLA write) | momentum_dump_test: 5/5 ✅ | `b1bf725` |
| PR-18 | HMC5883L driver + DLA mag fields | hmc5883l_test: 4/4 ✅ | `cfea46a` |
| PR-19 | EKF yaw update via magnetometer tilt compensation | ekf_mag_test: 6/6 ✅ | `4ee1213` |

---

## v1.0.0 — Flight Ready (TBD)

**Status**: ⏳ Planned — Full hardware validation  
**Target**: Q3 2026

### Planned Features
- On-hardware integration test suite (real MPU6050 + HMC5883L)
- Flash-backed persistent logging (Phase 3 logger backend)
- Flight qualification testing (vibration, thermal, radiation)
- Autonomous safe-mode transition end-to-end test (T-FMS-01, T-SAFE-01)
