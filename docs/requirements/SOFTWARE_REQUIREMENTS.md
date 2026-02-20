# Software Requirements Specification

**Document ID**: REQ-001  
**Version**: 1.0  
**Last Updated**: 2026-02-20  
**Status**: Active  
**Standard**: Based on ECSS-E-ST-10-06C

---

## 1. Functional Requirements

| ID | Requirement | Priority | Phase | Status |
|----|------------|----------|-------|--------|
| **FR-1** | The system shall read 6-DOF IMU data (accel + gyro) via I2C at ≥10 Hz | Must | 2 | ✅ API ready, driver TBD |
| **FR-2** | The system shall compute Euler angles (roll, pitch, yaw) from sensor data | Must | 1 | ✅ Implemented |
| **FR-3** | The system shall stabilize angular rates via independent PID loops (3 axes) | Must | 1 | ✅ Implemented, tested |
| **FR-4** | The system shall command desired attitude and compute torque setpoints | Must | 1 | ✅ Implemented |
| **FR-5** | The system shall apply torque commands to reaction wheel motors (3-axis) | Must | 1 | ✅ Implemented, tested |
| **FR-6** | The system shall apply magnetic dipole commands for de-saturation (3-axis) | Must | 1 | ✅ Implemented, tested |
| **FR-7** | The system shall transmit attitude, rates, and sensor data to ground station | Must | 3 | ⏳ Pending |
| **FR-8** | The system shall monitor bus voltage, temperature, and task health | Should | 2 | ✅ API ready |
| **FR-9** | The system shall support flashing firmware via USB (UF2 format) | Must | 2 | ✅ Verified |
| **FR-10** | The system shall provide USB CDC serial debug output | Should | 2 | ✅ Verified |

---

## 2. Non-Functional Requirements

| ID | Requirement | Priority | Phase | Status |
|----|------------|----------|-------|--------|
| **NFR-1** | Control loop shall execute at 20 Hz with jitter <10 ms | Must | 2 | TBC — pending task scheduling validation |
| **NFR-2** | Sensor read task shall execute at 10 Hz | Must | 2 | TBC — pending HW validation |
| **NFR-3** | No dynamic memory allocation in flight code (static only) | Must | 1 | ✅ Enforced |
| **NFR-4** | Average power consumption <2 W during nominal operation | Should | 3 | TBD — requires power profiling |
| **NFR-5** | Architecture shall support adding new sensors/actuators without core changes | Should | 1 | ✅ HAL pattern |
| **NFR-6** | Flash footprint <200 KB, SRAM usage <60 KB | Should | 2 | TBC — current blink: 536 KB UF2 |
| **NFR-7** | System shall boot to operational state within 5 seconds | Should | 2 | TBD |
| **NFR-8** | All code shall compile with `-Wall -Wextra -pedantic` without warnings | Must | 1 | ✅ Enforced |

---

## 3. Interface Requirements

| ID | Requirement | Protocol | Status |
|----|------------|----------|--------|
| **IR-1** | IMU sensor (MPU6050) communication | I2C, 400 kHz | TBD — Task 2.3 |
| **IR-2** | Temperature sensor communication | I2C or ADC | TBD — Task 2.3 |
| **IR-3** | Debug serial output | USB CDC, 115200 baud | ✅ Verified |
| **IR-4** | Ground station telemetry | WiFi TCP/UDP | TBD — Phase 3 |
| **IR-5** | Reaction wheel control | PWM/SPI | TBD — Phase 3+ |
| **IR-6** | Magnetorquer control | PWM | TBD — Phase 3+ |

---

## 4. Safety & Reliability Requirements

| ID | Requirement | Priority | Status |
|----|------------|----------|--------|
| **SR-1** | System shall implement watchdog timer for fault recovery | Must | TBD — Phase 5 |
| **SR-2** | System shall enter safe mode on critical failure detection | Must | TBD — Phase 5 |
| **SR-3** | No hardcoded secrets or credentials in firmware | Must | ✅ Verified |
| **SR-4** | Bounds checking on all array/buffer accesses | Must | ✅ Enforced |
| **SR-5** | Stack overflow detection enabled for all tasks | Must | ✅ FreeRTOSConfig.h |

---

## 5. Compliance Matrix

| Standard | Applicable Section | Compliance |
|----------|--------------------|------------|
| ECSS-Q-ST-80C | Software quality & architecture | ✅ Partial |
| MISRA C | Code safety & reliability | ✅ Conventions adopted |
| IEC 61508 | Functional safety foundation | ✅ Static memory, priorities |
| NASA SWE-130 | Software assurance practices | ✅ Modular design, testing |

---

## 6. Requirements Status Summary

| Category | Total | Implemented | TBD | TBC |
|----------|-------|-------------|-----|-----|
| Functional | 10 | 8 | 1 | 1 |
| Non-Functional | 8 | 4 | 2 | 2 |
| Interface | 6 | 1 | 5 | 0 |
| Safety | 5 | 3 | 2 | 0 |
| **Total** | **29** | **16** | **10** | **3** |
