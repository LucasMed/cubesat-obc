# Changelog

All notable changes to the CubeSat OBC project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.7.0] - 2026-03-20

### Added (Phase 6 — Closed-Loop Stability & Architectural Consolidation)
- **Quaternion library** (`src/control/quaternion.c`, `include/quaternion.h`): unit-quaternion
  multiply, rotate, normalize, spherical linear interpolation (slerp), and to-Euler conversion;
  replaces ad-hoc Euler math throughout the attitude pipeline (PR-21).
  Five unit tests: T-QAT-01..05.
- **Configurable magnetic declination** (`OBC_MAG_DECLINATION_RAD` in `include/config.h`):
  compile-time declination offset applied inside `ekf_update_mag()`; default 0.0 rad; keeps
  yaw estimate referenced to true north (PR-22). One unit test: T-EKFM-07.
- **LQR gain scheduling** (`src/control/lqr_schedule.c`, `include/lqr_schedule.h`):
  table-driven gain selection by combined energy state and angular momentum magnitude;
  three discrete gain sets (HIGH_ENERGY, NOMINAL, LOW_ENERGY); `lqr_schedule_select()`
  called by `vAttitudeControlTask_Step()` before each control tick (PR-23).
  Three unit tests: T-LQRS-01..03.
- **Closed-loop simulation harness** (`src/control/closed_loop_sim.c`,
  `include/closed_loop_sim.h`): full EKF → LQR → RK2-dynamics simulation loop with
  configurable initial attitude error, noise injection, and stability acceptance criterion
  (settling within 30 s, residual ω < 0.05 rad/s) (PR-24). Six unit tests: T-CLS-01..06.
- **Integration test — fault-to-safe sequence** (`tests/integration/test_fault_safe.c`):
  end-to-end verification that `fault_report(FAULT_ANY, FAULT_LEVEL_CRITICAL)` causes
  FMM to enter FM_SAFE within 100 ms simulation ticks (T-FMS-01a..d, 4 sub-tests) (PR-25).
- **Integration test — watchdog safe-mode trigger** (`tests/integration/test_safe_trigger.c`):
  verifies that `health_monitor_task.c` `watchdog_hal_triggered()` path calls
  `fault_report(FAULT_WDT_KICK_MISSED, FAULT_LEVEL_CRITICAL)` → FM_SAFE
  (T-SAFE-01a..c, 3 sub-tests) (PR-25).
- **Flash-backend stub** (`src/core/flash_backend_stub.c`, `include/flash_backend.h`):
  host-build stub that opens `/tmp/obc_log.bin` and writes packed records via
  `flash_backend_flush()`; hardware path is a `__attribute__((weak))` override (PR-26).
- **Event logger flush hook** (`src/core/event_logger.c`): ring-buffer (64 records × 40 B)
  now calls `flash_backend_flush()` automatically when capacity is reached; constant
  `LOG_RING_CAPACITY 64u` exported in `include/logger.h` (PR-26).
  Four sub-tests: T-LOG-01a..d (one CTest target `event_logger_test`).
- **MISRA C deviations log** (`docs/standards/MISRA_DEVIATIONS.md`): all advisory
  deviations documented with rationale; zero required or mandatory violations (PR-27).

### Changed
- `src/tasks/health_monitor_task.c`: added `watchdog_hal_triggered()` poll →
  `fault_report(FAULT_WDT_KICK_MISSED, FAULT_LEVEL_CRITICAL)` on every task tick (PR-25).
- `include/eps.h`: added `eps_hal_read()` declaration for host-HAL abstraction.
- Multiple files: MISRA C fixes — unused `<stdio.h>` removed, void casts on ignored
  return values, braced single-statement bodies, NULL pointer comparisons (PR-27).

### Testing
- Test suite: **29** CTest executables.  All **29/29** passing.
- New test targets (Phase 6): `quaternion_test`, `lqr_schedule_test`,
  `closed_loop_test`, `ekf_mag_test` (T-EKFM-07 added), `test_fault_safe`,
  `test_safe_trigger`, `event_logger_test`.
- gcovr line coverage: **91.8%** on `src/control/` + `src/core/` + `src/services/`
  combined (target was ≥90%).
- cppcheck, clang-format-14, clang-tidy-14 all clean.

---

## [0.6.0] - 2026-03-12

### Added (Phase 5 — Flight Readiness)
- **Watchdog HAL** (`src/core/watchdog.c`, `include/watchdog.h`): hardware watchdog
  kick/init/enable via `__attribute__((weak))` HAL stubs; host build issues periodic
  log messages; `xTaskGetTickCount()`-gated kick called from `vHealthMonitorTask_Step()`
  (PR-16). Five unit tests: T-WDT-01..05.
- **Momentum Dump** (`src/control/momentum_dump.c`, `include/momentum_dump.h`): angular
  momentum magnitude check, FM_DETUMBLE guard, duty-cycle B-dot dump algorithm.  Dump
  result published to DLA (`data_layer_write_momentum_dump()`); called from
  `vAttitudeControlTask_Step()` (PR-17). Five unit tests: T-MDT-01..05.
