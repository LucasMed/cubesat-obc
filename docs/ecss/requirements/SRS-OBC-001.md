# Software Requirements Specification

**Document ID**: SRS-OBC-001  
**Version**: 2.3  
**Last Updated**: 2026-03-10  
**Status**: Active  
**Standard**: Based on ECSS-E-ST-10-06C

---

## Change History

| Version | Date       | Author               | Description                                                          |
|---------|------------|----------------------|----------------------------------------------------------------------|
| 2.0     | 2026-03-08 | OBC Systems Team     | Initial active baseline                                              |
| 2.1     | 2026-03-10 | OBC Systems Team     | SRR-OBC-001 AIR resolution: ACT-01 (doc ID), ACT-03 (NFR-6 budget), ACT-05 (IR-4 interface), ACT-10-B (NFR-7 boot time) |
| 2.2     | 2026-03-10 | OBC Systems Team     | Phase 7 payload baseline: FR-13..17, NFR-9, IR-7..9, new §5 PLD-R-001..005; refs PAYLOAD-SPEC-001 |
| 2.3     | 2026-03-10 | OBC Systems Team     | GPS re-scoped into Phase 7 (AIR-OBC-001 ACT-06 Option A): FR-18..19, IR-10; ICD-OBC-001 §7 activated; WP-7.10 added |

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
| **FR-11** | The system shall read 3-axis magnetic field vector via I2C at ≥10 Hz and publish it to the EKF and momentum-dump service. **[EM: HMC5883L GY-271, I2C0 `0x1E`, 75 Hz — locked per ACT-11. CDR/FM: LIS3MDL I2C0 `0x1C` SA0=GND, 80 Hz — driver TBD]** | Must | 5 | 🔄 PR-18 (stub driver; I2C integration pending) |
| **FR-12** | The system shall feed the hardware watchdog from the health monitor task | Must | 5 | 🔄 PR-16 (watchdog HAL) |
| **FR-13** | The system shall acquire a JPEG image from the CAM-001 camera instrument (IMX219 via SPI1) upon receipt of a ground or internal command, and store the image to non-volatile storage | Must | 7 | ⏳ Phase 7 |
| **FR-14** | The system shall sample the MAG-001 scientific magnetometer (RM3100 via I2C0) at ≥ 10 Hz during `FM_PAYLOAD` and store samples to non-volatile storage | Must | 7 | ⏳ Phase 7 |
| **FR-15** | The system shall sample the RAD-001 radiation detector (PIN diode ADC1) at ≥ 1 Hz during `FM_PAYLOAD` and accumulate total dose to non-volatile storage | Must | 7 | ⏳ Phase 7 |
| **FR-16** | The system shall enable and disable the 5V payload power rail (GPIO21) on entry to and exit from `FM_PAYLOAD` respectively | Must | 7 | ⏳ Phase 7 |
| **FR-17** | The system shall include payload housekeeping data (MAG-001 field vector, RAD-001 dose rate, CAM-001 image count) in the telemetry stream during `FM_PAYLOAD` | Should | 7 | ⏳ Phase 7 |
| **FR-18** | The system shall read NMEA sentences (`$GPGGA`, `$GPRMC`) from the NEO-7M GPS module (GY-NEO6Mv2) via UART0 at ≥ 1 Hz during `FM_NOMINAL` and `FM_PAYLOAD`, and publish parsed position (latitude, longitude, altitude) and UTC time to the Data Layer | Must | 7 | ⏳ Phase 7 |
| **FR-19** | The system shall synchronise the internal software clock to GPS UTC time (from `$GPRMC`) within ± 500 ms on each valid fix acquisition | Should | 7 | ⏳ Phase 7 |

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
| **NFR-9** | The system shall provide non-volatile storage of at least 1 GB for payload data | Must | 7 | ⏳ Phase 7 — external flash or SDCard (see PAYLOAD-SPEC-001 §10.3) |

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
| **IR-7** | CAM-001 camera (IMX219) via SPI bridge: SPI1, GPIO10(SCK)/GPIO11(MOSI)/GPIO12(MISO)/GPIO13(CSn), ≤ 10 MHz | SPI1 | ⏳ Phase 7 |
| **IR-8** | MAG-001 scientific magnetometer (RM3100): I2C0 GPIO4/5, address 0x20, ≤ 400 kHz (shares bus with IMU and ADCS MAG) | I2C0 | ⏳ Phase 7 |
| **IR-9** | RAD-001 radiation detector (PIN diode): ADC1 GPIO27, analogue 0–3.3 V, 12-bit | ADC | ⏳ Phase 7 |
| **IR-10** | GPS module (GY-NEO6Mv2 / NEO-7M): UART0 GPIO0 (TX) / GPIO1 (RX), 9600 baud, NMEA 0183 (`$GPGGA` / `$GPRMC`); 3.3 V supply (always-on rail) | UART0 | ⏳ Phase 7 |

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

