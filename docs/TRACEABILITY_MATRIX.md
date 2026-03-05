# Requirements Traceability Matrix

## Overview

This matrix maps functional and non-functional requirements to implementation modules and test cases, ensuring complete coverage and testability.

---

## Functional Requirements

| ID | Requirement | Description | Module(s) | Test Case(s) | Status |
|----|----|-----------|-----------|------------|--------|
| **FR-1** | Attitude Sensing | Read 6-DOF IMU (accel + gyro) via I2C at 10 Hz | `drivers/imu/mpu6050.c`, `tasks/sensor_read_task.c` (DLA-migrated PR-7) | `test_sensor_read_task` (DLA write, rad/s conversion), integration Phase 2 | ✅ Ready |
| **FR-2** | Attitude Determination | Compute Euler angles (roll, pitch, yaw) from sensor data | `control/attitude_control.c`, `dynamics/attitude_dynamics.c` | `test_dynamics` (Euler integration), Phase 4 Kalman test | ✅ Implemented |
| **FR-3** | Rate Control | Stabilize angular rates via PID loops (3 axes: roll, pitch, yaw) | `control/pid_controller.c` | `test_pid` (PID math, anti-windup) | ✅ 100% Pass |
| **FR-4** | Attitude Control | Command desired attitude and compute torque setpoints | `control/attitude_control.c`, `tasks/attitude_control_task.c` (DLA-migrated PR-8) | `test_attitude_control_task` (FM guard, imu_valid guard), `test_dynamics` | ✅ Ready |
| **FR-5** | RW Actuation | Apply torque commands to reaction wheel motors (3-axis) | `actuators/reaction_wheel.c` | `test_actuators` (torque-to-momentum conversion) | ✅ 100% Pass |
| **FR-6** | Magnetorquer Actuation | Apply magnetic dipole commands (3-axis) for de-saturation | `actuators/magnetorquer.c` | `test_actuators` (dipole output validation) | ✅ 100% Pass |
| **FR-7** | Telemetry TX | Transmit attitude, rates, sensor data to ground station | `tasks/telemetry_task.c` (DLA-migrated PR-9, FM guard, energy flags) | `test_telemetry` (T-TLM-01..06), Phase 3 integration test | ✅ PR-9 |
| **FR-8** | Health Monitoring | Monitor bus voltage, temperature, task health | `tasks/health_monitor_task.c` (tick-wired PR-10), `services/eps/eps_monitor.c`, `services/fault/fault_manager.c` | `test_health_monitor_task` (T-HM-01..03), `test_eps_monitor` | ✅ PR-10 |
| **FR-9** | Flight Mode Management | Safe FSM: BOOT→SAFE→NOMINAL→DETUMBLE→DIAGNOSTIC; SAFE from any state | `services/fmm/flight_mode_manager.c`, `include/flight_mode.h` | `test_fmm` (T-FMM-01..11) | ✅ 11/11 PR-3 |
| **FR-10** | Fault Aggregation | Centralised fault table (32 slots), levels INFO/WARNING/CRITICAL, anti-cascade | `services/fault/fault_manager.c`, `include/fault_manager.h`, `include/fault_ids.h` | `test_fault_manager` (T-FMS-02..04) | ✅ 12/12 PR-4 |
| **FR-11** | EPS Monitoring | Battery voltage state machine (NOMINAL/LOW/CRITICAL/EMERGENCY) with hysteresis | `services/eps/eps_monitor.c`, `include/eps.h` | `test_eps_monitor` (T-EPS-03..05) | ✅ 12/12 PR-5 |
| **FR-12** | Event Logging | 320-entry ring buffer, Class-A protection, newest-first read | `services/log/logger.c`, `include/logger.h`, `include/log_event_ids.h` | `test_logger` (T-LOG-01..03) | ✅ 12/12 PR-6 |

---

## Non-Functional Requirements

