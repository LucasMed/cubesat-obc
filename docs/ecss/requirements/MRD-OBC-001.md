# MRD-OBC-001 — Mission Requirements Document / Concept of Operations

| Field            | Value                                             |
|------------------|---------------------------------------------------|
| **Document ID**  | MRD-OBC-001                                       |
| **Title**        | Mission Requirements Document / Concept of Operations |
| **Project**      | CubeSat OBC — RP2350 / Pico 2W                    |
| **Version**      | 1.0                                               |
| **Status**       | Approved — MRR Baseline                           |
| **Date**         | 2026-03-07                                        |
| **Author**       | OBC Systems Team                                  |
| **Review Level** | MRR                                               |
| **Standard**     | ECSS-E-ST-10-06C (Technical Requirements), ECSS-M-ST-10C |

---

## Change History

| Version | Date       | Author          | Description              |
|---------|------------|-----------------|--------------------------|
| 1.0     | 2026-03-07 | OBC Systems Team | Initial MRR baseline     |

---

## Table of Contents

1. [Introduction](#1-introduction)
2. [Mission Overview](#2-mission-overview)
3. [Stakeholders and Operational Concept](#3-stakeholders-and-operational-concept)
4. [Orbital Parameters](#4-orbital-parameters)
5. [Mission Phases and Modes](#5-mission-phases-and-modes)
6. [Mission Requirements](#6-mission-requirements)
7. [Constraints and Assumptions](#7-constraints-and-assumptions)
8. [Interface Requirements Summary](#8-interface-requirements-summary)
9. [Verification Approach](#9-verification-approach)
10. [Open Items](#10-open-items)

---

## 1. Introduction

### 1.1 Purpose

This document establishes the mission-level requirements and Concept of Operations
(ConOps) for the CubeSat OBC project. It defines what the system must accomplish,
the operational environment, the mission phases, and the top-level constraints
that drive all downstream requirements in `SyRS-OBC-001` and `SRS-OBC-001`.

### 1.2 Scope

This document covers:
- Mission objectives and success criteria
- Orbital environment and parameters
- Operational concept and ground segment interactions
- Top-level mission requirements (MIS-xxx)

Hardware design details are in `BOM-OBC-001`. Software architecture is in
`SAD-OBC-001`. Detailed system requirements are in `SyRS-OBC-001`.

### 1.3 Document Identifier

`MRD-OBC-001 v1.0`

---

## 2. Mission Overview

### 2.1 Mission Statement

> Design, implement, and validate a professional-grade On-Board Computer (OBC)
> flight software for a 1U CubeSat, demonstrating full 3-axis attitude
> determination and control (ADCS), autonomous fault management, bidirectional
> TT&C, and energy-aware subsystem management — all compliant with ECSS software
> standards.

### 2.2 Mission Type

| Attribute        | Value                                                      |
|------------------|------------------------------------------------------------|
| Platform         | 1U CubeSat (100 × 100 × 113 mm, ≤ 1.33 kg)               |
| MCU              | Raspberry Pi Pico 2W (RP2350, dual-core Cortex-M33, 133 MHz) |
| RTOS             | FreeRTOS with `ARM_CM33_NTZ` port                          |
| Mission class    | Technology Demonstration / Educational                     |
| Primary discipline | Guidance, Navigation & Control (GN&C); Flight Software   |

### 2.3 Mission Objectives

| ID    | Objective | Priority | Success Criterion |
|-------|-----------|----------|-------------------|
| MO-1  | Demonstrate 3-axis attitude determination via EKF (gyro + accel + magnetometer) | Primary | EKF converges in < 60 s; attitude error < 5° RMS in FM_NOMINAL |
| MO-2  | Demonstrate 3-axis attitude control — B-dot detumbling (MTQ) and LQR precision pointing (RW) | Primary | FM_DETUMBLE reduces angular rate to < 2 °/s; FM_NOMINAL holds pointing to < 1° |
| MO-3  | Validate autonomous FDIR: fault detection → FM_SAFE transition without ground command | Primary | FAULT_LEVEL_CRITICAL event triggers FM_SAFE within 1 orbit |
| MO-4  | Demonstrate bidirectional TT&C over 433 MHz LoRa link (E22-400M30S) | Primary | HK telemetry received on ground; uplink command executed within 1 pass |
| MO-5  | Validate energy-aware subsystem management (EPS Schmidt-trigger, rail shedding) | Primary | Load shedding activates within 5 s of ENERGY_CRITICAL; OBC rail never interrupted |
| MO-6  | Validate persistent event logging across power cycles (flash ring buffer) | Secondary | ≥ 320 events stored; Class A events survive reset |
| MO-7  | Achieve ECSS-Q-ST-80C software standards compliance (MISRA C, traceability, test coverage ≥ 90%) | Secondary | 0 MISRA required/mandatory violations; line coverage ≥ 90% |

### 2.4 Mission Lifetime

| Phase       | Duration          | Description                                 |
|-------------|-------------------|---------------------------------------------|
| Development | 2026-02 – 2026-06 | SW development, unit & integration tests    |
| Qualification | 2026-07 – 2026-09 | Environmental testing, EMC, vibration      |
| Launch readiness | 2026-10        | Flight image build, final PDR/CDR/TRR      |
| On-orbit    | ≥ 12 months       | Nominal operations; target 24 months       |

---

## 3. Stakeholders and Operational Concept

### 3.1 Stakeholders

| Stakeholder | Role | Key Concern |
|-------------|------|-------------|
| OBC Development Team | Designer / Implementer | Architecture correctness, standards compliance |
| Ground Station Operator | Mission Control | Telemetry reception, command uplink, anomaly response |
| Mission Manager | Overall authority | Schedule, success criteria, risk |
| Payload Operator | Secondary user | Payload power enable/disable, data downlink |

### 3.2 Operational Concept (ConOps)

#### 3.2.1 Launch and Early Operations Phase (LEOP)

1. Separation from launch vehicle.
2. Deployment inhibit release (30-minute timer or GS command).
3. OBC boot → `FM_BOOT` → antenna deployment.
4. B-dot detumbling (`FM_DETUMBLE`) using magnetorquers until ω < 2 °/s.
5. EPS initial energy assessment; if Vbatt ≥ 7.4 V → proceed.
6. Transition to `FM_SAFE` for first ground contact.

#### 3.2.2 Nominal Operations

1. Ground station acquires satellite (contact window: 10–14 min, 2–4 passes/day).
2. HK telemetry downlinked automatically at 1 Hz during contact.
3. Ground operator sends telecommands (mode change, payload enable, parameter update).
4. OBC transitions to `FM_NOMINAL`; LQR attitude control active.
5. Payload rail enabled; data collection active.
6. Eclipse period: OBC continues in FM_NOMINAL; EPS monitors Vbatt.

#### 3.2.3 Autonomous FDIR Sequence

```
Fault detected (Fault Manager)
        │
        ├── FAULT_LEVEL_WARNING → log event, no mode change
        │
        ├── FAULT_LEVEL_ERROR   → log event, raise alarm in HK
        │
        └── FAULT_LEVEL_CRITICAL → fmm_force_safe() → FM_SAFE
                  │
                  └── HK flag set → ground operator notified on next pass
```

#### 3.2.4 Ground Segment

| Component | Description |
|-----------|-------------|
| Ground radio | EBYTE E22-400M30S (433 MHz LoRa, 30 dBm) + USB-UART adapter |
| Ground software | Host application receiving binary CSP packets over KISS framing |
| Command protocol | CSP port 20 (command), port 10 (telemetry) |
| Link budget margin | +8.5 dB at 2300 km slant range (horizon pass) |

---

## 4. Orbital Parameters

Reference: `BOM-OBC-001 §0`.

| Parameter | Value | Rationale |
|-----------|-------|-----------|
| Orbit type | Sun-Synchronous (SSO) | Fixed local solar time; maximizes solar panel illumination repeatability |
| Altitude | 500–700 km (nominal: 600 km) | Low LEO; accessible for amateur-band radio |
| Inclination | ~96°–98° | Required for SSO at this altitude |
| Orbital period | 94.5 min (500 km) – 98.6 min (700 km) | $T = 2\pi\sqrt{a^3/\mu}$ |
| Max eclipse duration | 37 min (worst case, $\beta = 0°$) | Drives EPS battery sizing |
| Min eclipse duration | 0 min (continuous sunlight, $|\beta| > 66°$) | — |
| Contact window (GS) | 10–14 min/pass, 2–4 passes/day | Depends on GS latitude and min elevation |
| Critical slant range | ~2000–2300 km (5° elevation, horizon) | Drives link budget worst case |
| Orbital drift | ~0.98°/day (SSO precession) | — |

---

## 5. Mission Phases and Modes

### 5.1 Mission Phases

| Phase | Duration | OBC Flight Modes Active |
|-------|----------|-------------------------|
| Pre-launch (ground) | Until launch | —  |
| LEOP | 0 – 24 h post-separation | FM_BOOT → FM_DETUMBLE → FM_SAFE |
| Commissioning | 24–72 h | FM_SAFE → FM_NOMINAL (first commands) |
| Nominal operations | Days 3 – end-of-life | FM_NOMINAL ↔ FM_DETUMBLE (as needed) |
| Contingency / safe hold | On demand | FM_SAFE |
| End of life | After ≥ 12 months | Passivization sequence (Phase 2) |

### 5.2 Flight Modes Summary

Full flight mode design is in `FMM-DES-001`.

| Mode | Description | Entry |
|------|-------------|-------|
| FM_BOOT (0) | Post-reset initialization; no actuator output | Power-on / reset |
| FM_SAFE (1) | Minimal operations; TT&C listen only | FDIR critical fault; ground command |
| FM_DETUMBLE (2) | B-dot MTQ control; reduce angular rate | Ground command; LEOP |
| FM_NOMINAL (3) | LQR full ADCS; payload enabled; full TT&C | Ground command when stable |
| FM_DIAGNOSTIC (4) | PID control; extended telemetry; test mode | Ground command |

---

## 6. Mission Requirements

### 6.1 Performance Requirements

| Req ID | Requirement | Priority | Verification |
|--------|-------------|----------|--------------|
| MIS-P-001 | The OBC shall perform 3-axis attitude determination with accuracy ≤ 5° RMS in FM_NOMINAL | Mandatory | Test (closed-loop simulation) |
| MIS-P-002 | The OBC shall damp initial tumble to angular rate ≤ 2 °/s within 20 minutes in FM_DETUMBLE | Mandatory | Test (simulation) |
| MIS-P-003 | The OBC shall maintain nadir pointing to ≤ 1° accuracy in FM_NOMINAL with LQR control | Desirable | Test (simulation) |
| MIS-P-004 | The attitude estimation pipeline shall execute at ≥ 10 Hz | Mandatory | Test (task timing measurement) |
| MIS-P-005 | The telemetry task shall transmit HK packets at 1 Hz in FM_NOMINAL | Mandatory | Test |

### 6.2 Dependability Requirements

| Req ID | Requirement | Priority | Verification |
|--------|-------------|----------|--------------|
| MIS-D-001 | The OBC shall autonomously detect and respond to FAULT_LEVEL_CRITICAL faults by entering FM_SAFE within one OBC execution cycle | Mandatory | Test |
| MIS-D-002 | The OBC shall survive a watchdog reset and recover to FM_SAFE within 10 s | Mandatory | Test |
| MIS-D-003 | The OBC shall log all FAULT_LEVEL_CRITICAL events to non-volatile flash before FM_SAFE entry | Mandatory | Test |
| MIS-D-004 | The OBC shall maintain operation at battery voltages ≥ 6.6 V (ENERGY_CRITICAL) with non-essential loads shed | Mandatory | Test (EPS monitor tick with injected voltage) |
| MIS-D-005 | The OBC rail shall not be interruptive by software | Mandatory | Inspection (code review) |

### 6.3 Communication Requirements

| Req ID | Requirement | Priority | Verification |
|--------|-------------|----------|--------------|
| MIS-C-001 | The OBC shall downlink HK telemetry using CSP over 433 MHz LoRa (KISS framing) | Mandatory | Test |
| MIS-C-002 | The OBC shall receive and execute ground commands on CSP port 20 | Mandatory | Test |
| MIS-C-003 | The TT&C link shall provide ≥ +8 dB margin at 2300 km slant range | Mandatory | Analysis (link budget in BOM-OBC-001 §6) |
| MIS-C-004 | The OBC shall be reachable for command in FM_SAFE (listen-only mode) | Mandatory | Test |

### 6.4 Environmental Requirements

| Req ID | Requirement | Priority | Verification |
|--------|-------------|----------|--------------|
| MIS-E-001 | The OBC hardware shall operate across the LEO thermal environment: –20°C to +60°C operational | Mandatory | Analysis / Qualification test |
| MIS-E-002 | The OBC shall withstand launch vibration loads (GEVS random vibration, ≥ 14.1 g$_{rms}$) | Mandatory | Qualification test |
| MIS-E-003 | The OBC software shall be tolerant of single-bit upsets (SEU) in SRAM via watchdog recovery | Desirable | Analysis |

### 6.5 Operational Requirements

| Req ID | Requirement | Priority | Verification |
|--------|-------------|----------|--------------|
| MIS-O-001 | The OBC shall support all 5 flight mode transitions defined in FMM-DES-001 §6 | Mandatory | Test |
| MIS-O-002 | The OBC shall provide flight mode status in every HK telemetry packet | Mandatory | Test |
| MIS-O-003 | The OBC shall log all flight mode transitions to the persistent event logger | Mandatory | Test |
| MIS-O-004 | Ground operators shall be able to command a mode change from any mode to FM_SAFE without preconditions | Mandatory | Test |

---

## 7. Constraints and Assumptions

### 7.1 Constraints

| ID    | Constraint |
|-------|------------|
| CON-1 | Platform is fixed: RP2350 (Pico 2W). No migration to alternative MCU within v1.0 scope. |
| CON-2 | RF band: 433–435 MHz (UHF amateur satellite band, IARU Region 2). Amateur license required for operations. |
| CON-3 | ADCS Phase 1 uses magnetorquers only. Reaction wheel precision pointing is Phase 2. |
| CON-4 | Software shall comply with ECSS-Q-ST-80C and achieve 0 MISRA C required/mandatory violations. |
| CON-5 | All flight software shall be written in C11 (no C++ in flight code). |
| CON-6 | No custom hardware modifications to Pico 2W PCB in Phase 1. |

### 7.2 Assumptions

| ID    | Assumption |
|-------|------------|
| ASS-1 | The CubeSat will be deployed via a standard P-POD deployer; deployment inhibit is handled by deployer, not OBC. |
| ASS-2 | GPS (NEO-7M) provides orbital position and time reference at ≥ 1 Hz. |
| ASS-3 | Ground station is operated by a trained operator during contact windows. |
| ASS-4 | The battery is a 2S Li-ion / LiPo pack with nominal voltage 7.4 V and capacity sufficient for ≥ 37 min eclipse at full load. |
| ASS-5 | The Earth's magnetic field at targeted altitude is ≥ 25 µT — sufficient for B-dot detumbling and magnetometer yaw update. |

---

## 8. Interface Requirements Summary

Full interface definitions are in `ICD-OBC-001`.

| Interface | Counterpart | Protocol | Requirement |
|-----------|-------------|----------|-------------|
| IMU | MPU6050 | I²C @ 400 kHz (GP4/GP5) | SyRS-F-101 — 10 Hz minimum |
| Magnetometer | LIS3MDL | I²C @ 400 kHz | SyRS-F-105 — 80 Hz ODR |
| TT&C radio | E22-400M30S | UART1 @ 9600 baud KISS | MIS-C-001/002 |
| GPS | NEO-7M | UART0 @ 9600 baud NMEA 0183 | ASS-2 |
| Watchdog | TPS3431 | GPIO20 (kick) | MIS-D-002 |
| EPS (battery) | INA219 ADC | I²C (Phase 2) | MIS-D-004 |
| Reaction wheels | TB6612FNG H-bridge | PWM (Phase 2) | MO-2 (Phase 2) |

---

## 9. Verification Approach

| Method | Applies To |
|--------|------------|
| **Analysis** | Link budget (MIS-C-003), thermal (MIS-E-001), SEU (MIS-E-003) |
| **Inspection** | Code review for MISRA compliance, OBC rail protection (MIS-D-005) |
| **Test (unit)** | All MIS-P/D/C/O using host-build test suite (`test_eps_monitor`, `test_fmm`, etc.) |
| **Test (integration)** | Closed-loop simulation (`closed_loop_test`) for MO-1/MO-2 |
| **Test (qualification)** | Vibration, thermal cycling — Phase 2/3 |

Traceability from MIS-xxx to tests is maintained in `RTM-OBC-001`.

---

## 10. Open Items

| OI | Description | Priority | Phase |
|----|-------------|----------|-------|
| OI-1 | Define formal payload (scientific instrument) — mission has no payload defined yet; PAYLOAD rail management assumes a future instrument | High | 2 |
| OI-2 | Obtain IARU amateur satellite frequency coordination (435–438 MHz) | High | Pre-launch |
| OI-3 | Define passivization sequence for end-of-life compliance (ECSS-E-ST-10-03C) | Medium | 3 |
| OI-4 | Confirm launch vehicle and P-POD compatibility — deployment inhibit interface TBD | High | Pre-launch |
| OI-5 | Define qualification thermal and vibration test plan (MIS-E-001, MIS-E-002) | Medium | 3 |