## 5. Payload System Requirements (PLD-R)

Derived from PAYLOAD-SPEC-001 §1 and MRD-OBC-001 OI-1. These requirements are
allocated to Phase 7 and are baselined at Phase 7 PDR.

| ID | Requirement | Priority | Phase | Status |
|----|------------|----------|-------|--------|
| **PLD-R-001** | The spacecraft shall provide a 5V payload power rail capable of supplying up to 5 W continuously to the payload suite PLS-001. The rail shall be software-controlled (GPIO21 PAYLOAD_ENABLE) and shall be enabled only during `FM_PAYLOAD`. | Must | 7 | ⏳ Phase 7 — power rail exists (POWER-BDG-001 §11); software enable pending |
| **PLD-R-002** | The OBC shall provide an SPI1 interface (GPIO10/11/12/13) for payload camera communication at up to 10 MHz. | Must | 7 | ⏳ Phase 7 — requires GPIO10 reassignment from RW3 PWM (PAYLOAD-SPEC-001 OI-3) |
| **PLD-R-003** | The spacecraft shall provide non-volatile storage for at least 1 GB of payload science data. | Must | 7 | ⏳ Phase 7 — external storage TBD (PAYLOAD-SPEC-001 OI-1) |
| **PLD-R-004** | The spacecraft shall support a `FM_PAYLOAD` operational mode enabling scientific instrument operation. The mode shall be reachable from `FM_NOMINAL` via ground command and shall transition to `FM_SAFE` on any `FAULT_LEVEL_CRITICAL` event. | Must | 7 | ⏳ Phase 7 — FMM update required (FMM-DES-001 §5) |
| **PLD-R-005** | The payload mechanical mounting surface shall be capable of dissipating up to 5 W of thermal power to the spacecraft structure. | Should | 7 | ⏳ Phase 7 — thermal analysis pending (PAYLOAD-SPEC-001 OI-6) |

---

## 6. Compliance Matrix

| Standard | Applicable Section | Compliance |
|----------|--------------------|------------|
| ECSS-Q-ST-80C | Software quality & architecture | ✅ Partial |
| MISRA C | Code safety & reliability | ✅ Conventions adopted |
| IEC 61508 | Functional safety foundation | ✅ Static memory, priorities |
| NASA SWE-130 | Software assurance practices | ✅ Modular design, testing |

---

## 7. Requirements Status Summary

| Category | Total | Implemented | In Progress | TBD/Phase 7 | TBC |
|----------|-------|-------------|-------------|-------------|-----|
| Functional | 19 | 9 | 2 | 8 | 0 |
| Non-Functional | 9 | 5 | 0 | 3 | 1 |
| Interface | 10 | 1 | 0 | 9 | 0 |
| Safety | 5 | 4 | 1 | 0 | 0 |
| Payload (PLD-R) | 5 | 0 | 0 | 5 | 0 |
| **Total** | **48** | **19** | **3** | **25** | **1** |