| ID | Requirement | Description | Module(s) | Test Case(s) | Status |
|----|----|-----------|-----------|------------|--------|
| **NFR-1** | Real-Time Scheduling | Tasks execute within allocated time budgets (20 Hz control, 10 Hz sensors) | `FreeRTOS`, task priorities | Phase 2 timing analysis, task jitter test | 🔄 Phase 2 |
| **NFR-2** | Determinism | Control loop jitter <10 ms for stable attitude hold | `FreeRTOS`, priority inheritance | Phase 2 worst-case timing | 🔄 Phase 2 |
| **NFR-3** | Memory Safety | No stack overflow, no dynamic allocation in flight code | CMake static checks, code review | `test_pid`, `test_dynamics`, `test_actuators` | ✅ Pre-release |
| **NFR-4** | Power Efficiency | Average power <2 W during nominal operation | Pico datasheet, empirical measurement | Phase 3 power profiling | 🔄 Phase 3 |
| **NFR-5** | Scalability | Modular architecture supports future sensors, actuators, control laws | HAL pattern, clear interfaces | Code review, `INTERFACE_SPECIFICATION.md` | ✅ Design |

---

## System Requirements to Implementation Mapping

### Attitude Determination Subsystem
```
SYS-REQ 1: "Determine spacecraft attitude within ±5°"
├── FR-1: Attitude Sensing (IMU readout)
├── FR-2: Attitude Determination (Euler computation)
├── NFR-2: Determinism (consistent sampling)
└── Test Cases:
    ├── test_pid (validates PID integration with attitude error)
    ├── test_dynamics (validates Euler integration numerical accuracy)
    └── Phase 4: Kalman filter validation
```

### Attitude Control Subsystem
```
SYS-REQ 2: "Control spacecraft attitude to nadir-pointing (±10°)"
├── FR-3: Rate Control (PID loops)
├── FR-4: Attitude Control (error-to-torque mapping)
├── FR-5: RW Actuation (apply torque)
├── NFR-1: Real-Time Scheduling (control loop timing)
└── Test Cases:
    ├── test_pid (control law validation)
    ├── test_actuators (torque application)
    └── Phase 2: End-to-end control loop test
```

### Telemetry & Health Subsystem
```
SYS-REQ 3: "Transmit vehicle state and health to ground station"
├── FR-7: Telemetry TX (data formatting + transmission)
├── FR-8: Health Monitoring (bus, thermal, RW status)
├── NFR-4: Power Efficiency (low-power telemetry rate)
└── Test Cases:
    ├── Phase 3: WiFi/UART integration test
    └── Phase 3: Power budget validation
```

### Data Layer Subsystem (PR-2)
```
SYS-REQ 4: "All subsystems share vehicle state via a thread-safe snapshot API"
├── FR-1: Attitude Sensing (DLA write path)
├── FR-4: Attitude Control (DLA read path)
└── Test Cases:
    ├── test_sensor_read_task (T-SDM-01..03 partial)
    └── test_attitude_control_task (T-SDM-04..05 partial)
```

### Safety & Fault Subsystem (PRs 3–5)
```
SYS-REQ 5: "OBC shall enter SAFE mode within 100 ms of detecting a CRITICAL fault"
├── FR-9: Flight Mode Management
├── FR-10: Fault Aggregation (CRITICAL → fmm_force_safe)
├── FR-11: EPS Monitoring (EMERGENCY → fmm_force_safe)
└── Test Cases:
    ├── test_fmm (T-FMM-01..11)
    ├── test_fault_manager (T-FMS-02..04)
    └── test_eps_monitor (T-EPS-03..05)
```

### Logging Subsystem (PR-6)
```
SYS-REQ 6: "All CRITICAL events shall be persisted and readable via ground command"
├── FR-12: Event Logging
└── Test Cases:
    └── test_logger (T-LOG-01..03)
```

---

## Unit Test Coverage

### Test: `test_pid`
- **Scope**: PID controller mathematical correctness
- **Requirements Covered**: FR-3 (Rate Control)
- **Test Cases**:
  - ✅ Anti-windup: Integral term saturates correctly
  - ✅ Output saturation: Command clipped to torque limits
  - ✅ Zero-error steady-state: Small setpoint → near-zero error
  - ✅ Step response: Error reduction over iterations
