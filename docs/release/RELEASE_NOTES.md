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

## v0.4.0 — Spec-Review Alignment — PRs 1–8 (2026-03-01)

**Status**: 🔄 In Progress (8/10 PRs committed)  
**Branch**: `feature/spec-review-alignment`

### Highlights
- Closed SPEC-2 v2.0 gaps: Data Layer, FMM, Fault Manager, EPS Monitor, Logger, Task migrations
- 16/16 unit tests passing (was 6/6); 0 regressions after each PR
- All new components follow `__attribute__((weak))` HAL pattern for host testability
- Schmidt-trigger hysteresis in EPS Monitor — avoids flapping on boundary voltages
- Unified 320-entry ring logger with Class-A (critical) eviction protection
- Sensor gyro data now stored in rad/s throughout the pipeline (was deg/s)

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

### Pending in this series
- PR-9: Telemetry Task → DLA migration
- PR-10: Health Monitor + Watchdog integration

---

## v0.5.0 — Advanced Control (TBD)

**Status**: ⏳ Planned — Phase 4  
**Target**: Q2-Q3 2026

### Planned Features
- Kalman filter for attitude estimation
- LQR/MPC control law options
- Momentum dumping strategy
- Extended unit test suite

---

## v1.0.0 — Flight Ready (TBD)

**Status**: ⏳ Planned — Phase 5  
**Target**: Q3 2026

### Planned Features
- Watchdog timer integration (PR-10 foundation)
- Autonomous safe-mode transitions (FMM + Fault Manager complete)
- Configuration management
- Flash-backed persistent logging (Phase 3 logger backend)
- Flight qualification testing
