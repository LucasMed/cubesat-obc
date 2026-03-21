# FMEA-OBC-001 — Hardware Failure Mode and Effects Analysis

| Field           | Value                                   |
|-----------------|----------------------------------------|
| Document ID     | FMEA-OBC-001                           |
| Version         | 1.0                                    |
| Date            | 2026-03-21                             |
| Author          | OBC Systems Team                       |
| Status          | Approved                               |
| Classification  | Internal                               |
| Standard        | ECSS-Q-ST-30-02C                       |

---

## Change History

| Version | Date       | Author           | Description                           |
|---------|------------|------------------|---------------------------------------|
| 1.0     | 2026-03-21 | OBC Systems Team | Initial release — CDR baseline hardware FMEA |

---

## Table of Contents

1. [Introduction](#1-introduction)
2. [Applicable Documents](#2-applicable-documents)
3. [Acronyms and Definitions](#3-acronyms-and-definitions)
4. [Analysis Approach](#4-analysis-approach)
5. [System Boundary and Hardware Scope](#5-system-boundary-and-hardware-scope)
6. [Component FMEA](#6-component-fmea)
   - 6.1 [RP2350 Microcontroller](#61-rp2350-microcontroller)
   - 6.2 [MPU-6050 IMU](#62-mpu-6050-imu)
   - 6.3 [HMC5883L Magnetometer](#63-hmc5883l-magnetometer)
   - 6.4 [NEO-7M GPS Module](#64-neo-7m-gps-module)
   - 6.5 [Flash Memory](#65-flash-memory)
   - 6.6 [Power Subsystem (EPS)](#66-power-subsystem-eps)
7. [Criticality Matrix](#7-criticality-matrix)
8. [Single-Point Failures](#8-single-point-failures)
9. [Mitigation Strategies](#9-mitigation-strategies)
10. [Open Items](#10-open-items)
11. [References](#11-references)

---

## 1. Introduction

### 1.1 Purpose

This document presents the Hardware Failure Mode and Effects Analysis (FMEA) for the
CubeSat OBC flight hardware. It systematically identifies failure modes in each
hardware component, assesses their effects on mission objectives, assigns severity
and criticality ratings, and documents mitigation strategies.

The hardware FMEA complements the software FMEA (docs/ecss/design/FMEA-OBC-001.md)
to provide complete fault coverage for the OBC subsystem.

### 1.2 Scope

This document covers:

- RP2350 microcontroller (Raspberry Pi Pico 2W)
- MPU-6050 IMU (6-DOF accelerometer/gyroscope)
- HMC5883L magnetometer (3-axis magnetometer)
- NEO-7M GPS module
- Flash memory (W25Qxx external + RP2350 internal)
- Electrical Power System (EPS) monitoring

Out of scope: PCB trace failures, radiation effects requiring component-level
lot screening, payload hardware faults.

---

## 2. Applicable Documents

| ID              | Title                                              | Version |
|-----------------|----------------------------------------------------|---------|
| OBC-DES-001     | OBC Hardware & CDH Design Document                 | 0.1     |
| EPS-DES-001     | Electrical Power System Monitor Design Document    | 0.2     |
| ADCS-DES-001    | ADCS Design Document                               | 1.0     |
| BOM-OBC-001     | Bill of Materials                                  | 1.0.1   |
| FMEA-OBC-001    | Software FMEA (docs/ecss/design/FMEA-OBC-001)     | 0.2     |
| FAULT-DES-001   | Fault Manager Design Document                      | 0.2     |
| MRD-OBC-001     | Mission Requirements Document                       | 1.1     |
| ECSS-Q-ST-30-02C | Failure modes, effects (and criticality) analysis | —       |
| ECSS-E-ST-10-03C | Space product assurance — Derivation and validation of EEE components | — |

---

## 3. Acronyms and Definitions

| Term       | Definition                                                    |
|------------|---------------------------------------------------------------|
| FMEA       | Failure Mode and Effects Analysis                             |
| FMECA      | FMEA + Criticality Analysis                                   |
| RPN        | Risk Priority Number = Severity × Occurrence × Detectability  |
| SEU        | Single-Event Upset — bit flip caused by ionizing radiation    |
| SEL        | Single-Event Latchup — high-current state in CMOS devices     |
| TID        | Total Ionizing Dose — cumulative radiation damage             |
| LEO        | Low Earth Orbit                                              |
| COTS       | Commercial Off-The-Shelf                                      |
| I2C        | Inter-Integrated Circuit (bus protocol)                       |
| UART       | Universal Asynchronous Receiver-Transmitter                   |
| ADC        | Analog-to-Digital Converter                                   |
| GPIO       | General-Purpose Input/Output                                  |
| WDT        | Watchdog Timer                                                |
| EEPROM     | Electrically Erasable Programmable Read-Only Memory           |
| NOR Flash  | Non-volatile memory with NOR gate architecture                |
| EPS        | Electrical Power System                                       |
| IMU        | Inertial Measurement Unit                                     |
| GPS        | Global Positioning System                                     |
| MTQ        | Magnetorquer                                                  |
| RW         | Reaction Wheel                                                |

---

## 4. Analysis Approach

### 4.1 Methodology

This FMEA follows the FMECA methodology per ECSS-Q-ST-30-02C. Each hardware
component is analyzed for:

1. **Failure modes**: The specific manner in which a component can fail
2. **Failure causes**: Root causes that lead to each failure mode
3. **Local effects**: Immediate impact on the component and its interfaces
4. **Mission effects**: Impact on mission objectives (MO-1 through MO-5)
5. **Detection methods**: How the failure is identified
6. **Severity**: Impact on mission (1–5 scale)
7. **Occurrence**: Likelihood of occurrence (1–5 scale)
8. **Detectability**: Ability to detect the failure (1–5 scale)
9. **RPN**: Risk Priority Number for prioritization
10. **Mitigation**: Design features or procedures that reduce risk

### 4.2 Scoring Scales

**Severity (S):**

| Score | Level      | Effect on Mission                                      |
|-------|------------|--------------------------------------------------------|
| 5     | CRITICAL   | Mission loss or permanent safe-mode; MO cannot be met |
| 4     | MAJOR      | Significant degradation; primary MO at risk           |
| 3     | MODERATE   | Partial function lost; secondary MO affected           |
| 2     | MINOR      | Isolated anomaly; no MO impact                        |
| 1     | NEGLIGIBLE | Logged only; transparent to operations               |

**Occurrence (O):**

| Score | Description | Approximate Rate                        |
|-------|-------------|----------------------------------------|
| 5     | Frequent    | Multiple times per orbit               |
| 4     | Probable    | Once per day                           |
| 3     | Occasional  | Once per week                          |
| 2     | Remote      | Once per mission year                   |
| 1     | Improbable  | Theoretically possible only             |

**Detectability (D):**

| Score | Description                        | Mechanism                                      |
|-------|------------------------------------|------------------------------------------------|
| 1     | Certain                            | Fault flag set; logged; HK telemetry reflects  |
| 2     | High                               | Fault logged; not propagated to HK            |
| 3     | Moderate                           | Detectable by ground via trend analysis        |
| 4     | Low                                | Detectable only with FM_DIAGNOSTIC telemetry   |
| 5     | Undetectable                       | No observability                               |

**RPN = S × O × D** (range 1–125). Actions required if RPN ≥ 40 or S = 5.

### 4.3 Criticality Number

For each failure mode, criticality is computed as:

**C = β × λ × t**

Where:
- β = fraction of failures that result in the specific failure mode (0–1)
- λ = component failure rate (failures per 10⁶ hours)
- t = mission phase duration (hours)

---

## 5. System Boundary and Hardware Scope

```
┌─────────────────────────────────────────────────────────────────┐
│                     CubeSat OBC Hardware                         │
│                                                                  │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐             │
│  │ RP2350 MCU   │  │  MPU-6050   │  │  HMC5883L   │             │
│  │ (Pico 2W)    │──│  IMU        │  │  Magnetom.  │             │
│  │              │  │ (I2C0)      │  │  (I2C0)     │             │
│  └──────┬──────┘  └─────────────┘  └─────────────┘             │
│         │                                                        │
│  ┌──────┴──────┐  ┌─────────────┐  ┌─────────────┐             │
│  │  Internal    │  │  NEO-7M     │  │  W25Qxx     │             │
│  │  Flash 2MB   │  │  GPS        │  │  Flash      │             │
│  │  (RP2350)    │  │  (UART1)    │  │  (SPI)      │             │
│  └─────────────┘  └─────────────┘  └─────────────┘             │
│         │                                                        │
│  ┌──────┴──────┐                                                │
│  │     EPS     │                                                 │
│  │ (ADC/PWM)   │                                                 │
│  └─────────────┘                                                │
└─────────────────────────────────────────────────────────────────┘
```

### 5.1 Hardware Components Analyzed

| Component         | Part Number        | Interface    | Function                          |
|-------------------|--------------------|--------------|-----------------------------------|
| Microcontroller   | RP2350             | Internal     | OBC processing, FDIR, CDH         |
| IMU               | MPU-6050          | I2C0         | Attitude sensing (gyro/accel)     |
| Magnetometer      | HMC5883L          | I2C0         | Heading reference (3-axis mag)    |
| GPS               | NEO-7M            | UART1        | Position, time, velocity           |
| Internal Flash    | RP2350 QSPI       | Internal     | Firmware storage, data logging    |
| External Flash    | W25Q128           | SPI (future) | Extended storage (Phase 3)        |
| EPS Monitor       | Analog + GPIO     | ADC0/GPIO    | Battery monitoring, rail control   |

---

## 6. Component FMEA

### 6.1 RP2350 Microcontroller

| Item | Description |
|------|-------------|
| Manufacturer | Raspberry Pi Ltd. |
| Type | Dual-core Cortex-M33 ARMv8-M |
| Flash | 2 MB internal QSPI |
| SRAM | 520 KB |
| Clock | 133 MHz |
| Package | QFN-60 (Pico 2W module) |

#### FMEA Table

| ID   | Failure Mode | Failure Cause | Local Effect | Mission Effect | S | O | D | RPN | Detection | Mitigation |
|------|--------------|---------------|--------------|----------------|---|---|---|-----|-----------|------------|
| H-01 | MCU lockup / hang | SEU corruption of program counter, stack overflow, firmware bug | All tasks frozen; no WDT kick | ADCS lost; attitude uncontrolled (H-2); FM_SAFE transition | 5 | 2 | 1 | 10 | TPS3431 WDT timeout (8 s); HK heartbeat loss | Hardware WDT (TPS3431); software stack overflow detection; ECC on critical data |
| H-02 | Flash corruption | Single-event upset during write, power loss during erase | Firmware checksum fails; boot failure | OBC fails to boot; mission loss unless golden image recovery | 5 | 1 | 2 | 10 | Bootloader CRC check | Dual boot partitions; CRC validation; golden image fallback |
| H-03 | SRAM SEU | Single-event upset corrupting variables | Incorrect computation; EKF divergence | Bad attitude estimate; potential actuator saturation; tumble | 4 | 3 | 1 | 12 | EKF divergence check; task health monitoring | ECC on SRAM (if supported); EKF sanity checks; software parity |
| H-04 | I2C bus controller failure | Bus master stuck, clock stretching timeout | Sensors unreadable; DLA data stale | Attitude estimation pauses; H-2 risk | 4 | 2 | 1 | 8 | Software timeout detection; fault raised | Bus reset via GPIO toggle; software retry; fallback to last known values |
| H-05 | UART peripheral failure | UART1 (TT&C) TX/RX stuck or corrupted | Loss of uplink/downlink | Ground contact lost; H-4 | 4 | 2 | 2 | 16 | UART RX timeout; CSP heartbeat loss | CSP retry; FM_SAFE listen-only mode; dual-UART (future) |
| H-06 | ADC peripheral failure | ADC0 (battery sense) stuck or erroneous | Battery voltage unknown | EPS FSM cannot classify energy state | 3 | 1 | 1 | 3 | ADC read timeout; sanity check | Dual-ADC reads; voltage sanity range check; fail-safe EPS state |
| H-07 | PWM peripheral failure | PWM output stuck or incorrect duty cycle | Actuator commands incorrect | RW/MTQ output incorrect; potential uncontrolled torque | 4 | 1 | 2 | 8 | Actuator feedback monitoring; PWM channel check | Hardware enable signal for actuators; PWM fault flag; FM_SAFE disables actuators |
| H-08 | Internal reset | Brown-out, ESD, or latchup triggering reset | Task restart; data loss | Brief mission interruption; boot sequence executed | 3 | 2 | 1 | 6 | Boot log entry; WDT scratch check | External WDT; brown-out detector; power-on reset |
| H-09 | Clock crystal failure | 12 MHz crystal drifts or stops | Timing errors; task period violations | Control loop degradation; telemetry timestamps invalid | 4 | 1 | 2 | 8 | NTP sync (GPS); timing anomaly detection | Internal RC oscillator fallback; GPS time reference |
| H-10 | GPIO pin failure | ESD damage or short circuit | Pin stuck high/low | Depends on pin: sensor comms loss, actuator fault | 3 | 1 | 3 | 9 | Per-pin status monitoring; loopback test | Pin multiplexing; redundant signals where critical |

#### Criticality Analysis (RP2350)

| Failure Mode | λ (×10⁻⁶/h) | β | t (h) | C = β × λ × t |
|--------------|-------------|---|---|----------------|
| MCU lockup | 0.1 | 0.3 | 8760 | 0.26 |
| Flash corruption | 0.05 | 0.1 | 8760 | 0.04 |
| SRAM SEU | 1.0 | 0.2 | 8760 | 1.75 |
| I2C failure | 0.2 | 0.4 | 8760 | 0.70 |
| UART failure | 0.1 | 0.2 | 8760 | 0.18 |
| ADC failure | 0.05 | 0.1 | 8760 | 0.04 |
| PWM failure | 0.1 | 0.2 | 8760 | 0.18 |
| Reset | 0.3 | 0.5 | 8760 | 1.31 |
| Clock failure | 0.02 | 0.1 | 8760 | 0.02 |
| GPIO failure | 0.5 | 0.1 | 8760 | 0.44 |

**Total Criticality (RP2350): C_total = 4.92**

---

### 6.2 MPU-6050 IMU

| Item | Description |
|------|-------------|
| Manufacturer | InvenSense (TDK) |
| Type | 6-DOF IMU (3-axis gyro + 3-axis accelerometer) |
| Interface | I2C (400 kHz Fast Mode) |
| Address | 0x68 |
| Supply | 3.3 V |
| Package | QFN-24 |

#### FMEA Table

| ID   | Failure Mode | Failure Cause | Local Effect | Mission Effect | S | O | D | RPN | Detection | Mitigation |
|------|--------------|---------------|--------------|----------------|---|---|---|-----|-----------|------------|
| H-11 | Total I2C failure | Bus stuck, device not responding | IMU unreadable | Attitude estimate degrades; gyro data stale | 4 | 2 | 1 | 8 | I2C timeout; DLA staleness check | I2C retry ×3; bus reset; fallback to last valid data |
| H-12 | Gyroscope failure | MEMS element damaged, electronics fault | Gyro outputs zero or saturated | Angular rate unknown; attitude drift | 4 | 1 | 2 | 8 | Sanity check (non-zero variance); HK status | Accelerometer fallback for detumble; FM_DETUMBLE using accel only |
| H-13 | Accelerometer failure | MEMS element damaged, electronics fault | Accel outputs zero or saturated | Specific force unknown; EKF degraded | 3 | 1 | 2 | 6 | Sanity check; HK status | Gyro-only mode; magnetometer for yaw reference |
| H-14 | Temperature sensor failure | Internal temp sensor fault | Temperature readout invalid | Thermal compensation disabled | 2 | 1 | 2 | 4 | Range check; HK | Use nominal temperature; log anomaly |
| H-15 | I2C data corruption | Noise, ESD, bus contention | Incorrect sensor values | Erroneous attitude estimate | 3 | 2 | 1 | 6 | CRC if implemented; sanity check on values | Retries; plausibility bounds check; fault raised |
| H-16 | Power supply dropout | 3.3 V rail glitch | IMU reset; initialization required | Brief data gap; re-init needed | 2 | 2 | 1 | 4 | Power monitoring; I2C NACK | Local decoupling capacitor; voltage supervisor |
| H-17 | Mechanical decoupling | Vibration-induced connection loss | Intermittent readings | Attitude estimate jittery | 3 | 1 | 3 | 9 | Statistical outlier detection | Vibration mounting; potting compound |
| H-18 | Calibration drift | Temperature cycle, aging | Bias in measurements | Attitude drift over time | 3 | 2 | 3 | 18 | Ground calibration check; HK bias telemetry | Periodic ground recalibration; temperature compensation |

#### Criticality Analysis (MPU-6050)

| Failure Mode | λ (×10⁻⁶/h) | β | t (h) | C = β × λ × t |
|--------------|-------------|---|---|----------------|
| I2C failure | 0.5 | 0.3 | 8760 | 1.31 |
| Gyro failure | 0.3 | 0.2 | 8760 | 0.53 |
| Accel failure | 0.3 | 0.2 | 8760 | 0.53 |
| Temp sensor | 0.1 | 0.1 | 8760 | 0.09 |
| Data corruption | 0.2 | 0.2 | 8760 | 0.35 |
| Power dropout | 0.3 | 0.3 | 8760 | 0.79 |
| Mechanical | 0.1 | 0.2 | 8760 | 0.18 |
| Calibration drift | 0.5 | 0.4 | 8760 | 1.75 |

**Total Criticality (MPU-6050): C_total = 5.53**

---

### 6.3 HMC5883L Magnetometer

| Item | Description |
|------|-------------|
| Manufacturer | Honeywell |
| Type | 3-axis digital magnetometer |
| Interface | I2C (400 kHz) |
| Address | 0x1E |
| Supply | 3.3 V |
| Range | ±8 Gauss |

#### FMEA Table

| ID   | Failure Mode | Failure Cause | Local Effect | Mission Effect | S | O | D | RPN | Detection | Mitigation |
|------|--------------|---------------|--------------|----------------|---|---|---|-----|-----------|------------|
| H-21 | I2C communication failure | Bus fault, device unresponsive | Magnetometer unreadable | Yaw reference lost; attitude drift | 3 | 2 | 1 | 6 | I2C timeout; fault raised | Retry logic; fallback to gyro-only yaw propagation |
| H-22 | Magnetic sensor saturation | Strong local magnetic field (degaussing coils, motors) | Outputs saturate; heading invalid | Yaw estimate invalid; potential tumble | 4 | 2 | 2 | 16 | Range check; saturation flag detection | MTQ desaturation; FM_DETUMBLE fallback; ground command |
| H-23 | Sensor offset drift | Temperature change, magnetic hysteresis | Bias in heading | Slow yaw drift | 2 | 3 | 3 | 18 | Ground calibration pass; HK telemetry | Periodic ground calibration; temperature model |
| H-24 | ADC saturation | Overrange magnetic field | Readings clamped at max/min | Heading jumps | 3 | 2 | 2 | 12 | Saturation flag in status register | Magnetic cleanliness program; MTQ current limiting |
| H-25 | Power supply noise | Switching noise coupling | Erratic readings | Attitude estimate noisy | 2 | 3 | 2 | 12 | Statistical filter; outlier rejection | Separate power domain; LC filtering; grounding |
| H-26 | Degaassing failure | DRDY pin stuck or register error | Magnetometer sensitivity degraded | Calibration ineffective | 3 | 1 | 3 | 9 | Self-test register check; HK status | Ground commanded degauss; replacement calibration |

#### Criticality Analysis (HMC5883L)

| Failure Mode | λ (×10⁻⁶/h) | β | t (h) | C = β × λ × t |
|--------------|-------------|---|---|----------------|
| I2C failure | 0.5 | 0.3 | 8760 | 1.31 |
| Saturation | 0.2 | 0.3 | 8760 | 0.53 |
| Offset drift | 0.3 | 0.4 | 8760 | 1.05 |
| ADC saturation | 0.1 | 0.2 | 8760 | 0.18 |
| Power noise | 0.2 | 0.3 | 8760 | 0.53 |
| Degaussing | 0.1 | 0.2 | 8760 | 0.18 |

**Total Criticality (HMC5883L): C_total = 3.78**

---

### 6.4 NEO-7M GPS Module

| Item | Description |
|------|-------------|
| Manufacturer | u-blox |
| Type | GNSS receiver (GPS, GLONASS, BeiDou) |
| Interface | UART1 (115200 baud) |
| Supply | 3.3 V |
| Antenna | External passive patch |

#### FMEA Table

| ID   | Failure Mode | Failure Cause | Local Effect | Mission Effect | S | O | D | RPN | Detection | Mitigation |
|------|--------------|---------------|--------------|----------------|---|---|---|-----|-----------|------------|
| H-31 | No satellite fix | Antenna shadowing, RF interference, weak signal | Position/velocity unknown | Orbit determination lost; timing unavailable | 3 | 3 | 1 | 9 | Fix timeout; HK status | Ground orbit propagation; TLE updates; antenna positioning |
| H-32 | UART communication loss | TX/RX failure, protocol error | GPS data unavailable | Orbit state stale | 3 | 2 | 1 | 6 | UART timeout; checksum error | Retry; FM_NOMINAL continues without GPS; ground track update |
| H-33 | Time pulse failure | Internal PLL fault | 1PPS signal incorrect | Time reference error | 2 | 1 | 3 | 6 | 1PPS monitoring; HK status | Fallback to ground time; NTP from comms |
| H-34 | Ephemeris data corruption | Bit errors in satellite data | Position solution invalid | Erroneous position reported | 3 | 2 | 2 | 12 | CRC/checksum on ephemeris; position plausibility | Dual-GNSS constellation; ground filter |
| H-35 | Antenna failure | Open/short, damage | No RF signal | Complete GPS loss | 4 | 1 | 3 | 12 | Antenna sense check; signal strength monitoring | Ground-based tracking fallback; TLE propagation |
| H-36 | Power supply dropout | 3.3 V rail glitch | Module reset; cold start | TTFF delay (30-60 s) | 2 | 2 | 1 | 4 | Power monitor; UART activity check | Local decoupling; voltage supervisor |
| H-37 | NMEA message corruption | Noise on UART | Parsing errors; stale data | Position/velocity jumps | 2 | 3 | 1 | 6 | Checksum validation; message timeout | Message rejection; hold last valid |

#### Criticality Analysis (NEO-7M)

| Failure Mode | λ (×10⁻⁶/h) | β | t (h) | C = β × λ × t |
|--------------|-------------|---|---|----------------|
| No fix | 2.0 | 0.4 | 8760 | 7.01 |
| UART loss | 0.3 | 0.3 | 8760 | 0.79 |
| Time pulse fail | 0.1 | 0.1 | 8760 | 0.09 |
| Ephemeris corrupt | 0.5 | 0.2 | 8760 | 0.88 |
| Antenna failure | 0.1 | 0.1 | 8760 | 0.09 |
| Power dropout | 0.3 | 0.3 | 8760 | 0.79 |
| NMEA corruption | 1.0 | 0.5 | 8760 | 4.38 |

**Total Criticality (NEO-7M): C_total = 14.03**

---

### 6.5 Flash Memory

#### 6.5.1 RP2350 Internal Flash (2 MB)

| Item | Description |
|------|-------------|
| Type | QSPI NOR Flash |
| Size | 2 MB |
| Endurance | 100,000 program/erase cycles |
| Retention | 20 years |

#### FMEA Table

| ID   | Failure Mode | Failure Cause | Local Effect | Mission Effect | S | O | D | RPN | Detection | Mitigation |
|------|--------------|---------------|--------------|----------------|---|---|---|-----|-----------|------------|
| H-41 | Write failure | Voltage dropout during write, wear-out | Write fails; data not saved | Event log incomplete; parameter loss | 3 | 1 | 2 | 6 | Write return code; CRC check | Write verification; wear leveling; backup sectors |
| H-42 | Erase failure | Sector wear-out, oxide breakdown | Sector unusable | Loss of parameter storage | 2 | 1 | 3 | 6 | Erase verify fail | Spare sectors; wear-leveling algorithm |
| H-43 | Read failure | Bit errors, ECC failure | Corrupted data returned | Incorrect parameter values | 3 | 1 | 2 | 6 | CRC mismatch | ECC; redundant storage; fallback values |
| H-44 | SEU during read | Single-event upset | Random bit flips | Data corruption | 3 | 2 | 2 | 12 | CRC check on read | ECC; checksums; error detection |
| H-45 | Sector lockup | Write/erase algorithm stuck | Sector locked | Loss of storage region | 2 | 1 | 3 | 6 | Timeout detection | Watchdog; reset flash controller |
| H-46 | Boot sector corruption | SEU during boot, power loss | Boot failure | OBC cannot start | 5 | 1 | 2 | 10 | CRC check on boot | Dual boot partitions; golden image |

#### 6.5.2 W25Q128 External Flash (Future Phase 3)

| Item | Description |
|------|-------------|
| Type | SPI NOR Flash |
| Size | 16 MB |
| Interface | SPI |
| Endurance | 100,000 cycles per sector |

| ID   | Failure Mode | Failure Cause | Local Effect | Mission Effect | S | O | D | RPN | Detection | Mitigation |
|------|--------------|---------------|--------------|----------------|---|---|---|-----|-----------|------------|
| H-51 | SPI communication failure | Bus fault, chip select issue | Flash unreadable | Log data unavailable | 2 | 2 | 1 | 4 | SPI error; timeout | Retry; fallback to internal flash |
| H-52 | Write failure | Wear-out, voltage dropout | Write fails | Data loss | 3 | 1 | 2 | 6 | Write verify | Wear leveling; error correction |
| H-53 | Data corruption | SEU, read distur | Corrupted data | Incorrect telemetry replay | 2 | 2 | 2 | 8 | CRC check | ECC; redundant sectors |

---

### 6.6 Power Subsystem (EPS)

| Item | Description |
|------|-------------|
| Battery | 2S Li-ion (7.4 V nominal) |
| Capacity | 3000 mAh |
| Monitoring | ADC0 (GPIO26) via resistor divider |
| Voltage sense | R1=330 kΩ, R2=100 kΩ |

#### FMEA Table

| ID   | Failure Mode | Failure Cause | Local Effect | Mission Effect | S | O | D | RPN | Detection | Mitigation |
|------|--------------|---------------|--------------|----------------|---|---|---|-----|-----------|------------|
| H-61 | Battery voltage sense failure | ADC corruption, resistor divider open | V_batt reading invalid | Energy state unknown; FM_SAFE entered conservatively | 4 | 1 | 1 | 4 | ADC range check; sanity bounds | Dual voltage sensing; fail-safe FM_SAFE |
| H-62 | Resistor divider drift | Temperature, tolerance | Voltage reading offset | Incorrect energy state threshold crossing | 3 | 2 | 2 | 12 | Periodic calibration; trend monitoring | Precision resistors; temperature compensation |
| H-63 | Overcurrent protection trip | Load fault, short circuit | Rail disabled | Subsystem lost | 4 | 2 | 1 | 8 | INA219 alert; HK current telemetry | Current limiting; selective shutdown; FM_SAFE |
| H-64 | 3.3 V regulator failure | LDO failure, thermal shutdown | All 3.3 V loads lost | Complete OBC failure; WDT reset | 5 | 1 | 2 | 10 | Voltage monitor; brown-out detect | Redundant regulators (future); OBC always-on rail |
| H-65 | Battery depleted | Solar panel failure, high load | V_batt drops below thresholds | FM_SAFE → payload off; comms reduced | 5 | 2 | 1 | 10 | Battery voltage monitoring | Battery sizing; load shedding; survival mode |
| H-66 | Charging circuit failure | TP4056 fault | Battery not charging | Gradual battery depletion | 4 | 1 | 2 | 8 | Charge current monitoring; HK | Battery capacity buffer; ground commanded safe mode |
| H-67 | Solar panel open circuit | Connector failure, diode failure | No solar charging | Battery depletion accelerated | 4 | 2 | 2 | 16 | Voltage/current monitoring | Battery sizing for eclipse survival; ground alert |
| H-68 | Temperature sensor failure | NTC open/short | Battery temperature unknown | Charging disabled (thermal protection) | 3 | 1 | 2 | 6 | Range check; HK status | Default to safe charging limits; ground alert |
| H-69 | Power rail noise | Switching noise, ground loops | Sensor readings jittery | Data quality degraded | 2 | 3 | 2 | 12 | Signal quality monitoring; filtering | LC filtering; separate analog ground; decoupling |

#### Criticality Analysis (EPS)

| Failure Mode | λ (×10⁻⁶/h) | β | t (h) | C = β × λ × t |
|--------------|-------------|---|---|----------------|
| V_sense fail | 0.1 | 0.2 | 8760 | 0.18 |
| Resistor drift | 0.1 | 0.3 | 8760 | 0.26 |
| Overcurrent | 0.5 | 0.3 | 8760 | 1.31 |
| 3.3V reg fail | 0.1 | 0.1 | 8760 | 0.09 |
| Battery depleted | 0.3 | 0.2 | 8760 | 0.53 |
| Charge fail | 0.2 | 0.2 | 8760 | 0.35 |
| Solar open | 0.2 | 0.2 | 8760 | 0.35 |
| Temp sensor | 0.1 | 0.2 | 8760 | 0.18 |
| Rail noise | 1.0 | 0.5 | 8760 | 4.38 |

**Total Criticality (EPS): C_total = 7.63**

---

## 7. Criticality Matrix

### 7.1 Component Criticality Summary

| Component | Criticality (C) | Rank | Highest RPN |
|-----------|----------------|------|------------|
| NEO-7M GPS | 14.03 | 1 | 12 |
| EPS | 7.63 | 2 | 16 |
| MPU-6050 IMU | 5.53 | 3 | 18 |
| RP2350 MCU | 4.92 | 4 | 12 |
| HMC5883L Magnetometer | 3.78 | 5 | 18 |
| Flash Memory | 2.10 | 6 | 10 |

### 7.2 Risk Priority Number Summary

| Priority | ID | Component | Failure Mode | RPN | S | Action |
|----------|----|-----------|--------------|-----|---|--------|
| 1 | H-18 | MPU-6050 | Calibration drift | 18 | 3 | Ground recalibration schedule |
| 1 | H-23 | HMC5883L | Sensor offset drift | 18 | 2 | Ground calibration procedure |
| 2 | H-67 | EPS | Solar panel open circuit | 16 | 4 | Battery sizing; ground monitoring |
| 2 | H-05 | RP2350 | UART peripheral failure | 16 | 4 | CSP retry; FM_SAFE mode |
| 2 | H-22 | HMC5883L | Magnetic saturation | 16 | 4 | MTQ desat; FM_DETUMBLE fallback |
| 3 | H-69 | EPS | Power rail noise | 12 | 2 | LC filtering; decoupling |
| 3 | H-34 | NEO-7M | Ephemeris corruption | 12 | 3 | Ground filter; dual-GNSS |
| 3 | H-35 | NEO-7M | Antenna failure | 12 | 4 | Ground tracking fallback |
| 3 | H-62 | EPS | Resistor divider drift | 12 | 3 | Precision components; temp comp |
| 3 | H-44 | Flash | SEU during read | 12 | 3 | ECC; checksums |
| 4 | H-01 | RP2350 | MCU lockup/hang | 10 | 5 | WDT; stack overflow detection |
| 4 | H-02 | RP2350 | Flash corruption | 10 | 5 | Dual boot; CRC; golden image |
| 4 | H-31 | NEO-7M | No satellite fix | 9 | 3 | Ground orbit propagation |
| 4 | H-10 | RP2350 | GPIO pin failure | 9 | 3 | Pin multiplexing; redundant signals |
| 4 | H-26 | HMC5883L | Degaussing failure | 9 | 3 | Ground commanded degauss |
| 4 | H-17 | MPU-6050 | Mechanical decoupling | 9 | 3 | Vibration mounting; potting |

---

## 8. Single-Point Failures

A Single-Point Failure (SPF) is a failure that alone causes mission loss with no
recovery path.

| Component | Failure Mode | SPF? | Justification | Mitigation |
|-----------|--------------|------|----------------|------------|
| RP2350 MCU | Complete failure (latchup, destruction) | **Yes** | No redundant MCU | Redundant MCU (future Phase 4); OBC always-on rail protected |
| 3.3V Regulator | Complete failure | **Yes** | All 3.3V loads lost | Redundant regulator (future); voltage monitoring |
| Battery | Complete failure | **Yes** | No power source | Battery redundancy; solar-only survival mode |
| Battery connector | Open circuit | **Yes** | Immediate power loss | Connector strain relief; conformal coating |
| Flash (boot) | Boot sector corruption | **Yes** | Cannot boot without recovery | Dual boot partitions; golden image in ROM |
| Solar panel | Complete failure | **Conditional** | Mission continues on battery | Battery sizing for full mission duration |
| GPS antenna | Complete failure | No | Ground tracking fallback | TLE propagation; ground station tracking |

### SPF Risk Assessment

| SPF | Probability | Mission Impact | Mitigation Status |
|-----|-------------|----------------|-------------------|
| RP2350 MCU failure | Very Low | Total mission loss | No mitigation (COTS limitation) |
| 3.3V regulator failure | Low | Total mission loss | Voltage monitoring only |
| Battery failure | Very Low | Gradual mission loss | Battery health monitoring |
| Solar panel failure | Low | Accelerated depletion | Ground monitoring; load shedding |
| Boot corruption | Very Low | OBC cannot boot | Dual partition (planned) |

---

## 9. Mitigation Strategies

### 9.1 Design Mitigations

| ID | Mitigation | Component(s) | Implementation | Status |
|----|-----------|-------------|----------------|--------|
| M-H01 | Hardware watchdog (TPS3431) | RP2350 | External WDT; GPIO20 kick every 5 s | Implemented |
| M-H02 | Dual boot partitions | RP2350 Flash | Primary + backup partition; CRC validation | Planned Phase 3 |
| M-H03 | Stack overflow detection | RP2350 | FreeRTOS configCHECK_FOR_STACK_OVERFLOW=2 | Implemented |
| M-H04 | I2C bus reset | MPU-6050, HMC5883L | GPIO toggle reset; retry ×3 | Implemented |
| M-H05 | EKF divergence check | MPU-6050 | Quaternion norm check; covariance limit | Implemented |
| M-H06 | FM_DETUMBLE fallback | MPU-6050 | B-dot using magnetometer if gyro fails | Implemented |
| M-H07 | Battery voltage hysteresis | EPS | Schmidt trigger (7.4/7.0/6.6 V thresholds) | Implemented |
| M-H08 | Load shedding | EPS | Automatic based on energy state | Planned Phase 2 |
| M-H09 | GPS timeout fallback | NEO-7M | 60 s fix timeout; ground propagation | Implemented |
| M-H10 | Flash ECC | RP2350, W25Qxx | Error correction on critical data | Planned Phase 3 |
| M-H11 | Voltage supervisors | All | Brown-out detect; power-on reset | Implemented |
| M-H12 | Decoupling capacitors | All ICs | Local 100 nF + bulk caps | PCB design |
| M-H13 | Current monitoring | EPS | INA219 for overcurrent detection | Planned Phase 2 |
| M-H14 | Temperature compensation | MPU-6050, HMC5883L | On-chip temp sensor; calibration tables | Planned Phase 2 |
| M-H15 | Redundant sensors | IMU, Mag | Software voter for sensor data | Planned Phase 3 |

### 9.2 Procedural Mitigations

| ID | Mitigation | Description | Schedule |
|----|-----------|-------------|----------|
| P-H01 | Ground calibration | IMU and magnetometer calibration | Pre-flight; monthly |
| P-H02 | Battery health check | Voltage/current trend analysis | Weekly ground pass |
| P-H03 | Software CRC verification | Verify firmware integrity | Every boot |
| P-H04 | TLE update | Orbital element set update | Daily |
| P-H05 | Magnetic cleanliness test | Verify stray fields | Pre-flight environmental test |
| P-H06 | Radiation testing | SEE/TID characterization | Component qualification |

---

## 10. Open Items

| OI  | Description | Priority | Owner | Target Date |
|-----|-------------|----------|-------|------------|
| OI-1 | Redundant MCU not implemented — single RP2350 is SPF for total mission loss | High | Hardware Lead | Phase 4 |
| OI-2 | Dual boot partition not implemented — boot corruption is SPF | High | Software Lead | Phase 3 |
| OI-3 | INA219 overcurrent monitoring not implemented — EPS overcurrent detection limited | High | Software Lead | Phase 2 |
| OI-4 | GPS antenna radiation testing not completed — COTS antenna not space-qualified | Medium | Hardware Lead | Pre-flight |
| OI-5 | SEE characterization for RP2350 not completed — SEU rate in LEO unknown | Medium | Systems Engineer | Phase 2 |
| OI-6 | Flash ECC not implemented — data integrity relies on CRC only | Medium | Software Lead | Phase 3 |
| OI-7 | Temperature compensation tables for sensors not populated | Medium | ADCS Lead | Phase 2 |
| OI-8 | Redundant voltage sensing for EPS not implemented | Low | Hardware Lead | Phase 3 |
| OI-9 | 3.3V redundant regulator not implemented | Low | Hardware Lead | Phase 4 |
| OI-10 | External flash W25Q128 integration deferred to Phase 3 | Low | Software Lead | Phase 3 |

---

## 11. References

| Ref | Document |
|-----|----------|
| [1] | RP2350 Datasheet — Raspberry Pi RP2350 Technical Reference Manual |
| [2] | MPU-6050 Datasheet — InvenSense 6-Axis MEMS MotionTracking Device |
| [3] | HMC5883L Datasheet — Honeywell 3-Axis Digital Compass |
| [4] | NEO-7M Datasheet — u-blox 7 GPS/GNSS Modules |
| [5] | W25Q128 Datasheet — Winbond 128M-bit Serial Flash Memory |
| [6] | ECSS-Q-ST-30-02C — Failure modes, effects (and criticality) analysis |
| [7] | ECSS-E-ST-10-03C — Derivation and validation of EEE components |
| [8] | OBC-DES-001 v0.1 — OBC Hardware & CDH Design Document |
| [9] | EPS-DES-001 v0.2 — Electrical Power System Monitor Design Document |
| [10] | BOM-OBC-001 v1.0.1 — Bill of Materials |
| [11] | FAULT-DES-001 v0.2 — Fault Manager Design Document |
| [12] | MRD-OBC-001 v1.1 — Mission Requirements Document |

---

(End of file — FMEA-OBC-001 Hardware FMEA v1.0)