- **Result**: **PASS** (100%)

### Test: `test_dynamics`
- **Scope**: Attitude dynamics (Euler integration) accuracy
- **Requirements Covered**: FR-2 (Attitude Determination), FR-4 (Attitude Control)
- **Test Cases**:
  - ✅ Zero-torque: Attitude remains constant
  - ✅ Constant-torque: Angular acceleration applied correctly
  - ✅ Numerical stability: No NaN/inf outputs over 100 iterations
  - ✅ Energy conservation: Kinetic energy bounds reasonable
- **Result**: **PASS** (100%)

### Test: `test_actuators`
- **Scope**: Actuator command→output models
- **Requirements Covered**: FR-5 (RW Actuation), FR-6 (Magnetorquer Actuation)
- **Test Cases**:
  - ✅ RW torque scaling: Command → momentum change linear
  - ✅ RW saturation: Max torque enforced
  - ✅ Magnetorquer dipole: Command → magnetic moment correct
  - ✅ Momentum limits: RW accumulation capped
- **Result**: **PASS** (100%)

---

## Unit Test Coverage — Spec-Alignment (PRs 3–8)

### Test: `test_fmm` (PR-3)
- **Scope**: Flight Mode FSM transitions and guards
- **Requirements Covered**: FR-9 (Flight Mode Management), SYS-REQ-5
- **Test IDs**: T-FMM-01..11
- **Result**: **PASS** 11/11

### Test: `test_fault_manager` (PR-4)
- **Scope**: Fault table management, FSM levels, CRITICAL→SAFE trigger, anti-cascade
- **Requirements Covered**: FR-10 (Fault Aggregation), SYS-REQ-5
- **Test IDs**: T-FMS-02..04
- **Result**: **PASS** 12/12

### Test: `test_eps_monitor` (PR-5)
- **Scope**: Battery voltage state machine with Schmidt-trigger hysteresis
- **Requirements Covered**: FR-11 (EPS Monitoring), SYS-REQ-5
- **Test IDs**: T-EPS-03..05
- **Result**: **PASS** 12/12

### Test: `test_logger` (PR-6)
- **Scope**: Ring buffer, Class-A eviction protection, ordered log retrieval
- **Requirements Covered**: FR-12 (Event Logging), SYS-REQ-6
- **Test IDs**: T-LOG-01..03
- **Result**: **PASS** 12/12

### Test: `test_sensor_read_task` (PR-7)
- **Scope**: DLA write path, gyro deg/s→rad/s conversion, HAL isolation
- **Requirements Covered**: FR-1 (Attitude Sensing), SYS-REQ-4
- **Test IDs**: T-SDM-01..03 (partial coverage)
- **Result**: **PASS** 7/7

### Test: `test_attitude_control_task` (PR-8)
- **Scope**: DLA read path, FM guard (NOMINAL/DIAGNOSTIC only), imu_valid guard
- **Requirements Covered**: FR-4 (Attitude Control), SYS-REQ-4
- **Test IDs**: T-SDM-04..05 (partial coverage)
- **Result**: **PASS** 8/8

### Test: `test_telemetry` (PR-9)
- **Scope**: DLA read path, FM guard (FM_SAFE → HK-only), energy state encoding in flags
- **Requirements Covered**: FR-7 (Telemetry TX), SYS-REQ-3
- **Test IDs**: T-TLM-01..06
- **Result**: **PASS** 6/6

### Test: `test_health_monitor_task` (PR-10)
- **Scope**: Tick wiring — `fault_manager_tick()` and `eps_monitor_tick()` called on every Step
- **Requirements Covered**: FR-8 (Health Monitoring), FR-10 (Fault Aggregation), SYS-REQ-5
- **Test IDs**: T-HM-01..03
- **Result**: **PASS** 3/3

---

## Integration Test Planning

