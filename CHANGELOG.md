# Changelog

All notable changes to the CubeSat OBC project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

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
