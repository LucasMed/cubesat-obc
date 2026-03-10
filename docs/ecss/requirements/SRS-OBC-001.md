# Software Requirements Specification

**Document ID**: SRS-OBC-001  
**Version**: 2.1  
**Last Updated**: 2026-03-10  
**Status**: Active  
**Standard**: Based on ECSS-E-ST-10-06C

---

## Change History

| Version | Date       | Author               | Description                                                          |
|---------|------------|----------------------|----------------------------------------------------------------------|
| 2.0     | 2026-03-08 | OBC Systems Team     | Initial active baseline                                              |
| 2.1     | 2026-03-10 | OBC Systems Team     | SRR-OBC-001 AIR resolution: ACT-01 (doc ID), ACT-03 (NFR-6 budget), ACT-05 (IR-4 interface), ACT-10-B (NFR-7 boot time) |

---

## 1. Functional Requirements

| ID | Requirement | Priority | Phase | Status |
|----|------------|----------|-------|--------|
| **FR-1** | The system shall read 6-DOF IMU data (accel + gyro) via I2C at ≥10 Hz | Must | 2 | ✅ Implemented, EKF-integrated |
| **FR-2** | The system shall estimate attitude (roll, pitch, yaw) from sensor data via EKF | Must | 4 | ✅ EKF complete (Phase 4) |
| **FR-3** | The system shall stabilize angular rates via LQR / PID (3 axes) | Must | 4 | ✅ Implemented (Phase 4) |
| **FR-4** | The system shall command desired attitude and compute torque setpoints | Must | 1 | ✅ Implemented |
| **FR-5** | The system shall apply torque commands to reaction wheel motors (3-axis) | Must | 1 | ✅ Implemented, tested |
| **FR-6** | The system shall apply magnetic dipole commands for de-saturation (3-axis) | Must | 5 | 🔄 PR-17 (momentum dump) |
| **FR-7** | The system shall transmit attitude, rates, and sensor data to ground station | Must | 3 | ⏳ Stub complete, HW pending |
| **FR-8** | The system shall monitor bus voltage, temperature, and task health | Should | 2 | ✅ Implemented (Phase SA) |
| **FR-9** | The system shall support flashing firmware via USB (UF2 format) | Must | 2 | ✅ Verified |
| **FR-10** | The system shall provide USB CDC serial debug output | Should | 2 | ✅ Verified |
| **FR-11** | The system shall read 3-axis magnetometer via I2C at ≥10 Hz | Must | 5 | 🔄 PR-18 (HMC5883L stub) |
| **FR-12** | The system shall feed the hardware watchdog from the health monitor task | Must | 5 | 🔄 PR-16 (watchdog HAL) |

---

## 2. Non-Functional Requirements

| ID | Requirement | Priority | Phase | Status |
|----|------------|----------|-------|--------|
| **NFR-1** | Control loop shall execute at 20 Hz with jitter <10 ms | Must | 2 | ✅ Verified ±22 µs (Phase 2) |
| **NFR-2** | Sensor read task shall execute at 10 Hz | Must | 2 | ✅ Verified (Phase 2) |
| **NFR-3** | No dynamic memory allocation in flight code (static only) | Must | 1 | ✅ Enforced |
| **NFR-4** | Average power consumption <2 W during nominal operation | Should | 3 | ⏳ TBD — requires power profiling on HW |
| **NFR-5** | Architecture shall support adding new sensors/actuators without core changes | Should | 1 | ✅ HAL pattern |
| **NFR-6** | Flash image size (`.text`+`.data`+`.rodata` of linked ELF) ≤ 500 KB; SRAM (`.bss`+`.data`) ≤ 200 KB | Should | 2 | ✅ Measured: `.text`=348 KB, `.bss`=155 KB on `cubesat_obc_pico.elf` (2 MB flash, 520 KB SRAM device — 17% and 30% utilization). Previous budget of 200 KB/60 KB was set for minimal firmware and is superseded. |
| **NFR-7** | System shall boot to operational state within 10 s of power-on (excluding USB CDC enumeration) — per SyRS-NF-005 | Should | 2 | ✅ Measured within spec (SyRS-NF-005 [IMPL]) |
| **NFR-8** | All code shall compile with `-Wall -Wextra -pedantic` without warnings | Must | 1 | ✅ Enforced |

---

## 3. Interface Requirements

| ID | Requirement | Protocol | Status |
|----|------------|----------|--------|
| **IR-1** | IMU sensor (MPU6050) communication | I2C, 400 kHz | TBD — Task 2.3 |
| **IR-2** | Temperature sensor communication | I2C or ADC | TBD — Task 2.3 |
| **IR-3** | Debug serial output | USB CDC, 115200 baud | ✅ Verified |
| **IR-4** | Ground station TT&C link: CSP v2.2 over UART1, KISS framing, 433 MHz LoRa (E22-400M30S), 9600 baud | UART1/KISS/CSP | ✅ Implemented — see `src/core/comm_init.c`, ICD-OBC-001 §8. WiFi-based telemetry (CYW43) is formally descoped from this release. |
| **IR-5** | Reaction wheel control | PWM/SPI | TBD — Phase 3+ |
| **IR-6** | Magnetorquer control | PWM | TBD — Phase 3+ |

---

## 4. Safety & Reliability Requirements

| ID | Requirement | Priority | Status |
|----|------------|----------|--------|
| **SR-1** | System shall implement watchdog timer for fault recovery | Must | 🔄 PR-16 |
| **SR-2** | System shall enter safe mode on critical failure detection | Must | ✅ FM_SAFE via FMM + fault_manager (Phase SA) |
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

| Category | Total | Implemented | In Progress | TBD | TBC |
|----------|-------|-------------|-------------|-----|-----|
| Functional | 12 | 9 | 2 | 1 | 0 |
| Non-Functional | 8 | 5 | 0 | 2 | 1 |
| Interface | 6 | 1 | 0 | 5 | 0 |
| Safety | 5 | 4 | 1 | 0 | 0 |
| **Total** | **31** | **19** | **3** | **8** | **1** |