### Phase 2: Pico SDK + Sensor Integration
```
ITest-1: I2C Write/Read (MPU6050)
  └─ Validates: FR-1 (Attitude Sensing), hardware drivers
  └─ Expected: IMU data readable, values in expected range

ITest-2: Control Loop Execution
  └─ Validates: FR-1 → FR-3 → FR-5 (sensor → control → actuator)
  └─ Expected: Attitude error reduces over time in simulation
  └─ Acceptance: Attitude stabilization within 30 seconds

ITest-3: FreeRTOS Task Scheduling
  └─ Validates: NFR-1, NFR-2 (timing, determinism)
  └─ Expected: All tasks meet deadline; jitter <10 ms
  └─ Acceptance: No missed deadlines in 1 hour operation
```

### Phase 3: Telemetry + Communication
```
ITest-4: WiFi TX Packet Format
  └─ Validates: FR-7 (Telemetry TX)
  └─ Expected: Packets received by ground station, data valid
  └─ Acceptance: 100% packet success rate at 1 Hz

ITest-5: Health Monitor Watchdog
  └─ Validates: FR-8 (Health Monitoring), NFR-5 (reliability)
  └─ Expected: Watchdog triggers graceful shutdown on task stall
  └─ Acceptance: Safe mode entered within 5 seconds
```

### Phase 4: Advanced Control + Kalman
```
ITest-6: Kalman Filter Attitude Estimation
  └─ Validates: FR-2 (Attitude Determination, enhanced)
  └─ Expected: Estimated attitude matches true attitude ±3°
  └─ Acceptance: RMS error <5° over 10 min simulation

T-DYN-01..05: RK2 dynamics integrator (PR-11)
  └─ Validates: FR-3 (Attitude Dynamics, RK2 accuracy)
  └─ Status: ✅ All 5 passing

T-EKF-01..06: Extended Kalman Filter estimator (PR-12)
  └─ Validates: FR-2 (Attitude Determination, EKF-enhanced)
  └─ Status: ✅ All 6 passing

T-LQR-01..07: LQR full-state controller (PR-13)
  └─ Validates: FR-4 (Attitude Control, LQR gains)
  └─ Status: ✅ All 7 passing

T-SRF-08..10: Sensor Read Task — EKF integration (PR-14)
  └─ Validates: FR-2 (sensor fusion pipeline), FR-3 (EKF outputs in DLA)
  └─ Status: ✅ All 3 passing

T-ACT-09..11: Attitude Control Task — LQR/PID dispatch (PR-15)
  └─ Validates: FR-4 (LQR in FM_NOMINAL, PID fallback)
  └─ Status: ✅ All 3 passing

T-WDT-01..05: Hardware Watchdog HAL (PR-16)
  └─ Validates: SYS-REQ-4 (fault-tolerant safe-mode, watchdog supervision)
  └─ Status: ✅ All 5 passing

T-MDT-01..05: Momentum Dump algorithm (PR-17)
  └─ Validates: FR-5 (momentum management, detumble mode)
  └─ Status: ✅ All 5 passing

T-MAG-01..04: HMC5883L Magnetometer driver (PR-18)
  └─ Validates: FR-2 (sensor suite, magnetometer data in DLA)
  └─ Status: ✅ All 4 passing

T-EKFM-01..06: EKF yaw update via magnetometer tilt compensation (PR-19)
  └─ Validates: FR-2 (full 3-axis attitude determination including yaw), FR-3 (EKF observability)
  └─ Status: ✅ All 6 passing

T-EKFM-07: Configurable magnetic declination offset applied in ekf_update_mag() (PR-22)
  └─ Validates: FR-2 (yaw referenced to true north via OBC_MAG_DECLINATION_RAD)
  └─ Status: ✅ Passing

T-QAT-01..05: Unit-quaternion library — multiply, rotate, normalize, slerp, to-euler (PR-21)
  └─ Validates: FR-4 (Attitude Control — quaternion math correctness)
  └─ Status: ✅ All 5 passing

T-LQRS-01..03: LQR gain scheduling — table lookup by energy state + angular momentum (PR-23)
  └─ Validates: FR-4 (Attitude Control — gain scheduling), SYS-REQ-4 (adaptive gains)
  └─ Status: ✅ All 3 passing

T-CLS-01..06: Closed-loop simulation — EKF → LQR → RK2-dynamics stability harness (PR-24)
  └─ Validates: FR-2 (EKF), FR-4 (LQR), FR-3 (RK2 dynamics), SYS-REQ-1 (attitude stabilisation)
  └─ Criterion: settling within 30 s, residual ω < 0.05 rad/s
  └─ Status: ✅ All 6 passing

T-FMS-01a..d: Fault-to-safe integration — CRITICAL fault → FM_SAFE within 100 ms ticks (PR-25)
  └─ Validates: FR-10 (Fault Aggregation → fmm_force_safe), SYS-REQ-5 (safe-mode latency)
  └─ Status: ✅ All 4 sub-tests passing

T-SAFE-01a..c: Watchdog miss → safe-mode integration — watchdog_hal_triggered() path (PR-25)
  └─ Validates: FR-8 (Health Monitoring), FR-10 (Fault Aggregation), SYS-REQ-5
  └─ Status: ✅ All 3 sub-tests passing

T-LOG-01a..d: Flash-backend event logger flush — ring-buffer overflow triggers flash write (PR-26)
  └─ Validates: FR-12 (Persistent Event Logging via flash_backend_flush()), SYS-REQ-6
  └─ Status: ✅ All 4 sub-tests passing
```

