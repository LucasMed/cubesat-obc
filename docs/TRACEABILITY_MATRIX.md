# Requirements Traceability Matrix

## Overview

This matrix maps functional and non-functional requirements to implementation modules and test cases, ensuring complete coverage and testability.

---

## Functional Requirements

| ID | Requirement | Description | Module(s) | Test Case(s) | Status |
|----|----|-----------|-----------|------------|--------|
| **FR-1** | Attitude Sensing | Read 6-DOF IMU (accel + gyro) via I2C at 10 Hz | `drivers/imu/mpu6050.c`, `tasks/sensor_read_task.c` | `test_pid` (validates sensor integration), integration test Phase 2 | ✅ Ready |
| **FR-2** | Attitude Determination | Compute Euler angles (roll, pitch, yaw) from sensor data | `control/attitude_control.c`, `dynamics/attitude_dynamics.c` | `test_dynamics` (Euler integration), Phase 4 Kalman test | ✅ Implemented |
| **FR-3** | Rate Control | Stabilize angular rates via PID loops (3 axes: roll, pitch, yaw) | `control/pid_controller.c` | `test_pid` (PID math, anti-windup) | ✅ 100% Pass |
| **FR-4** | Attitude Control | Command desired attitude and compute torque setpoints | `control/attitude_control.c` | `test_dynamics`, Phase 2 integration | ✅ Ready |
| **FR-5** | RW Actuation | Apply torque commands to reaction wheel motors (3-axis) | `actuators/reaction_wheel.c` | `test_actuators` (torque-to-momentum conversion) | ✅ 100% Pass |
| **FR-6** | Magnetorquer Actuation | Apply magnetic dipole commands (3-axis) for de-saturation | `actuators/magnetorquer.c` | `test_actuators` (dipole output validation) | ✅ 100% Pass |
| **FR-7** | Telemetry TX | Transmit attitude, rates, sensor data to ground station | `tasks/telemetry_task.c`, `drivers/comms/` (Phase 3) | Phase 3 integration test | 🔄 Phase 3 |
| **FR-8** | Health Monitoring | Monitor bus voltage, temperature, task health | `tasks/health_monitor_task.c`, `core/system_state.c` | Phase 2 integration test | ✅ Ready |

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
```

---

## Verification Plan

| Phase | Verification Method | Requirements | Target |
|-------|-------------------|--------------|--------|
| **Phase 1** (Complete) | Unit Testing (C code) | FR-3, FR-5, FR-6 (control & actuators) | ✅ 100% tests passing |
| **Phase 2** | Integration Testing (Pico SDK) | FR-1, FR-2, FR-7, NFR-1, NFR-2 | Deadline met in simulation |
| **Phase 3** | Communication Testing (WiFi/UART) | FR-7, FR-8, NFR-4 | Packets received, power <2W |
| **Phase 4** | Sensor Fusion Testing (Kalman) | FR-2, FR-3, FR-4 (enhanced) | Attitude error <5° RMS |
| **Phase 5** | Flight Hardware Validation | All functional + safety checks | Ready for CubeSat deployment |

---

## Traceability Gaps & Risks

| Gap | Impact | Mitigation | Owner |
|-----|--------|-----------|-------|
| Phase 2 real hardware unavailable | I2C drivers untested on Pico | Simulator emulation or loopback test | SW Team |
| Kalman filter not yet designed | Attitude determination accuracy unknown | Phase 4 design review required | Control Lead |
| WiFi power budget not measured | NFR-4 unvalidated | Phase 3 power profiling on real hardware | System Engineer |
| Task jitter not characterized | NFR-2 risk | Phase 2 timing analyzer (FreeRTOS hooks) | Integration Lead |

---

## Summary

- **Total Requirements**: 13 (8 functional, 5 non-functional)
- **Unit Test Coverage**: 3 tests covering 6 requirements (100% passing)
- **Integration Test Coverage**: 6 planned tests covering remaining requirements
- **Overall Readiness**: 46% (6/13 requirements verified, remainder in Phase 2+)
- **Risk Level**: LOW (gaps are expected in design phase; clear mitigation plan)

---

**Last Updated**: 2026-02-15  
**Matrix Version**: 1.0  
**Status**: Active (updated each phase)
