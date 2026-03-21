# Integration Test Plan

**Document ID**: ITP-OBC-001  
**Version**: 1.0  
**Date**: 2026-03-21  
**Status**: Released  
**Author**: OBC Systems Team  
**Project**: CubeSat OBC Flight Software (RP2350 / Pico 2W)  
**Standard**: ECSS-E-ST-40C, ECSS-E-ST-10-04C  

---

## Change History

| Version | Date       | Author           | Description                                          |
|---------|------------|------------------|------------------------------------------------------|
| 1.0     | 2026-03-21 | OBC Systems Team | Initial release following Phase 7 completion |

---

## Table of Contents

1. [Introduction](#1-introduction)
2. [Integration Strategy](#2-integration-strategy)
3. [Integration Sequence](#3-integration-sequence)
4. [Test Environment](#4-test-environment)
5. [Level 1 Integration Tests: Driver Integration](#5-level-1-integration-tests-driver-integration)
6. [Level 2 Integration Tests: Control Algorithms](#6-level-2-integration-tests-control-algorithms)
7. [Level 3 Integration Tests: Services Integration](#7-level-3-integration-tests-services-integration)
8. [Level 4 Integration Tests: Full System Integration](#8-level-4-integration-tests-full-system-integration)
9. [Success Criteria](#9-success-criteria)
10. [Risk Assessment](#10-risk-assessment)
11. [Traceability Matrix](#11-traceability-matrix)

---

## 1. Introduction

### 1.1 Purpose

This Integration Test Plan (ITP) defines the strategy, approach, and procedures for verifying the correct integration of the CubeSat OBC flight software subsystems. The document establishes a structured bottom-up integration sequence progressing from individual drivers through control algorithms, services, and finally full system integration.

The purpose of this document is to:
- Define the integration levels and their pass/fail criteria
- Specify the test cases required to verify inter-module interfaces
- Provide a roadmap for progressive integration from unit-tested modules to a complete flight software system
- Ensure that all module interactions and data flows are correctly implemented and verified

### 1.2 Scope

This Integration Test Plan covers the integration of all flight software components from initial driver-level integration through full system integration:

**In Scope:**
- Hardware driver integration (MPU6050 IMU, HMC5883L Magnetometer, NEO-7M GPS)
- Control algorithm integration (EKF, LQR, PID, Momentum Dump, Attitude Dynamics)
- Service layer integration (FMM, Fault Manager, DLA, EPS Monitor, Logger)
- Task-level integration (SensorRead, AttitudeControl, Telemetry, HealthMonitor, Command, GpsTask)
- Communication stack integration (CSP, KISS framing, UART interfaces)
- Data flow verification across all subsystems

**Out of Scope:**
- Unit testing of individual modules (documented in STP-OBC-001)
- Hardware-in-the-Loop (HIL) testing with physical hardware
- Software-in-the-Loop (SIL) end-to-end scenarios
- Ground station validation
- Third-party library validation (Pico SDK, FreeRTOS, libcsp)

### 1.3 Reference Documents

| ID | Title | Version | Relationship |
|----|-------|---------|--------------|
| SRS-OBC-001 | Software Requirements Specification | 2.4 | Primary requirements baseline |
| SyRS-OBC-001 | System Requirements Specification | 1.1 | System-level requirements |
| SVVP-OBC-001 | Software Verification and Validation Plan | 1.0 | Verification strategy |
| STP-OBC-001 | Software Test Procedure | 3.0 | Unit and integration test procedures |
| STR-OBC-001 | Software Test Report | 1.0 | Test execution results |
| RTM-OBC-001 | Requirements Traceability Matrix | — | Requirement-to-test mapping |
| ADCS-DES-001 | ADCS Design Document | 1.0 | Attitude control design |
| FMM-DES-001 | Flight Mode Manager Design | 1.0 | Flight mode FSM specification |
| FAULT-DES-001 | Fault Management Design | 1.0 | Fault detection and response |
| OBC-DES-001 | OBC Design Document | 1.0 | System architecture |
| ECSS-E-ST-40C | Space Engineering — Software | — | Software engineering standard |
| ECSS-E-ST-10-04C | Space Engineering — Verification | — | Verification documentation standard |

### 1.4 Definitions and Acronyms

| Acronym | Definition |
|---------|------------|
| ITP | Integration Test Plan |
| OBC | On-Board Computer |
| ADCS | Attitude Determination and Control System |
| EKF | Extended Kalman Filter |
| LQR | Linear Quadratic Regulator |
| PID | Proportional-Integral-Derivative |
| FMM | Flight Mode Manager |
| DLA | Data Layer Abstraction |
| EPS | Electrical Power System |
| CSP | CubeSat Space Protocol |
| KISS | Keep It Simple Stupid (framing protocol) |
| UART | Universal Asynchronous Receiver-Transmitter |
| I2C | Inter-Integrated Circuit |
| SPI | Serial Peripheral Interface |
| IMU | Inertial Measurement Unit |
| RW | Reaction Wheel |
| HIL | Hardware-in-the-Loop |
| SIL | Software-in-the-Loop |

---

## 2. Integration Strategy

### 2.1 Bottom-Up Integration Approach

The integration follows a bottom-up strategy, progressively building from verified unit-tested components to a fully integrated system:

```
Level 1: Driver Integration (MPU6050, HMC5883L, NEO-7M)
    ↓
Level 2: Control Algorithm Integration (EKF, LQR, PID, Dynamics)
    ↓
Level 3: Services Integration (FMM, Fault Manager, DLA, Logger)
    ↓
Level 4: Full System Integration (All Tasks, CSP, Communications)
```

### 2.2 Integration Principles

1. **Incremental Verification**: Each level must pass all integration tests before proceeding to the next level
2. **Interface Contracts**: All inter-module interfaces must be verified with concrete test cases
3. **Data Flow Validation**: End-to-end data paths must be traced from sensor input through processing to actuator output
4. **Failure Mode Testing**: Each integration boundary must be tested for proper fault propagation and handling
5. **Deterministic Results**: Integration tests must produce consistent, repeatable results on host build

### 2.3 Integration Criteria

| Criterion | Description |
|-----------|-------------|
| **Interface Verification** | All function calls between modules execute without errors |
| **Data Consistency** | Data passed between modules maintains correct values and units |
| **Error Propagation** | Errors at integration boundaries are correctly detected and handled |
| **Timing Behavior** | Task interactions maintain correct temporal relationships |
| **Resource Usage** | No memory leaks or resource contention at integration boundaries |

---

## 3. Integration Sequence

### 3.1 Level 1: Driver Integration

**Objective**: Verify correct operation of hardware abstraction layer and sensor data acquisition.

**Modules Integrated**:
- MPU6050 IMU Driver (`src/drivers/imu/mpu6050.c`)
- HMC5883L Magnetometer Driver (`src/drivers/mag/hmc5883l.c`)
- NEO-7M GPS Driver (`src/drivers/gps/neo7m.c`)

**Integration Points**:
- Driver → Data Layer (sensor data storage)
- Driver → I2C HAL (hardware abstraction)
- Driver → UART HAL (GPS NMEA input)

**Entry Criteria**:
- All unit tests for each driver pass
- Host HAL stubs available for I2C/UART operations
- Driver initialization functions verified

**Exit Criteria**:
- Sensor data successfully flows to Data Layer
- Raw sensor readings are correctly scaled and converted
- Sensor validity flags are properly managed

### 3.2 Level 2: Control Algorithm Integration

**Objective**: Verify correct integration of attitude determination and control algorithms.

**Modules Integrated**:
- Extended Kalman Filter (`src/control/ekf.c`)
- LQR Controller (`src/control/lqr.c`)
- LQR Gain Scheduler (`src/control/lqr_schedule.c`)
- PID Controller (`src/control/pid_controller.c`)
- Attitude Dynamics (`src/dynamics/attitude_dynamics.c`)
- Momentum Dump (`src/services/adcs/momentum_dump.c`)

**Integration Points**:
- EKF → DLA (attitude state output)
- Sensor Drivers → EKF (sensor measurements input)
- EKF → LQR (estimated state input)
- LQR → Attitude Dynamics (torque command input)
- Attitude Dynamics → Actuators (actuator commands output)
- FMM → LQR Gain Scheduler (flight mode input)

**Entry Criteria**:
- All Level 1 integration tests pass
- Sensor data flows correctly to EKF
- Control algorithms receive valid state estimates

**Exit Criteria**:
- Attitude control loop executes without errors
- State estimates converge correctly
- Actuator commands are within physical limits
- Gain scheduling responds to flight mode changes

### 3.3 Level 3: Services Integration

**Objective**: Verify correct integration of flight software services and fault management.

**Modules Integrated**:
- Flight Mode Manager (`src/services/fmm/flight_mode_manager.c`)
- Fault Manager (`src/services/fault/fault_manager.c`)
- Data Layer Abstraction (`src/core/data_layer.c`)
- EPS Monitor (`src/services/eps/eps_monitor.c`)
- Event Logger (`src/services/log/logger.c`)

**Integration Points**:
- Fault Manager → FMM (fault-driven mode transitions)
- FMM → DLA (flight mode storage and retrieval)
- EPS Monitor → Fault Manager (power fault reporting)
- Health Monitor Task → Fault Manager (health fault reporting)
- All Tasks → DLA (data access)
- Logger → DLA (event logging)

**Entry Criteria**:
- All Level 2 integration tests pass
- Attitude control algorithms verified
- DLA data structures verified

**Exit Criteria**:
- Faults correctly trigger mode transitions
- Flight mode changes are properly managed
- All services access DLA consistently
- Fault recovery sequences execute correctly

### 3.4 Level 4: Full System Integration

**Objective**: Verify complete flight software system integration with all tasks and communications.

**Modules Integrated**:
- All FreeRTOS Tasks (SensorRead, AttitudeControl, Telemetry, HealthMonitor, Command, GpsTask)
- CSP Communication Stack (`third_party/libcsp/`)
- KISS Framing (`src/core/kiss.c`)
- UART Interfaces (`src/drivers/uart/pico_usart.c`)

**Integration Points**:
- All Tasks → DLA (shared state access)
- Telemetry Task → CSP (telemetry packet transmission)
- Command Task ← CSP (command reception)
- GPS Task → DLA (GPS data integration)
- All Tasks → Health Monitor (task health reporting)

**Entry Criteria**:
- All Level 3 integration tests pass
- Services layer verified
- Communication stubs available

**Exit Criteria**:
- All tasks communicate via DLA without conflicts
- Telemetry packets are correctly formatted and transmitted
- Commands are correctly received and processed
- CSP connections establish and maintain properly

---

## 4. Test Environment

### 4.1 Host Build Configuration

| Component | Specification | Purpose |
|-----------|---------------|---------|
| **Build System** | CMake | ≥ 3.13 |
| **Host Compiler** | GCC / Clang | C11 compliant |
| **Test Framework** | Unity | v2.x |
| **Protocol Stack** | libcsp | v2.2 (host build) |
| **Coverage Tools** | gcov / gcovr | Coverage analysis |
| **Platform** | macOS / Linux (Ubuntu 22.04) | Host execution |

### 4.2 Test Infrastructure

| Item | Location | Purpose |
|------|----------|---------|
| **Integration Tests** | `tests/integration/*.c` | Multi-module verification |
| **Host HAL Stubs** | `include/host/` | Hardware abstraction for host |
| **Unity Framework** | `third_party/Unity/` | Test assertions and reporting |
| **FreeRTOS Stubs** | `include/FreeRTOS.h` | RTOS task abstraction on host |
| **CSP Host Library** | `third_party/libcsp/` | Protocol stack on host |

### 4.3 Build Commands

```bash
# Configure host build
mkdir -p build && cd build
cmake -DPICO_ENABLED=OFF ..

# Build integration tests
cmake --build . --target test_integration

# Run integration tests
ctest -R integration --output-on-failure --verbose
```

### 4.4 Target Platform Configuration

| Component | Specification | Notes |
|-----------|---------------|-------|
| **Hardware** | Raspberry Pi Pico 2W (RP2350) | Dual-core Cortex-M33 |
| **Clock Speed** | 100 MHz minimum | Runtime target |
| **RAM** | 520 KB SRAM | Runtime memory |
| **Flash** | 2 MB | Firmware storage |
| **IMU** | MPU6050 (I2C0, 400 kHz) | GPIO4/5 |
| **Magnetometer** | HMC5883L (I2C0, 0x1E) | GPIO4/5 |
| **GPS** | NEO-7M (UART1, 9600 baud) | GPIO4/5 |
| **TT&C** | UART1 (KISS/CSP, 115200) | GPIO8/9 |

---

## 5. Level 1 Integration Tests: Driver Integration

### 5.1 IMU Driver Integration Tests

**Test File**: `tests/integration/test_i2c_mpu6050.c`  
**Requirements Verified**: FR-1, FR-2, SYS-F-100

| Test ID | Test Description | Test Procedure | Pass Criterion |
|---------|-----------------|---------------|---------------|
| INT-L1-IMU-01 | MPU6050 → DLA data flow | Initialize driver; provide simulated I2C data; call sensor read; verify DLA update | DLA `imu_valid` = true; accel and gyro values match expected |
| INT-L1-IMU-02 | I2C error handling | Simulate I2C timeout; verify graceful degradation | `imu_valid` = false; no crash; error logged |
| INT-L1-IMU-03 | Accelerometer scaling verification | Provide known raw values; verify scaling to m/s² | Values match expected scaling (±1 LSB tolerance) |
| INT-L1-IMU-04 | Gyroscope scaling verification | Provide known raw values; verify scaling to rad/s | Values match expected scaling (±1 LSB tolerance) |
| INT-L1-IMU-05 | Driver initialization sequence | Call init function; verify chip ID read | Returns expected chip ID (0x68); configuration registers set |

### 5.2 Magnetometer Driver Integration Tests

**Test File**: `tests/integration/test_mag_integration.c`  
**Requirements Verified**: FR-11, SYS-F-105, SYS-F-107

| Test ID | Test Description | Test Procedure | Pass Criterion |
|---------|-----------------|---------------|---------------|
| INT-L1-MAG-01 | HMC5883L → DLA data flow | Initialize driver; provide simulated I2C data; call mag read; verify DLA update | DLA `mag_valid` = true; field values in expected range |
| INT-L1-MAG-02 | Microtesla conversion | Provide known raw values; verify conversion to µT | Output matches expected microtesla value (±1 LSB) |
| INT-L1-MAG-03 | I2C address verification | Verify driver addresses correct I2C device | Correct slave address (0x1E) used |
| INT-L1-MAG-04 | Mag data written to DLA during sensor task | Execute sensor read flow; verify DLA contents | DLA `mag_valid` = true; field vector populated |
| INT-L1-MAG-05 | Sensor unavailability handled | Disable mag; verify graceful handling | `mag_valid` = false; no crash |

### 5.3 GPS Driver Integration Tests

**Test File**: `tests/integration/test_gps_integration.c`  
**Requirements Verified**: FR-18, FR-19, SYS-F-461, SYS-F-462, SYS-F-463

| Test ID | Test Description | Test Procedure | Pass Criterion |
|---------|-----------------|---------------|---------------|
| INT-L1-GPS-01 | NMEA $GPGGA sentence parsing | Provide valid $GPGGA; call parser; verify DLA update | DLA contains valid latitude, longitude, altitude |
| INT-L1-GPS-02 | NMEA $GPRMC sentence parsing | Provide valid $GPRMC; call parser; verify DLA update | DLA contains valid speed, course, time |
| INT-L1-GPS-03 | UART → DLA data flow | Simulate UART input; execute GPS task; verify DLA | GPS data correctly parsed and stored |
| INT-L1-GPS-04 | Invalid NMEA handling | Provide malformed sentence; verify graceful handling | Parser returns error; no crash; system continues |
| INT-L1-GPS-05 | Checksum verification | Provide valid and invalid checksums; verify detection | Valid checksum accepted; invalid rejected |
| INT-L1-GPS-06 | GPS data in telemetry packet | Execute telemetry task with GPS data; verify packet | Telemetry packet contains GPS fields |

---

## 6. Level 2 Integration Tests: Control Algorithms

### 6.1 EKF Integration Tests

**Test File**: `tests/integration/test_ekf_integration.c`  
**Requirements Verified**: SYS-F-101, SYS-F-102, SYS-F-103, SYS-F-104, SYS-F-105

| Test ID | Test Description | Test Procedure | Pass Criterion |
|---------|-----------------|---------------|---------------|
| INT-L2-EKF-01 | EKF → DLA state output | Run EKF update cycle; verify DLA contents | DLA quaternion, biases, covariance updated |
| INT-L2-EKF-02 | IMU → EKF prediction flow | Provide gyro reading; run predict; verify state | Attitude changes by expected amount |
| INT-L2-EKF-03 | Accel → EKF roll/pitch update | Provide accel reading; run update; verify P reduction | P[0][0], P[1][1] decrease |
| INT-L2-EKF-04 | Mag → EKF yaw update | Provide mag reading; run tilt-compensated update; verify yaw correction | EKF yaw estimate corrected |
| INT-L2-EKF-05 | EKF → LQR state feed | Run EKF; verify LQR receives valid state | LQR computes non-zero torque |
| INT-L2-EKF-06 | GPS → EKF time sync (if available) | Provide GPS time; verify EKF timestamp update | EKF state timestamp synchronized |

### 6.2 Control Loop Integration Tests

**Test File**: `tests/integration/test_control_loop.c`  
**Requirements Verified**: FR-3, FR-4, SYS-F-110, SYS-F-111, SYS-F-112, SYS-F-113

| Test ID | Test Description | Test Procedure | Pass Criterion |
|---------|-----------------|---------------|---------------|
| INT-L2-CTRL-01 | Attitude control task execution | Set FM_NOMINAL; run attitude control task; verify torque computed | Non-zero torque produced for non-zero error |
| INT-L2-CTRL-02 | LQR dispatch in NOMINAL mode | FM_NOMINAL + EKF valid; run task; verify LQR called | `lqr_compute()` executed |
| INT-L2-CTRL-03 | PID fallback without EKF | FM_NOMINAL + EKF invalid; run task; verify PID called | `attitude_ctrl_update()` (PID) executed |
| INT-L2-CTRL-04 | PID in DIAGNOSTIC mode | FM_DIAGNOSTIC; run task; verify PID used | LQR bypassed; PID active |
| INT-L2-CTRL-05 | LQR gain scheduling by flight mode | Test FM_NOMINAL, FM_DETUMBLE; verify correct gains selected | Gains correspond to scheduled values |
| INT-L2-CTRL-06 | Torque saturation | Command large attitude error; verify torque clamped | |torque| ≤ CLS_TAU_SAT |
| INT-L2-CTRL-07 | Attitude dynamics step | Apply torque; run dynamics; verify quaternion updated | Quaternion changes proportionally to torque |

### 6.3 Momentum Dump Integration Tests

**Test File**: `tests/integration/test_momentum_dump.c`  
**Requirements Verified**: FR-6, SYS-F-114, SYS-F-116b

| Test ID | Test Description | Test Procedure | Pass Criterion |
|---------|-----------------|---------------|---------------|
| INT-L2-MD-01 | Momentum threshold detection | Set momentum above threshold; verify dump triggered | `momentum_dump_needed()` returns true |
| INT-L2-MD-02 | FM guard in DETUMBLE mode | FM_DETUMBLE; run momentum dump; verify actuator output | Magnetorquer commands generated |
| INT-L2-MD-03 | FM guard prevents dump in NOMINAL | FM_NOMINAL; run momentum dump; verify no output | No actuator output |
| INT-L2-MD-04 | B-dot duty cycle computation | Provide magnetic field reading; verify duty cycle | Duty cycle proportional to dB/dt |
| INT-L2-MD-05 | Magnetorquer saturation | Request dipole above maximum; verify clamping | Output clamped to max value |

---

## 7. Level 3 Integration Tests: Services Integration

### 7.1 Flight Mode Manager Integration Tests

**Test File**: `tests/integration/test_fmm_integration.c`  
**Requirements Verified**: FR-9, SR-2, SYS-F-115, SYS-F-204, SYS-F-205

| Test ID | Test Description | Test Procedure | Pass Criterion |
|---------|-----------------|---------------|---------------|
| INT-L3-FMM-01 | FMM → DLA mode storage | Call mode transition; verify DLA mode updated | DLA `flight_mode` reflects new mode |
| INT-L3-FMM-02 | Valid mode transitions | Test BOOT→SAFE, SAFE→NOMINAL, etc.; verify allowed transitions | Only valid transitions succeed |
| INT-L3-FMM-03 | Invalid mode transitions blocked | Test BOOT→NOMINAL (invalid); verify rejection | Mode unchanged; error returned |
| INT-L3-FMM-04 | fmm_force_safe() from any state | From NOMINAL/DIAGNOSTIC; call force_safe; verify SAFE entered | Mode = FM_SAFE regardless of prior state |
| INT-L3-FMM-05 | DETUMBLE → NOMINAL auto-transition | In DETUMBLE; inject low rates; verify auto-transition | Mode changes to NOMINAL when rates low |
| INT-L3-FMM-06 | Mode transition logging | Execute transition; verify logger entry | Event logged with correct mode info |

### 7.2 Fault Manager Integration Tests

**Test File**: `tests/integration/test_fault_safe.c`  
**Requirements Verified**: SR-1, SR-2, SYS-F-211, SYS-F-212, SYS-F-213

| Test ID | Test Description | Test Procedure | Pass Criterion |
|---------|-----------------|---------------|---------------|
| INT-L3-FM-01 | CRITICAL fault → FM_SAFE | Report CRITICAL fault; verify FMM → FM_SAFE | Mode transitions to SAFE within 1 tick |
| INT-L3-FM-02 | ERROR fault does NOT trigger SAFE | Report ERROR fault; verify mode unchanged | Mode remains; ERROR logged |
| INT-L3-FM-03 | WARNING fault does NOT trigger SAFE | Report WARNING fault; verify mode unchanged | Mode remains; WARNING logged |
| INT-L3-FM-04 | FM_SAFE is sticky after fault clear | CRITICAL → FM_SAFE; clear fault; verify SAFE persists | Mode remains SAFE; requires explicit recovery |
| INT-L3-FM-05 | Multiple faults tracked correctly | Report multiple faults; verify all stored and highest level correct | All faults active; highest level correct |
| INT-L3-FM-06 | Fault event logging | Report fault; verify logger entry | Event logged with fault ID and level |
| INT-L3-FM-07 | WARNING auto-clear after timeout | Report WARNING; advance ticks; verify auto-clear | Fault cleared at configured timeout |

### 7.3 Watchdog Integration Tests

**Test File**: `tests/integration/test_safe_trigger.c`  
**Requirements Verified**: SYS-F-211, SYS-F-212, SYS-F-213

| Test ID | Test Description | Test Procedure | Pass Criterion |
|---------|-----------------|---------------|---------------|
| INT-L3-WDT-01 | Watchdog trigger → FM_SAFE | Simulate watchdog triggered; execute health monitor; verify SAFE | Mode = FM_SAFE; FAULT_WDT_KICK_MISSED active |
| INT-L3-WDT-02 | Clean boot no spurious SAFE | Simulate clean boot (no watchdog); verify mode unchanged | No FM_SAFE transition |
| INT-L3-WDT-03 | Repeated triggers remain SAFE | Multiple watchdog triggers; verify mode remains SAFE | Mode sticky; fault count increments |
| INT-L3-WDT-04 | Health monitor feeds watchdog | Execute health monitor step; verify watchdog fed | `watchdog_hal_feed()` called |

### 7.4 Service Cross-Integration Tests

**Test File**: `tests/integration/test_services_integration.c`  
**Requirements Verified**: FR-9, FR-10a, SYS-F-301, SYS-F-302

| Test ID | Test Description | Test Procedure | Pass Criterion |
|---------|-----------------|---------------|---------------|
| INT-L3-SVC-01 | FMM ↔ Fault Manager interaction | Inject CRITICAL fault; verify FMM response | Mode transition and fault recording consistent |
| INT-L3-SVC-02 | EPS Monitor → Fault Manager | Simulate low battery; verify fault reported | FAULT_EPS_LOW_VOLTAGE reported |
| INT-L3-SVC-03 | All services access DLA consistently | Multiple services read/write DLA; verify consistency | No data corruption; mutex working |
| INT-L3-SVC-04 | Logger integration with FMM/Fault | Execute mode transitions and faults; verify logging | All events logged with correct data |
| INT-L3-SVC-05 | Snapshot consistency | Create DLA snapshot during service activity; verify coherent state | Snapshot represents single instant |

---

## 8. Level 4 Integration Tests: Full System Integration

### 8.1 Task Integration Tests

**Test File**: `tests/integration/test_tasks_integration.c`  
**Requirements Verified**: FR-3, FR-4, FR-7, FR-8, FR-9, FR-10a

| Test ID | Test Description | Test Procedure | Pass Criterion |
|---------|-----------------|---------------|---------------|
| INT-L4-TASK-01 | SensorRead → AttitudeControl data flow | Execute both tasks; verify attitude estimate available | DLA populated by SensorRead; consumed by AttitudeControl |
| INT-L4-TASK-02 | AttitudeControl → Actuators data flow | Run attitude control; verify actuator commands generated | Torque/magnetorquer commands in expected range |
| INT-L4-TASK-03 | Telemetry task captures full state | Execute all tasks; run Telemetry; verify packet contents | Telemetry packet contains all required fields |
| INT-L4-TASK-04 | HealthMonitor task integration | Execute health monitor with other tasks running; verify watchdog management | All tasks healthy; watchdog fed |
| INT-L4-TASK-05 | Command task mode change | Send mode command; verify FMM transition | Mode changes per command |
| INT-L4-TASK-06 | GpsTask data integration | Execute GPS task; verify DLA GPS fields updated | GPS data in DLA; `gps_valid` set correctly |
| INT-L4-TASK-07 | Multi-task DLA access safety | Run multiple tasks accessing DLA; verify no corruption | All data consistent; no race conditions |

### 8.2 Communication Integration Tests

**Test File**: `tests/integration/test_comm_integration.c`  
**Requirements Verified**: FR-7, SYS-F-300 series

| Test ID | Test Description | Test Procedure | Pass Criterion |
|---------|-----------------|---------------|---------------|
| INT-L4-COMM-01 | Telemetry packet construction | Execute Telemetry task; verify CSP packet format | Packet structure matches CSP specification |
| INT-L4-COMM-02 | KISS framing encoding | Provide telemetry data; verify KISS encoded output | Frame delimiters and escaping correct |
| INT-L4-COMM-03 | Command reception and decoding | Send command via simulated uplink; verify processing | Command executed; response generated |
| INT-L4-COMM-04 | CSP connection establishment | Initialize CSP; verify node address and ports | CSP initialized; ports bound correctly |
| INT-L4-COMM-05 | UART → CSP data flow | Simulate UART input; verify CSP packet extraction | NMEA/command data reaches correct handler |
| INT-L4-COMM-06 | CSP → UART data flow | Generate telemetry; verify UART transmission | Data transmitted with correct framing |

### 8.3 Payload Integration Tests

**Test File**: `tests/integration/test_payload_integration.c`  
**Requirements Verified**: FR-15, FR-16, FR-17

| Test ID | Test Description | Test Procedure | Pass Criterion |
|---------|-----------------|---------------|---------------|
| INT-L4-PAY-01 | Payload task execution | Run payload task; verify execution without errors | Task completes; no crashes |
| INT-L4-PAY-02 | Payload data to DLA | Execute payload task; verify data stored | DLA contains valid payload data |
| INT-L4-PAY-03 | Payload telemetry integration | Run telemetry with payload data; verify inclusion | Telemetry packet contains payload fields |

### 8.4 End-to-End Integration Tests

**Test File**: `tests/integration/test_e2e_integration.c`  
**Requirements Verified**: All FR and SYS-F requirements

| Test ID | Test Description | Test Procedure | Pass Criterion |
|---------|-----------------|---------------|---------------|
| INT-L4-E2E-01 | Boot sequence: BOOT → SAFE → NOMINAL | Simulate boot; verify mode progression | Correct mode at each stage |
| INT-L4-E2E-02 | Normal operation data path | Sensor→EKF→LQR→Actuator; verify complete flow | Attitude controlled; actuators commanded |
| INT-L4-E2E-03 | Fault injection → SAFE mode | Inject CRITICAL fault during operation; verify SAFE | Mode transitions to SAFE; operation suspended |
| INT-L4-E2E-04 | Recovery sequence | After SAFE; clear fault; send NOMINAL command; verify recovery | Mode returns to NOMINAL |
| INT-L4-E2E-05 | Extended operation stability | Run system for simulated duration; verify no degradation | Memory stable; timing maintained |

---

## 9. Success Criteria

### 9.1 Level 1 Success Criteria

| Criterion | Requirement | Status |
|-----------|-------------|--------|
| All IMU integration tests pass | 5/5 tests pass | TBD |
| All magnetometer integration tests pass | 5/5 tests pass | TBD |
| All GPS integration tests pass | 6/6 tests pass | TBD |
| Sensor data flows to DLA correctly | All sensors write valid data | TBD |
| I2C/UART error handling verified | Errors handled gracefully | TBD |

### 9.2 Level 2 Success Criteria

| Criterion | Requirement | Status |
|-----------|-------------|--------|
| EKF integration tests pass | 6/6 tests pass | TBD |
| Control loop tests pass | 7/7 tests pass | TBD |
| Momentum dump tests pass | 5/5 tests pass | TBD |
| EKF → LQR data flow verified | State estimates consumed correctly | TBD |
| Control outputs within limits | Torques saturated correctly | TBD |

### 9.3 Level 3 Success Criteria

| Criterion | Requirement | Status |
|-----------|-------------|--------|
| FMM integration tests pass | 6/6 tests pass | TBD |
| Fault manager tests pass | 7/7 tests pass | TBD |
| Watchdog integration tests pass | 4/4 tests pass | TBD |
| Service cross-integration tests pass | 5/5 tests pass | TBD |
| Fault → SAFE transitions correct | CRITICAL triggers SAFE | TBD |
| FM_SAFE sticky behavior verified | Mode persists after fault clear | TBD |

### 9.4 Level 4 Success Criteria

| Criterion | Requirement | Status |
|-----------|-------------|--------|
| Task integration tests pass | 7/7 tests pass | TBD |
| Communication integration tests pass | 6/6 tests pass | TBD |
| Payload integration tests pass | 3/3 tests pass | TBD |
| End-to-end tests pass | 5/5 tests pass | TBD |
| All tasks communicate via DLA | No direct task-to-task coupling | TBD |
| CSP communications functional | Packets sent and received correctly | TBD |

### 9.5 Overall Integration Success Criteria

| Criterion | Requirement | Target |
|-----------|-------------|--------|
| All integration tests passing | 100% pass rate | 100% |
| No memory errors | Zero leaks, zero invalid access | 0 errors |
| No integration deadlocks | All test scenarios complete | 0 hangs |
| Code coverage (integration) | ≥ 80% line coverage on integrated modules | ≥ 80% |
| Build reproducibility | Clean build on host and target | 100% |

---

## 10. Risk Assessment

### 10.1 Integration Risks

| Risk ID | Description | Likelihood | Impact | Mitigation |
|---------|-------------|------------|--------|------------|
| INT-RISK-01 | DLA mutex contention causing task delays | Medium | High | Verify mutex acquisition order; add timeout handling |
| INT-RISK-02 | EKF divergence with sensor noise | Medium | High | Verify covariance bounds; implement divergence detection |
| INT-RISK-03 | CSP memory fragmentation on long runs | Low | Medium | Monitor memory usage; implement pool allocation |
| INT-RISK-04 | Watchdog timeout during integration tests | Low | High | Use host stubs; test watchdog integration separately |
| INT-RISK-05 | I2C bus contention with multiple sensors | Medium | Medium | Verify I2C arbitration; add bus reset capability |
| INT-RISK-06 | Flight mode transitions during critical operations | Low | High | Implement transition guards; verify atomicity |

### 10.2 Risk Mitigation Strategies

1. **DLA Contention**: Verify mutex acquisition order is consistent across all tasks; implement timeout-based acquisition with fallback behavior
2. **EKF Divergence**: Implement divergence detection with automatic reset; verify covariance matrix remains positive definite
3. **Memory Management**: Monitor heap usage during extended integration tests; implement fixed-size memory pools for critical allocations
4. **I2C Bus Issues**: Implement bus reset on timeout; verify sensor initialization sequence is deterministic

---

## 11. Traceability Matrix

### 11.1 Integration Test to Requirement Mapping

| Requirement ID | Description | Integration Tests |
|----------------|-------------|------------------|
| FR-1 | IMU data reading at ≥10 Hz | INT-L1-IMU-01, INT-L1-IMU-02 |
| FR-2 | Attitude estimation via EKF | INT-L2-EKF-01, INT-L2-EKF-02, INT-L2-EKF-03 |
| FR-3 | LQR/PID stabilization | INT-L2-CTRL-01, INT-L2-CTRL-02, INT-L2-CTRL-03 |
| FR-4 | Attitude command processing | INT-L2-CTRL-01, INT-L4-TASK-01, INT-L4-TASK-02 |
| FR-6 | Magnetorquer de-saturation | INT-L2-MD-02, INT-L2-MD-03, INT-L2-MD-04 |
| FR-7 | Telemetry downlink | INT-L4-COMM-01, INT-L4-COMM-02, INT-L4-TASK-03 |
| FR-8 | Health monitoring | INT-L3-WDT-01, INT-L3-WDT-04, INT-L4-TASK-04 |
| FR-9 | Flight mode management | INT-L3-FMM-01, INT-L3-FMM-02, INT-L3-FMM-03, INT-L3-FMM-04 |
| FR-10a | Flash storage | INT-L3-SVC-04 |
| FR-11 | Magnetometer reading | INT-L1-MAG-01, INT-L1-MAG-02, INT-L1-MAG-03 |
| FR-15 | Payload integration | INT-L4-PAY-01, INT-L4-PAY-02, INT-L4-PAY-03 |
| FR-18 | GPS position acquisition | INT-L1-GPS-01, INT-L1-GPS-02, INT-L1-GPS-03 |
| FR-19 | GPS time synchronization | INT-L1-GPS-02, INT-L2-EKF-06 |
| SR-1 | Watchdog fault recovery | INT-L3-WDT-01, INT-L3-WDT-02, INT-L3-WDT-03 |
| SR-2 | Safe mode on failure | INT-L3-FM-01, INT-L3-FM-02, INT-L3-FM-03, INT-L3-FM-04 |
| SYS-F-100 | Sensor data acquisition | INT-L1-IMU-01, INT-L1-MAG-01, INT-L1-GPS-01 |
| SYS-F-101 | EKF 6-state vector | INT-L2-EKF-01 |
| SYS-F-102 | RK2 dynamics integration | INT-L2-CTRL-07 |
| SYS-F-103 | Gyro bias estimation | INT-L2-EKF-02 |
| SYS-F-104 | Accelerometer roll/pitch update | INT-L2-EKF-03 |
| SYS-F-105 | Magnetometer yaw update | INT-L2-EKF-04 |
| SYS-F-110 | Attitude control task | INT-L2-CTRL-01 |
| SYS-F-111 | LQR full-state controller | INT-L2-CTRL-02 |
| SYS-F-113 | Gain scheduling | INT-L2-CTRL-05 |
| SYS-F-114 | B-dot momentum dump | INT-L2-MD-01, INT-L2-MD-02, INT-L2-MD-03, INT-L2-MD-04 |
| SYS-F-115 | Flight mode transitions | INT-L3-FMM-01, INT-L3-FMM-02, INT-L3-FMM-03 |
| SYS-F-204 | Fault → SAFE transition | INT-L3-FM-01 |
| SYS-F-205 | SAFE mode stability | INT-L3-FM-04 |
| SYS-F-211 | Watchdog fault detection | INT-L3-WDT-01 |
| SYS-F-212 | Watchdog recovery | INT-L3-WDT-01, INT-L3-WDT-02 |
| SYS-F-213 | Watchdog timeout handling | INT-L3-WDT-03 |
| SYS-F-301 | Event logging | INT-L3-FMM-06, INT-L3-FM-06 |
| SYS-F-302 | Log event formatting | INT-L3-SVC-04 |
| SYS-F-461 | GPS NMEA parsing | INT-L1-GPS-01, INT-L1-GPS-02 |
| SYS-F-462 | GPS data validation | INT-L1-GPS-04, INT-L1-GPS-05 |
| SYS-F-463 | GPS time extraction | INT-L1-GPS-02 |

### 11.2 Test File to Integration Level Mapping

| Integration Level | Test Files | Test Count |
|------------------|------------|------------|
| Level 1: Driver | `test_i2c_mpu6050.c`, `test_mag_integration.c`, `test_gps_integration.c` | 16 |
| Level 2: Control | `test_ekf_integration.c`, `test_control_loop.c`, `test_momentum_dump.c` | 18 |
| Level 3: Services | `test_fmm_integration.c`, `test_fault_safe.c`, `test_safe_trigger.c`, `test_services_integration.c` | 22 |
| Level 4: Full System | `test_tasks_integration.c`, `test_comm_integration.c`, `test_payload_integration.c`, `test_e2e_integration.c` | 21 |
| **Total** | | **77** |

---

**END OF DOCUMENT**