---

## Verification Plan

| Phase | Verification Method | Requirements | Target |
|-------|-------------------|--------------|--------|
| **Phase 1** (Complete) | Unit Testing (C code) | FR-3, FR-5, FR-6 (control & actuators) | ✅ 100% tests passing |
| **Phase 2** | Integration Testing (Pico SDK) | FR-1, FR-2, FR-7, NFR-1, NFR-2 | Deadline met in simulation |
| **Phase 3** | Communication Testing (WiFi/UART) | FR-7, FR-8, NFR-4 | Packets received, power <2W |
| **Spec-Alignment PRs 1–8** | Unit Testing (host build) | FR-9..12, SYS-REQ-4..6, FR-1/FR-4 DLA path | ✅ 16/16 passing |
| **Spec-Alignment PRs 9–10** | Unit Testing (host build) | FR-7 DLA migration (T-TLM-01..06), FR-8 tick wiring (T-HM-01..03) | ✅ 17/17 passing |
| **Phase 4 PRs 11–15** | Unit Testing (host build) | FR-2 (EKF), FR-3 (RK2/dynamics), FR-4 (LQR dispatch) | ✅ 19/19 passing |
| **Phase 5 PRs 16–20** | Unit Testing (host build) | FR-2 (yaw/mag), FR-5 (momentum dump), FR-8 (watchdog HAL) | ✅ 23/23 passing |
| **Phase 6 PRs 21–27** | Unit + Integration Testing (host build) | FR-2 (quat, declination), FR-4 (LQR schedule, closed-loop), FR-8 (watchdog safe-mode), FR-10 (CRITICAL→FM_SAFE), FR-12 (flash backend) | ✅ 29/29 passing |
| **MISRA C Audit** | Static analysis + deviation log | All source files | ✅ 0 required/mandatory violations; 91.8% line coverage |
| **Phase 4** | Sensor Fusion Testing (Kalman) | FR-2, FR-3, FR-4 (enhanced) | Attitude error <5° RMS |
| **Phase 5** | Flight Hardware Validation | All functional + safety checks | Ready for CubeSat deployment |

---

## Traceability Gaps & Risks

| Gap | Impact | Mitigation | Owner |
|-----|--------|-----------|-------|
| WiFi power budget not measured | NFR-4 unvalidated | Phase 3 power profiling on real hardware | System Engineer |
| T-SDM full coverage requires DLA integration tests | `data_layer_read/write` race condition not exercised | Add integration test after next sprint | SW Team |

---

## Summary

- **Total Requirements**: 17 (12 functional, 5 non-functional)
- **Unit Test Coverage**: 27 unit tests + 2 integration test targets = **29 CTest executables** (29/29 passing)
- **Integration Test Coverage**: 2 done (T-FMS-01, T-SAFE-01), 2 planned (hardware)
- **Code Line Coverage**: 91.8% (src/control/ + src/core/ + src/services/ combined)
- **MISRA C**: 0 required/mandatory violations; advisory deviations documented in `docs/standards/MISRA_DEVIATIONS.md`
- **Overall Readiness**: 96% (Phase 6 host-testable complete; hardware validation pending)
- **Risk Level**: LOW