- **HMC5883L Magnetometer driver** (`src/drivers/hmc5883l.c`, `include/drivers/hmc5883l.h`):
  I²C 3-axis mag driver with `__attribute__((weak))` HAL stub returning `{25, 0, 42}` µT;
  `vSensorReadTask_Step()` reads mag, converts to µT, writes to DLA via
  `data_layer_write_mag()`.  `system_state_t` extended with `mag_field_uT[3]`,
  `mag_valid`, `mag_timestamp_ms` (PR-18). Four unit tests: T-MAG-01..04.
- **EKF yaw update via magnetometer** (`src/control/ekf.c` / `include/ekf.h`): scalar
  yaw measurement update `ekf_update_mag()` using tilt-compensated H=[0,0,1,0,0,0]
  observation; degenerate-field guard (`|Bh| < 1 µT`); innovation wrapped to `[-π, +π]`;
  `ekf_t` extended with `float r_mag`; `vSensorReadTask_Step()` calls update after each
  mag read (PR-19). Six unit tests: T-EKFM-01..06.

### Testing
- Test suite: **23** CTest executables.  All **23/23** passing.
- New test targets: `watchdog_test`, `momentum_dump_test`, `hmc5883l_test`,
  `ekf_mag_test`.
- cppcheck, clang-format-14 clean on all PR-16..19 files.

---

## [0.5.0] - 2026-03-08

### Added (Phase 4 — Advanced Control)
- **RK2 Dynamics integrator** (`src/dynamics/attitude_dynamics.c`): Euler → midpoint
  (RK2) integration for attitude propagation; reduces attitude error to <0.1 mrad at
  10 Hz (PR-11).  Five unit tests: T-DYN-01..05.
- **EKF attitude estimator** (`src/control/ekf.c`, `include/ekf.h`): 6-state EKF
  `x = [roll, pitch, yaw, bx, by, bz]` with analytic 2×2 S⁻¹ inversion, gyro-bias
  estimation, and accelerometer measurement update (PR-12).
  Six unit tests: T-EKF-01..06.
- **LQR controller** (`src/control/lqr.c`, `include/lqr.h`): 3×6 full-state gain
  matrix `u = -K·x` with analytic default gains for ωn = 10 rad/s, ζ = 1, derived
  for Isat = diag(0.01, 0.01, 0.005) kg·m² (PR-13).
  Seven unit tests: T-LQR-01..07.
- **Sensor fusion integration** (`src/tasks/sensor_read_task.c`): static `ekf_t g_ekf`
  runs EKF predict + update every 10 Hz tick; EKF attitude, gyro bias, and diagonal
  covariance published to DLA via `data_layer_write_ekf()` (PR-14).
  Three new tests: T-SRF-08..10 (10 total for sensor read task).
- **LQR/PID mode dispatch** (`src/tasks/attitude_control_task.c`): FM_NOMINAL +
  `imu_ekf_valid` → LQR; FM_DIAGNOSTIC or EKF converging → PID fallback (PR-15).
  Three new tests: T-ACT-09..11 (11 total for attitude control task).
- **EKF fields in system_state_t**: `gyro_bias[3]`, `att_uncertainty[3]`,
  `imu_ekf_valid` (PR-14).
- **`data_layer_write_ekf()`** DLA write function for EKF outputs (PR-14).

### Testing
- Test suite: 19 CTest executables.  All 19/19 passing.
- clang-format-14, clang-tidy-14, cppcheck all clean.

---

## [0.4.0] - 2026-03-02

### Added
- **Foundation Types** (`include/foundation_types.h`): `FlightMode_t`, `FaultCode_t`, `EpsState_t`, `SubsystemId_t`, `OBCResult_t` — shared enums and status codes used across all subsystems (PR-1).
- **Data Layer Abstraction (DLA)** (`src/core/data_layer.c`): thread-safe `SystemStateStore` with getter/setter API and FreeRTOS mutex protection; replaces direct `system_state_t` struct accesses (PR-2).
- **Flight Mode Manager (FMM)** (`src/core/flight_mode_manager.c`): state machine with 6 operational modes (`SAFE`, `NOMINAL`, `DETUMBLE`, `SCIENCE`, `COMMS`, `LOW_POWER`), priority-based transitions, and inhibit flags (PR-3).
- **Fault Manager (FM)** (`src/core/fault_manager.c`): fault table with severity levels (`INFO`, `WARNING`, `CRITICAL`), auto-escalation to safe mode on critical faults, fault persistence counter, and `fault_manager_tick()` (PR-4).
- **EPS Monitor** (`src/core/eps_monitor.c`): voltage/current threshold checks, energy state calculation (`FULL`, `NOMINAL`, `LOW`, `CRITICAL`), and `eps_monitor_tick()` for periodic health updates (PR-5).
- **Persistent Event Logger** (`src/core/event_logger.c`): ring-buffer event store with severity tagging, query-by-severity API, and `event_logger_flush()` hook for future flash backend (PR-6).
- **Sensor Read Task — DLA migration**: rewired `sensor_read_task.c` to write via DLA setters, removing direct struct coupling (PR-7).
- **Attitude Control Task — DLA migration**: rewired `attitude_control_task.c` to consume flight mode and fault state from DLA, gating actuators on FM guard (PR-8).
- **Telemetry Task — DLA migration**: rewired `telemetry_task.c` to read all fields via DLA getters and embed energy state flags in telemetry packets (PR-9).
- **Health Monitor Task wiring**: integrated `fault_manager_tick()` and `eps_monitor_tick()` calls into `health_monitor_task.c` periodic loop (PR-10).

