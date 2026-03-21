# FMEA-OBC-002 — Software Failure Mode and Effects Analysis

| Field           | Value                                           |
|-----------------|------------------------------------------------|
| Document ID     | FMEA-OBC-002                                   |
| Version         | 1.0                                            |
| Date            | 2026-03-21                                     |
| Author          | OBC Systems Team                               |
| Status          | Approved — CDR Baseline                        |
| Classification  | Internal                                       |

---

## Change History

| Version | Date       | Author           | Description                                |
|---------|------------|------------------|--------------------------------------------|
| 1.0     | 2026-03-21 | OBC Systems Team | Initial release — software FMEA v1.0       |

---

## Table of Contents

1. [Introduction](#1-introduction)
2. [Purpose and Scope](#2-purpose-and-scope)
3. [Software Components Analyzed](#3-software-components-analyzed)
4. [Applicable Documents](#4-applicable-documents)
5. [Acronyms and Definitions](#5-acronyms-and-definitions)
6. [FMEA Methodology](#6-fmea-methodology)
7. [Software Component FMEA](#7-software-component-fmea)
   - [7.1 FreeRTOS Scheduler](#71-freertos-scheduler)
   - [7.2 EKF Estimator](#72-ekf-estimator)
   - [7.3 LQR Controller](#73-lqr-controller)
   - [7.4 Flight Mode Manager](#74-flight-mode-manager)
   - [7.5 Fault Manager](#75-fault-manager)
   - [7.6 Driver Layer](#76-driver-layer)
   - [7.7 Data Layer Abstraction](#77-data-layer-abstraction)
   - [7.8 Event Logger](#78-event-logger)
8. [Failure Mode Summary](#8-failure-mode-summary)
9. [Criticality Assessment](#9-criticality-assessment)
10. [Mitigation Strategies](#10-mitigation-strategies)
11. [Criticality Category Definitions](#11-criticality-category-definitions)
12. [Open Items](#12-open-items)
13. [References](#13-references)

---

## 1. Introduction

This document is the Software Failure Mode and Effects Analysis (FMEA) for the CubeSat OBC flight software, complementing FMEA-OBC-001 which addresses hardware and system-level faults. This analysis focuses specifically on software implementation faults, algorithmic failures, concurrency issues, and RTOS-related failure modes that may affect mission success.

The software FMEA systematically identifies failure modes in each software component, assesses their effects on the spacecraft and mission objectives, and maps them to the implemented mitigation strategies through coding standards, testing, and static analysis.

---

## 2. Purpose and Scope

### 2.1 Purpose

This document provides a systematic analysis of software failure modes for the CubeSat OBC flight software running on the Raspberry Pi Pico 2W (RP2350) under FreeRTOS. The purpose is to:

- Identify software-specific failure modes not covered by hardware FMEA-OBC-001
- Assess the impact of software failures on mission objectives
- Define mitigation strategies through development process controls
- Support software verification and validation activities

### 2.2 Scope

**In Scope:**
- All flight software components listed in Section 3
- FreeRTOS kernel configuration and scheduling behavior
- Control algorithms (EKF, LQR)
- System services (FMM, Fault Manager, Data Layer)
- Driver software (IMU, GPS, Magnetometer)
- Concurrency and inter-task communication

**Out of Scope:**
- Hardware failures (addressed in FMEA-OBC-001)
- Ground segment software
- Third-party libraries (FreeRTOS kernel, CSP) — assumed verified by suppliers
- Radiation-induced single-event effects in hardware

---

## 3. Software Components Analyzed

| Component | Location | Description |
|-----------|----------|-------------|
| FreeRTOS Scheduler | `src/` | Task scheduling, context switching, inter-task communication |
| EKF Estimator | `src/control/ekf.c` | Extended Kalman Filter for attitude estimation |
| LQR Controller | `src/control/lqr.c` | Linear Quadratic Regulator for attitude control |
| Flight Mode Manager | `src/services/fmm/` | Mode state machine and transitions |
| Fault Manager | `src/services/fault/` | Fault detection, classification, and FDIR |
| IMU Driver | `src/drivers/imu/mpu6050.c` | MPU-6050 inertial measurement unit driver |
| GPS Driver | `src/drivers/gps/neo7m.c` | NEO-7M GPS receiver driver |
| Magnetometer Driver | `src/drivers/mag/hmc5883l.c` | HMC5883L magnetometer driver |
| Data Layer Abstraction | `src/core/data_layer.c` | Thread-safe shared state management |
| Event Logger | `src/core/event_logger.c` | Persistent event recording to flash |

---

## 4. Applicable Documents

| ID              | Title                                              | Version |
|-----------------|----------------------------------------------------|---------|
| FMEA-OBC-001    | Hardware FMEA                                      | 0.2     |
| FAULT-DES-001   | Fault Manager Design Document                      | 0.2     |
| FMM-DES-001     | Flight Mode Manager Design Document                | 0.4     |
| DL-DES-001      | Data Layer Abstraction Design Document            | 0.1     |
| FSW-SDD-001     | Flight Software Design Description                 | 0.2     |
| SRS-OBC-001     | Software Requirements Specification                | 2.0     |
| CODING_STANDARDS| Coding Standards Document                          | 1.0     |
| ECSS-E-ST-40C   | Software Engineering Standard                      | —       |
| ECSS-Q-ST-30-02C| Failure modes, effects (and criticality) analysis | —       |

---

## 5. Acronyms and Definitions

| Term       | Definition                                                    |
|------------|---------------------------------------------------------------|
| FMEA       | Failure Mode and Effects Analysis                             |
| FMECA      | FMEA + Criticality Analysis                                   |
| FDIR       | Fault Detection, Isolation and Recovery                       |
| RPN        | Risk Priority Number = Severity × Occurrence × Detectability  |
| EKF        | Extended Kalman Filter                                       |
| LQR        | Linear Quadratic Regulator                                   |
| FMM        | Flight Mode Manager                                          |
| DLA        | Data Layer Abstraction                                       |
| HWM        | High Water Mark (FreeRTOS stack metric)                      |
| ISR        | Interrupt Service Routine                                    |
| SVO        | Severity (1-4 scale)                                          |
| OCC        | Occurrence (1-4 scale)                                       |
| DET        | Detectability (1-4 scale)                                    |
| CR         | Criticality Number                                           |

---

## 6. FMEA Methodology

### 6.1 Scoring Scales

**Severity (SVO):**

| Score | Level    | Effect on Mission / Spacecraft |
|-------|----------|-------------------------------|
| 4     | CATASTROPHIC | Mission loss; permanent loss of spacecraft control |
| 3     | CRITICAL | Primary mission objectives compromised; spacecraft survives but MO-1..MO-3 at risk |
| 2     | MARGINAL | Secondary mission degradation; workaround exists; partial MO loss |
| 1     | MINOR    | No mission impact; cosmetic or logged anomaly |

**Occurrence (OCC):**

| Score | Level | Description |
|-------|-------|-------------|
| 4     | FREQUENT | Expected to occur multiple times during mission |
| 3     | PROBABLE | Expected to occur at least once during mission |
| 2     | REMOTE | Unlikely but possible; may occur under extreme conditions |
| 1     | IMPROBABLE | Almost impossible; design prevents occurrence |

**Detectability (DET):**

| Score | Level | Description |
|-------|-------|-------------|
| 4     | ABSOLUTE | Cannot be detected by any means before effect occurs |
| 3     | LOW | Detectable only through post-flight analysis or extended monitoring |
| 2     | MODERATE | Detectable by ground station monitoring or onboard diagnostics |
| 1     | HIGH | Detectable immediately by fault manager or self-check mechanisms |

### 6.2 Criticality Calculation

Software Criticality Number (CR) = SVO × OCC

| CR Range | Criticality Category | Action Required |
|----------|---------------------|-----------------|
| 16 | Category I (Extremely Critical) | Immediate mitigation required |
| 12-15 | Category II (Critical) | Mitigation required before flight |
| 6-11 | Category III (Marginal) | Mitigation planned; monitoring required |
| 1-5 | Category IV (Minor) | Acceptable; document rationale |

### 6.3 Mitigation Categories

| Code | Mitigation Method |
|------|------------------|
| STD | Coding standards compliance (CODING_STANDARDS.md) |
| STA | Static analysis (cppcheck, compiler warnings) |
| UNT | Unit testing |
| INT | Integration testing |
| REV | Code review |
| DES | Design-level mitigation (watchdog, timeout mechanisms) |

---

## 7. Software Component FMEA

### 7.1 FreeRTOS Scheduler

| ID | Component | Failure Mode | Local Effect | System Effect | SVO | OCC | DET | CR | Mitigation |
|----|-----------|-------------|--------------|---------------|-----|-----|-----|----|------------|
| SW-FRTOS-001 | Scheduler | Task priority inversion | High-priority task delayed | Sensor/Control loop starvation; attitude drift | 4 | 2 | 1 | 8 | STD: Priority ceiling in CODING_STANDARDS; REV: Review all task priorities |
| SW-FRTOS-002 | Scheduler | Stack overflow | Task crash; undefined behavior | System instability; potential watchdog reset | 3 | 2 | 2 | 12 | DES: 2048-word stacks; HWM monitoring in health task; UNT: Stack overflow tests |
| SW-FRTOS-003 | Scheduler | Priority inversion on mutex | Task blocked indefinitely | Sensor or control task hangs; FM_SAFE transition | 4 | 1 | 1 | 4 | STD: Critical sections <1ms; DES: Priority inheritance when available |
| SW-FRTOS-004 | Scheduler | Context switch corruption | CPU register state corrupted | Unpredictable task behavior; possible crash | 3 | 1 | 2 | 6 | DES: Single-core mode (configNUMBER_OF_CORES=1); FPU disabled (-mfloat-abi=soft) |
| SW-FRTOS-005 | Scheduler | Timer tick starvation | All tasks blocked; system halt | Complete OBC failure; watchdog reset | 4 | 1 | 1 | 4 | DES: Watchdog timer (TPS3431) at 8s timeout; INT: Timer interrupt verification |
| SW-FRTOS-006 | Scheduler | Deadlock between tasks | Two or more tasks blocked | System partial freeze; HK may continue | 3 | 1 | 2 | 6 | STD: No nested mutex acquisition; INT: Deadlock injection test |
| SW-FRTOS-007 | Scheduler | Queue overflow | Inter-task message dropped | Missing sensor data; control based on stale data | 2 | 2 | 1 | 4 | DES: 10-element queue sizes; overflow detection in fault manager |
| SW-FRTOS-008 | Scheduler | Memory allocation failure | pvPortMalloc returns NULL | Task initialization fails; FM_SAFE | 3 | 1 | 1 | 3 | STD: No malloc() in flight code; static allocation only; DES: 32KB heap pre-allocated |

### 7.2 EKF Estimator

| ID | Component | Failure Mode | Local Effect | System Effect | SVO | OCC | DET | CR | Mitigation |
|----|-----------|-------------|--------------|---------------|-----|-----|-----|----|------------|
| SW-EKF-001 | EKF | Quaternion norm divergence | q not unit quaternion | Invalid attitude output; actuator saturation | 3 | 2 | 1 | 6 | DES: Re-normalization after each update; FAULT_EST_QUAT_NORM fault |
| SW-EKF-002 | EKF | Covariance matrix blowup | P matrix elements exceed bounds | State estimate unusable | 4 | 1 | 2 | 8 | DES: Covariance norm check; FAULT_EST_DIVERGENCE; reset EKF state |
| SW-EKF-003 | EKF | Division by zero in S⁻¹ | Algorithm failure | Crash or invalid output | 3 | 1 | 1 | 3 | STD: Bounds check before division; UNT: Zero-input test cases |
| SW-EKF-004 | EKF | Gyro bias windup | Bias estimates grow unbounded | Estimation accuracy degrades over time | 2 | 2 | 2 | 8 | DES: Bias limit clamping; UNT: Long-duration simulation tests |
| SW-EKF-005 | EKF | Matrix inversion failure | S⁻¹ computation fails | Attitude estimate stuck at last valid value | 3 | 1 | 1 | 3 | DES: Determinant check; fallback to last valid state; UNT: Ill-conditioned input tests |
| SW-EKF-006 | EKF | Input data NaN propagation | All outputs become NaN | Invalid control commands; potential actuator fault | 4 | 1 | 1 | 4 | STD: NaN checks on all inputs; DES: Fault manager integration |
| SW-EKF-007 | EKF | Jacobian computation error | F or H matrices incorrect | Convergence to wrong state | 3 | 1 | 2 | 6 | UNT: Analytical vs numerical Jacobian comparison tests |
| SW-EKF-008 | EKF | Timestamp discontinuity | dt calculation error | Integration instability | 2 | 1 | 1 | 2 | DES: Timestamp validation; vTaskDelayUntil for deterministic dt |

### 7.3 LQR Controller

| ID | Component | Failure Mode | Local Effect | System Effect | SVO | OCC | DET | CR | Mitigation |
|----|-----------|-------------|--------------|---------------|-----|-----|-----|----|------------|
| SW-LQR-001 | LQR | Gain matrix corruption | Incorrect control output | Attitude divergence; potential tumble | 4 | 1 | 2 | 8 | STD: const matrices in flash; DES: Range check on outputs; UNT: Matrix element tests |
| SW-LQR-002 | LQR | Output saturation | Torque command clipped | Slower maneuver execution; MO timeline impact | 2 | 3 | 1 | 6 | DES: FAULT_CTRL_OUTPUT_SATURATED logged; FAULT_CTRL_RATE_LIMIT |
| SW-LQR-003 | LQR | State vector NaN | All outputs NaN | Invalid actuator commands; possible fault | 4 | 1 | 1 | 4 | STD: NaN/INF validation; DES: Fault manager integration |
| SW-LQR-004 | LQR | Wrong gain schedule selection | Inappropriate gains applied | Suboptimal control; potential instability | 3 | 1 | 2 | 6 | DES: Mode validation before gain selection; INT: Mode transition tests |
| SW-LQR-005 | LQR | Integrator windup (PID fallback) | Integral term grows unbounded | Overshoot; oscillation | 2 | 2 | 1 | 4 | DES: Anti-windup in pid_controller.c; UNT: Saturation boundary tests |
| SW-LQR-006 | LQR | Rate limit violation | Commanded rate exceeds safe limit | Mechanical stress; RW saturation | 3 | 1 | 1 | 3 | DES: FAULT_CTRL_RATE_LIMIT; rate limiter in attitude_control_task.c |

### 7.4 Flight Mode Manager

| ID | Component | Failure Mode | Local Effect | System Effect | SVO | OCC | DET | CR | Mitigation |
|----|-----------|-------------|--------------|---------------|-----|-----|-----|----|------------|
| SW-FMM-001 | FMM | Invalid mode transition | Transition to disallowed mode | System in undefined state; FDIR confusion | 3 | 1 | 1 | 3 | DES: g_allowed[][] matrix enforcement; UNT: All transition combinations tested |
| SW-FMM-002 | FMM | Mode state corruption | Current mode unknown | Incorrect subsystem behavior dispatch | 4 | 1 | 1 | 4 | DES: Single enum variable; mutex-protected access via DLA |
| SW-FMM-003 | FMM | Forced safe blocked | fmm_force_safe() fails | CRITICAL fault not acted upon | 4 | 1 | 1 | 4 | DES: ISR-safe flag mechanism planned (OI-5); watchdog reset as fallback |
| SW-FMM-004 | FMM | Mode transition race | Concurrent transition requests | Unpredictable final mode | 3 | 1 | 1 | 3 | DES: FMM mutex serialization; UNT: Concurrent transition tests |
| SW-FMM-005 | FMM | Event log failure | LOG_EVT_MODE_CHANGE not recorded | Loss of mode history; anomaly investigation harder | 1 | 2 | 2 | 4 | DES: Event logger with ring buffer; INT: Log integrity verification |

### 7.5 Fault Manager

| ID | Component | Failure Mode | Local Effect | System Effect | SVO | OCC | DET | CR | Mitigation |
|----|-----------|-------------|--------------|---------------|-----|-----|-----|----|------------|
| SW-FM-001 | Fault Manager | Fault table overflow | New faults silently dropped | Undetected faults; missed FDIR | 4 | 1 | 2 | 8 | DES: Fixed 32-slot table; FAULT_ID_COUNT=25 < capacity; STA: Static allocation analysis |
| SW-FM-002 | Fault Manager | Critical section too long | Interrupts disabled too long | Missed timing deadlines; sensor data loss | 3 | 1 | 1 | 3 | DES: O(32) linear scan <2µs; STD: No blocking calls in critical section |
| SW-FM-003 | Fault Manager | Fault level escalation logic error | Severity under-reported | CRITICAL fault treated as WARNING | 4 | 1 | 1 | 4 | UNT: Level escalation test cases; REV: Algorithm review |
| SW-FM-004 | Fault Manager | Ageing timer error | WARNING faults persist | Fault table fills with stale entries | 2 | 1 | 2 | 4 | DES: 30-tick aging; UNT: Aging timer tests |
| SW-FM-005 | Fault Manager | CRITICAL fault bypass | fmm_force_safe() not called | FM_SAFE not entered on critical fault | 4 | 1 | 1 | 4 | UNT: T-FM-06 critical_triggers_safe; INT: CRITICAL fault injection |

### 7.6 Driver Layer

| ID | Component | Failure Mode | Local Effect | System Effect | SVO | OCC | DET | CR | Mitigation |
|----|-----------|-------------|--------------|---------------|-----|-----|-----|----|------------|
| SW-DRV-001 | IMU Driver | I²C bus stuck | mpu6050_read() hangs | EKF starved; attitude unknown | 3 | 2 | 1 | 6 | DES: 3-retry with bus reset; FAULT_SENS_IMU_I2C_ERROR; INT: I²C error injection |
| SW-DRV-002 | IMU Driver | Data checksum failure | Invalid sensor data returned | Bad attitude estimate; possible tumble | 3 | 1 | 2 | 6 | DES: Data validity checks; staleness detection |
| SW-DRV-003 | IMU Driver | Register read/write failure | Configuration lost | Reduced accuracy; wrong measurement range | 2 | 2 | 1 | 4 | DES: Init sequence verification; INT: Driver init tests |
| SW-DRV-004 | GPS Driver | NMEA parse error | Invalid GPS data | Position unknown; no orbit determination | 2 | 2 | 1 | 4 | DES: Parse validation; FAULT_GPS_PARSE_ERROR; INT: NMEA injection tests |
| SW-DRV-005 | GPS Driver | UART buffer overflow | Characters dropped | Partial/corrupt NMEA message | 2 | 2 | 2 | 8 | DES: 256-byte buffer; overflow detection; INT: Buffer stress tests |
| SW-DRV-006 | GPS Driver | Fix timeout | No valid position data | GPS data stale; FM_NOMINAL may be inhibited | 2 | 2 | 1 | 4 | DES: Staleness timer; DLA marks gps_valid=false |
| SW-DRV-007 | Mag Driver | I²C timeout | mag_read() fails | EKF yaw update lost; attitude drift | 2 | 2 | 1 | 4 | DES: 3-retry; FAULT_EST_MAG_TIMEOUT; INT: Mag timeout tests |
| SW-DRV-008 | Mag Driver | Clones/different IC detection | Wrong register map accessed | Invalid magnetometer data | 3 | 2 | 2 | 12 | DES: HMC5883L vs QMC5883L detection; hardware verification at build |
| SW-DRV-009 | Driver Framework | Null pointer dereference | Crash on bad handle | Task crash; potential watchdog reset | 4 | 1 | 1 | 4 | STD: NULL checks on all handle parameters; STA: Coverity analysis |
| SW-DRV-010 | Driver Framework | Buffer overflow | Memory corruption | Unpredictable behavior; potential exploit | 4 | 1 | 1 | 4 | STD: Bounds checking; STA: cppcheck overflow checks |

### 7.7 Data Layer Abstraction

| ID | Component | Failure Mode | Local Effect | System Effect | SVO | OCC | DET | CR | Mitigation |
|----|-----------|-------------|--------------|---------------|-----|-----|-----|----|------------|
| SW-DL-001 | DLA | Mutex deadlock | All DLA accesses blocked | System freeze; watchdog reset | 4 | 1 | 1 | 4 | STD: No nested DLA mutex acquisition; REV: Locking order analysis |
| SW-DL-002 | DLA | Snapshot inconsistency | Partial read of old/new state | Race condition between fields | 3 | 1 | 1 | 3 | DES: Atomic snapshot copy; UNT: Concurrent access tests |
| SW-DL-003 | DLA | Field corruption | Invalid data written | Wrong subsystem decisions | 3 | 1 | 2 | 6 | DES: Type-safe accessors; STD: No direct struct member access |
| SW-DL-004 | DLA | Priority inversion | Low-priority task holds mutex | High-priority task blocked; timing miss | 3 | 1 | 1 | 3 | DES: Fast mutex release; critical sections <1ms per coding standards |

### 7.8 Event Logger

| ID | Component | Failure Mode | Local Effect | System Effect | SVO | OCC | DET | CR | Mitigation |
|----|-----------|-------------|--------------|---------------|-----|-----|-----|----|------------|
| SW-LOG-001 | Event Logger | Flash write failure | Event not persisted | Loss of fault history | 2 | 2 | 2 | 8 | DES: Ring buffer with 3-slot backup; power-fail-safe write; INT: Flash write tests |
| SW-LOG-002 | Event Logger | Ring buffer overflow | Old events overwritten | Limited fault history | 1 | 3 | 1 | 3 | DES: 256-event ring buffer; oldest event overwritten; priority given to critical events |
| SW-LOG-003 | Event Logger | CRC mismatch | Corrupt log entry detected | Event ignored; logged as corrupted | 1 | 2 | 1 | 2 | DES: Class A events have CRC-16; corrupted entries marked; INT: CRC validation tests |
| SW-LOG-004 | Event Logger | Timestamp error | Event times incorrect | Anomaly correlation difficult | 2 | 1 | 2 | 4 | DES: 64-bit timestamps from RTC; INT: Timestamp monotonicity tests |
| SW-LOG-005 | Event Logger | Blocking write | Logger task blocks caller | Timing deadline miss | 3 | 1 | 1 | 3 | DES: Non-blocking write to RAM buffer; async flush to flash; STD: No blocking I/O in ISR |

---

## 8. Failure Mode Summary

### 8.1 Criticality Distribution

| CR Range | Count | Percentage | Category |
|----------|-------|------------|----------|
| 16 | 0 | 0% | Category I |
| 12-15 | 2 | 5% | Category II |
| 6-11 | 14 | 37% | Category III |
| 1-5 | 22 | 58% | Category IV |

### 8.2 Category II (Critical) Items Requiring Mitigation

| ID | Component | Failure Mode | CR | Required Mitigation |
|----|-----------|-------------|----|---------------------|
| SW-FRTOS-002 | Scheduler | Stack overflow | 12 | HWM monitoring; unit tests |
| SW-DRV-008 | Mag Driver | Clone IC detection | 12 | Hardware verification |

### 8.3 Category III (Marginal) Items

| ID | Component | Failure Mode | CR | Required Mitigation |
|----|-----------|-------------|----|---------------------|
| SW-FRTOS-001 | Scheduler | Task priority inversion | 8 | Code review |
| SW-EKF-002 | EKF | Covariance blowup | 8 | FAULT_EST_DIVERGENCE |
| SW-EKF-004 | EKF | Gyro bias windup | 8 | Bias limit clamping |
| SW-LQR-001 | LQR | Gain matrix corruption | 8 | const matrices |
| SW-DRV-005 | GPS Driver | UART buffer overflow | 8 | Buffer overflow detection |
| SW-FM-001 | Fault Manager | Table overflow | 8 | Static allocation |
| SW-LOG-001 | Event Logger | Flash write failure | 8 | Ring buffer backup |
| SW-EKF-001 | EKF | Quaternion norm divergence | 6 | Re-normalization |
| SW-FRTOS-004 | Scheduler | Context switch corruption | 6 | Single-core mode |
| SW-FRTOS-006 | Scheduler | Deadlock between tasks | 6 | Code review |
| SW-LQR-002 | LQR | Output saturation | 6 | Saturation logging |
| SW-LQR-004 | LQR | Wrong gain schedule | 6 | Mode validation |
| SW-DRV-001 | IMU Driver | I²C bus stuck | 6 | Bus reset |
| SW-DRV-002 | IMU Driver | Data checksum failure | 6 | Validity checks |
| SW-DL-003 | DLA | Field corruption | 6 | Type-safe accessors |

---

## 9. Criticality Assessment

### 9.1 Critical Items by System Function

| System Function | Critical Items | Highest CR | Risk Level |
|-----------------|----------------|------------|------------|
| Real-time Scheduling | SW-FRTOS-002, SW-FRTOS-001 | 12 | MODERATE |
| Attitude Estimation | SW-EKF-002, SW-EKF-001 | 8 | LOW |
| Attitude Control | SW-LQR-001, SW-LQR-002 | 8 | LOW |
| Flight Mode Management | SW-FMM-002, SW-FMM-003 | 4 | LOW |
| Fault Management | SW-FM-001, SW-FM-005 | 8 | LOW |
| Sensor Drivers | SW-DRV-001, SW-DRV-008 | 12 | MODERATE |
| Data Management | SW-DL-001, SW-DL-002 | 6 | LOW |
| Event Logging | SW-LOG-001, SW-LOG-005 | 8 | LOW |

### 9.2 Mission Objective Impact Assessment

| Mission Objective | Relevant Software Components | Risk if Failed | Mitigation Status |
|-------------------|------------------------------|----------------|-------------------|
| MO-1: Detumble | EKF, LQR, Scheduler, FMM | HIGH | Implemented |
| MO-2: Nadir Pointing | EKF, LQR, Sensor Drivers | MEDIUM | Implemented |
| MO-3: Payload Operations | DLA, Event Logger, FMM | LOW | Implemented |
| MO-4: Telemetry | Event Logger, DLA, Drivers | LOW | Implemented |
| MO-5: Ground Commanding | FMM, Fault Manager | LOW | Implemented |

---

## 10. Mitigation Strategies

### 10.1 Coding Standards Compliance

All software components shall comply with CODING_STANDARDS.md requirements:

| Rule | Applies To | Verification Method |
|------|-----------|---------------------|
| No malloc/free | All flight code | STA: Coverity analysis |
| Static allocation only | All flight code | STA: cppcheck |
| stdint.h types | Hardware registers | REV: Code review |
| Error return conventions | All functions | UNT: Unit tests |
| Bounds checking | Array access | STA: cppcheck bounds |
| NULL pointer checks | All handle parameters | STA: Coverity |
| NaN/INF validation | Control algorithms | UNT: Edge case tests |

### 10.2 Static Analysis Requirements

| Tool | Configuration | Gate |
|------|--------------|------|
| cppcheck | `--enable=all --suppress=missingInclude` | Zero errors |
| GCC | `-Wall -Wextra -Werror` | Zero warnings |
| Coverity | Standard C rules | Zero defects |

### 10.3 Testing Requirements

| Test Type | Coverage Target | Critical Items |
|-----------|-----------------|----------------|
| Unit Tests | ≥80% line coverage per module | EKF, LQR, FMM, Fault Manager |
| Integration Tests | All subsystem interfaces | DLA, Scheduler, Drivers |
| Fault Injection | All FAULT_ID entries | Critical and High severity |

### 10.4 Code Review Requirements

| Review Type | Frequency | Focus Areas |
|-------------|-----------|-------------|
| Locking Review | Per PR | DLA, FMM, Fault Manager |
| Algorithm Review | Per module | EKF, LQR, PID |
| Safety Review | Pre-flight | All Category II/III items |

---

## 11. Criticality Category Definitions

| Category | Definition | Disposition |
|----------|------------|-------------|
| I (Extremely Critical) | Failure causes mission loss with no workaround | Shall not occur; design changes required |
| II (Critical) | Failure causes primary MO compromise | Shall be mitigated before flight |
| III (Marginal) | Failure causes secondary MO impact | Shall be mitigated or accepted with rationale |
| IV (Minor) | Failure has no mission impact | Acceptable; document rationale |

---

## 12. Open Items

| ID | Description | Category | Owner | Target |
|----|-------------|----------|-------|--------|
| OI-SW-1 | QMC5883L clone detection for HMC5883L — verify IC markings before procurement | II | Hardware Lead | Pre-flight |
| OI-SW-2 | ISR-safe fmm_force_safe() path (FMM-DES-001 OI-5) | III | Software Lead | Phase 2 |
| OI-SW-3 | FreeRTOS priority inheritance evaluation | III | Software Lead | Phase 2 |
| OI-SW-4 | Coverity static analysis integration in CI pipeline | III | DevOps | Phase 2 |
| OI-SW-5 | Fault injection test suite for all 25 fault IDs | III | Software Lead | Phase 2 |

---

## 13. References

| Ref | Document |
|-----|---------|
| [1] | FMEA-OBC-001 v0.2 — Hardware FMEA |
| [2] | FAULT-DES-001 v0.2 — Fault Manager Design Document |
| [3] | FMM-DES-001 v0.4 — Flight Mode Manager Design Document |
| [4] | DL-DES-001 v0.1 — Data Layer Abstraction Design Document |
| [5] | FSW-SDD-001 v0.2 — Flight Software Design Description |
| [6] | SRS-OBC-001 v2.0 — Software Requirements Specification |
| [7] | CODING_STANDARDS.md — Coding Standards Document |
| [8] | `src/control/ekf.c` — EKF Implementation |
| [9] | `src/control/lqr.c` — LQR Controller Implementation |
| [10] | `src/services/fmm/flight_mode_manager.c` — FMM Implementation |
| [11] | `src/services/fault/fault_manager.c` — Fault Manager Implementation |
| [12] | `src/core/data_layer.c` — Data Layer Implementation |
| [13] | `src/core/event_logger.c` — Event Logger Implementation |
| [14] | `src/drivers/imu/mpu6050.c` — IMU Driver |
| [15] | `src/drivers/gps/neo7m.c` — GPS Driver |
| [16] | `src/drivers/mag/hmc5883l.c` — Magnetometer Driver |
| [17] | ECSS-E-ST-40C — Software Engineering Standard |
| [18] | ECSS-Q-ST-30-02C — Failure modes, effects (and criticality) analysis |

---

**End of Document**