---

## Hardware Design Requirements (BOM v1.0)

> Traceability from BOM component decisions to firmware requirements and test coverage.
> **PDR Result: PASS** (2026-03-05, `feature/hardware-bom`)

| HW-REQ | Component | BOM Ref | Firmware Requirement | Tests | Status |
|--------|-----------|---------|---------------------|-------|--------|
| HW-01 | OBC: RP2350 / Pico 2W | §2 | FR-1..FR-12 (all tasks run on RP2350) | All 29/29 | ✅ |
| HW-02 | IMU: MPU-6050 (I2C0, 0x68) | §3 | FR-1 (Attitude Sensing), FR-2 (EKF input) | T-SDM-01..03, T-EKF-01..06 | ✅ |
| HW-03 | Magnetometer: HMC5883L (I2C0, 0x1E) — lab; LIS3MDL (0x1C) for flight | §3, §3.1 | FR-2 (EKF yaw update via `ekf_update_mag()`), FR-6 (momentum dump B×L) | T-MAG-01..04, T-EKFM-01..07 | ✅ Lab (HMC5883L); CDR: new LIS3MDL driver |
| HW-04 | GPS: NEO-7M UART0 @ 9600 baud, NMEA 0183 | §4 | FR-13 (GPS positioning, future) — NMEA parser `src/drivers/gps/neo7m.c` pending | — | 🔄 Planned |
| HW-05 | External watchdog: TPS3431, GPIO20, timeout=3 s | §10 #12 | FR-8 (Health Monitoring — `watchdog_hal_feed()` in `HealthMonitorTask`) | T-WDT-01..05, T-SAFE-01a..c | ✅ HAL ready |
| HW-06 | TT&C: E22-400M30S UART1 @ 115200 baud | §6 | FR-7 (Telemetry TX via KISS/CSP), link margin +8.5 dB @ 2300 km | T-TLM-01..06 | ✅ |
| HW-07 | SAW filter 433 MHz (TDK B39431) | §6e | Non-functional: EMI immunity; no firmware driver required | — | 🔄 Planned |
| HW-08 | EPS: LiPo 18650 → MT3608 5V → Pico VSYS | §9 | FR-11 (EPS Monitor, GPIO26 ADC0 Vbatt) | T-EPS-03..05 | ✅ |
| HW-09 | Magnetorquers: DRV8833 on GPIO14/15/16 | §8 | FR-6 (MTQ actuation), B-dot detumbling (Phase 1 ADCS) | T-MDT-01..05 | ✅ HAL stub |
| HW-10 | Reaction wheels: TB6612FNG on GPIO6/7/8 | §8 | FR-5 (RW actuation), LQR torque output (Phase 2 ADCS) | test_actuators | ✅ HAL stub |

### Open Hardware Items (CDR scope)

| Item | Description | Owner |
|------|-------------|-------|
| CDR-HW-01 | Migrate magnetometer driver: `hmc5883l.c` → `lis3mdl.c` (addr 0x1C, new register map) | SW Team |
| CDR-HW-02 | Implement GPS NMEA parser: `src/drivers/gps/neo7m.c` + FreeRTOS task | SW Team |
| CDR-HW-03 | Implement B-dot detumbling controller: `src/control/b_dot_control.c` | Control Team |
| CDR-HW-04 | Add PWM HAL output to `reaction_wheel.c` and `magnetorquer.c` (torque → duty cycle) | HW/SW Team |
| CDR-HW-05 | Qualify all flight components for TID/SEE (polar LEO, ~97° orbit) | Systems |
| CDR-HW-06 | Measure WCET of all 7 FreeRTOS tasks using `DWT->CYCCNT` and document timing budget | SW Team |

---

**Last Updated**: 2026-03-20
**Matrix Version**: 2.3
**Status**: Active (updated each phase)
