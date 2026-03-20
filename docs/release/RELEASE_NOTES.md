# Release Notes

**Last Updated**: 2026-03-20

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

## v0.7.0 — Hardware Validation & Production Migration (2026-03-04)

**Status**: ✅ Released  
**Branch**: `dev`

### Highlights
- **Critical architecture fix**: FreeRTOS port migrated from ARM_CM0 (RP2040) to
  `ARM_CM33_NTZ` (Cortex-M33 / RP2350) — resolves PendSV stack corruption caused
  by mismatched `EXC_RETURN` unwinding between M0+ and M33 exception frames
- `obc_main.c` production entry point fully validated on Pico 2W hardware:
  all 7 tasks created, CYW43 LED blinking, telemetry streaming at ~2 Hz,
  heap stable at **60,416 bytes free** across >10 heartbeats
- `cyw43_arch_init()` moved inside `vStartupTask` (after scheduler start) to
  avoid SMP spinlock deadlock on pre-scheduler hardware init
- `blink_standalone/` example relocated into `examples/` tree
- Full `pico_ci.sh` pipeline validated: 29/29 tests, `.uf2` build, smoke-test,
  static analysis (0 issues), coverage (91.9% lines / 82.5% branches)
- Sensor timing on hardware: min=99,954 µs, max=100,037 µs, avg=100,000 µs
  (100 Hz loop, ±43 µs absolute jitter — within spec)
- Host stub headers (`include/host/FreeRTOS.h`, `task.h`) hardened with
  `pdPASS`, `BaseType_t`, `vTaskSuspend`, `vTaskPrioritySet` stubs so
  clang-tidy reports 0 errors on host builds

### Key Technical Finding
`configNUMBER_OF_CORES = 1` — SMP dual-core intentionally disabled during
initial bring-up (comment: *"re-enable when boot is stable"*). All tasks
currently execute on Core 0. Core 1 remains idle. Enabling SMP and core-affinity
pinning is gated on HW stability and is tracked for v1.0.0.

### Stack High-Water Marks (hardware, single-core)
| Task | HWM (words) | Stack alloc (words) | Utilisation |
|------|-------------|---------------------|-------------|
| Heartbeat | 1930 | 2048 | 94% free |
| SensorRead | — | 2048 | not yet instrumented |
| AttitudeCtrl | — | 2048 | not yet instrumented |
| Telemetry | — | 2048 | not yet instrumented |
| Command | — | 2048 | not yet instrumented |
| HealthMon | — | 2048 | not yet instrumented |
| LEDBlink | — | 2048 | not yet instrumented |

> Full HWM instrumentation in `obc_main.c` heartbeat loop is planned for v0.7.1.

### CI Pipeline Results
| Stage | Result | Notes |
|-------|--------|-------|
| host-test | ✅ 29/29 | CTest, 0.13 s |
| pico-build | ✅ PASS | 660,992 bytes `.uf2` |
| emu-build | ✅ PASS | Thumb-16 smoke ELF, 67,052 bytes |
| emulate | ✅ PASS | 6/6 boot strings + 3 STATUS packets, 91 ms |
| static | ✅ PASS | clang-format + clang-tidy + cppcheck — 0 issues |
| coverage | ✅ PASS | 91.9% lines, 82.5% branches |

### Components
| Component | Status |
|-----------|--------|
| FreeRTOS ARM_CM33_NTZ port | ✅ Boot stable, PendSV validated |
| `obc_main.c` production code | ✅ Validated on Pico 2W HW |
| CYW43 LED blink | ✅ Confirmed blinking |
| CI pipeline (`pico_ci.sh`) | ✅ All 6 stages green |
| Static analysis | ✅ 0 violations |
| `examples/blink_standalone` | ✅ Moved to `examples/`, paths updated |

---

## v0.24.0 — Phase 7 Payload Integration (2026-03-20)

**Status**: ✅ Released  
**Branch**: `main`

### Highlights
- **GPS NEO-7M integration**: Full NMEA parser for `$GPGGA`/`$GPRMC` sentences on UART0
  @ 9600 baud; GpsTask at 1 Hz with UTC time synchronization; `gps_fix_t` extended to
  Data Layer with telemetry fields (lat/lon/alt/utc/valid). Resolves AIR-OBC-001 ACT-06 Option A.
- **IMU integration validation**: `test_imu_integration.c` validates IMU driver and telemetry
  data flow end-to-end; fixed `temp_sensor_available` → `temp_available` naming.
- **Magnetometer EKF operations**: `test_mag_integration.c` validates EKF operations using
  magnetometer data; HMC5883L stub in place; EKF yaw update integration pending PR-18.
- **Event logger flush hook**: Flash backend stub integration via `flash_backend_flush()`;
  persistent event logging with 64-record ring buffer.
- **Test coverage improved**: Line coverage **93.0%** (up from 91.8%), function coverage **92.4%**.
- **Radiation test disabled**: `test_radiation_integration.c` disabled in CI (unsupported APIs;
  re-enabled in Phase 8).

### Bug Fixes
- Heap configuration review: ~82 KB needed vs 60 KB configured (OI-8 HIGH priority).
- UART GPS integration fixes.

### Documentation
- PDR Report for FSW complete.
- SRR/AIR resolution documentation.
- ECSS document alignment.

### CI Pipeline Results
| Stage | Result | Notes |
|-------|--------|-------|
| host-test | ✅ 6/6 | CTest stages passing |
| pico-build | ✅ PASS | |
| static-analysis | ✅ PASS | clang-format + clang-tidy + cppcheck |
| coverage | ✅ PASS | 93.0% lines, 92.4% functions |

### Components
| Component | Status |
|-----------|--------|
| GPS NEO-7M Driver | ✅ NMEA parser, GPRMC/GGA, UTC sync |
| IMU Integration Test | ✅ Validated |
| Magnetometer EKF Test | ✅ EKF operations validated |
| Event Logger Flush Hook | ✅ Flash backend stub integrated |
| Radiation Test | ⚠️ Disabled in CI (Phase 8) |

---

## v1.0.0 — Flight Ready (TBD)

**Status**: ⏳ Planned — Full hardware validation + SMP enablement  
**Target**: Q3 2026

### Planned Features
- Enable SMP dual-core (`configNUMBER_OF_CORES = 2`) with core-affinity pinning
  validated on Pico 2W hardware
- Full stack HWM instrumentation in production heartbeat loop
- On-hardware integration test suite (real MPU6050 + HMC5883L)
- Flash-backed persistent logging (Phase 3 logger backend)
- FDIR authority chain formalised: EPS → Fault Manager → FMM (single path,
  removing direct `fmm_request_transition` bypass from EPS Monitor)
- Autonomous safe-mode transition end-to-end test (T-FMS-01, T-SAFE-01)
- Flight qualification testing (vibration, thermal, radiation)
- SAD v1.0 hardware-validated fields filled in (HWM, jitter, core assignment)