### Changed
- Architecture aligned with SPEC-2 v2.0 (Data Flow), SPEC-3 (Fault Management), and SPEC-5 (Command & Telemetry Interface).
- All tasks now communicate exclusively through the DLA — no direct `SystemState` struct access in task code.
- Flight-mode-aware actuator inhibit logic added to attitude and sensor tasks.

### Testing
- Test suite extended to 17 CTest executables covering DLA, FMM, FM, EPS Monitor, Event Logger, and task integration.
- 100% tests passing (17/17).

---

## [0.3.0] - 2026-02-22

### Added
- Integrated `libcsp` via a git submodule for cross-platform POSIX and FreeRTOS SMP communication.
- Implemented `pico_usart.c` UART driver mapping libcsp's KISS protocol to the RP2350 hardware UART.
- Rewrote `telemetry_task.c` to accurately pack and send `csp_telemetry_packet_t` payloads over UART.
- Added `command_task.c` to listen for remote commands (`CMD_ECHO`, `CMD_REBOOT`, `CMD_SET_MODE`) and act on system states.
- Reached extensive unit testing suite totaling 8 CTest executables checking parsing logic, packet packing, and initialization routing.
- Increased overall GCC line test coverage from 38% to 64%.

### Changed
- Shifted default debugging output from target UART to UART0 while allocating UART1 explicitly for the libcsp protocol.

## [0.2.0] - 2026-02-20

### Added
- MPU6050 IMU driver and onboard Temperature sensor driver (Pico ADC4)
- Thread-safe system state management using FreeRTOS mutexes
- High-resolution task jitter telemetry (µs precision)
- Integrated sensor-to-control loop in `obc_main.c`

### Changed
- Increased `configMINIMAL_STACK_SIZE` to 4KB and `configTOTAL_HEAP_SIZE` to 128KB for SMP/USB stability
- Forced `-mfloat-abi=soft` to ensure FreeRTOS SMP compatibility on RP2350
- Enabled UART0 and USB CDC stdio with FreeRTOS-aware initialization
- Fixed `pico_flash` header shadowing issues

### Fixed
- Resolved `FATAL: Stack overflow in task 'IDLE0'` caused by FPU context switching mismatch
- Resolved USB CDC enumeration issues on boot

### Known Issues
- Power consumption baseline measurement pending final hardware sign-off

## [0.1.0] - 2026-02-12

### Added
- Initial skeleton implementation
  - Modular architecture following ECSS-Q-ST-80C standards
  - Core subsystems: drivers, actuators, control, dynamics, tasks
  - FreeRTOS task framework with stub implementation for host builds
  - Configuration system (config/FreeRTOSConfig.h)
- Development environment
  - CMake build system with per-module organization
  - Unit testing framework (3 basic tests)
  - CI workflow via GitHub Actions
  - Build automation scripts
- Documentation
  - README with project overview
  - Coding standards (MISRA-like guidelines)
  - Build guide
  - Next steps and verification plan
- Project governance
  - MIT License
  - Contributing guidelines
  - `.gitignore` for embedded projects

### Project Status
**✅ Complete:** Architecture, FreeRTOS integration, testing framework, documentation.

**🔄 In Progress:** N/A

**⏳ TODO (Priority Order):**
1. Pico SDK integration (Pico 2W hardware support)
2. I2C driver for MPU6050 (real sensor readout)
3. WiFi and lwIP integration (telemetry transmission)
4. Kalman filter for attitude estimation
5. Advanced control algorithms (LQR, MPC)
6. Flight-ready hardening (watchdog, safe states, logging)

---

## Development Notes

### Versions 0.1.x Series
- **0.1.0**: Skeleton with FreeRTOS and basic control logic
- **0.2.0**: Pico SDK + hardware driver support
- **0.3.0**: libcsp and Telemetry/Command integration
- **0.4.0**: (Planned) Advanced Control filters and algorithms

### Estimated Timeline
- Phase 2 (Pico SDK): Q1 2026
- Phase 3 (Communication): Feb 2026 (Completed)
- Phase 4 (Advanced Control): Q2-Q3 2026
- Phase 5 (Flight Ready): Q3 2026

---

## Unreleased Commits

View unreleased changes with:
```bash
git log $(git describe --tags --abbrev=0)..HEAD --oneline
```

---

**Last Updated:** 2026-02-22
