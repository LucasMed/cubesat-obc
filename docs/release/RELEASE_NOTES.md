# Release Notes

**Last Updated**: 2026-02-20

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

## v0.2.0 — Pico SDK Integration (TBC)

**Status**: 🔄 In Progress  
**Branch**: `feature/pico-sdk-integration`  
**Target**: Early March 2026

### Planned Highlights
- Pico SDK fully integrated with CMake
- Real FreeRTOS kernel on RP2350 (dual-core SMP)
- I2C drivers for MPU6050 and temperature sensor
- Hardware validation on Pico 2W

### Current Progress
| Component | Status |
|-----------|--------|
| Pico SDK Setup (Task 2.1) | ✅ Blink test verified |
| FreeRTOS Kernel (Task 2.2) | ✅ Integrated, LED validated |
| I2C Drivers (Task 2.3) | ⏳ Next task |
| System Integration (Task 2.4) | ⏳ Pending |
| HW Validation (Task 2.5) | ⏳ Pending |

### Known Issues
- `pico_flash` library conflicts with FreeRTOS headers — OBC firmware task scheduling pending resolution

---

## v0.3.0 — Communication & Telemetry (TBD)

**Status**: ⏳ Planned — Phase 3  
**Target**: Q2 2026

### Planned Features
- WiFi/CYW43 integration
- lwIP TCP/IP stack for telemetry
- UART debug logging
- Ground station protocol
- Telemetry packet format

---

## v0.4.0 — Advanced Control (TBD)

**Status**: ⏳ Planned — Phase 4  
**Target**: Q2-Q3 2026

### Planned Features
- Kalman filter for attitude estimation
- LQR/MPC control law options
- Momentum dumping strategy
- Extended unit test suite

---

## v0.5.0 — Flight Ready (TBD)

**Status**: ⏳ Planned — Phase 5  
**Target**: Q3 2026

### Planned Features
- Watchdog timer integration
- Autonomous safe-mode
- Configuration management
- Comprehensive logging
- Flight qualification testing
