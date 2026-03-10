# FMEA-OBC-001 — Failure Mode and Effects Analysis

| Field           | Value                                |
|-----------------|--------------------------------------|
| Document ID     | FMEA-OBC-001                         |
| Version         | 0.2                                  |
| Date            | 2026-03-10                           |
| Author          | OBC Systems Team                     |
| Status          | CDR Baseline — SRR-resolved           |
| Classification  | Internal                             |

## Change History

| Version | Date       | Author           | Description                              |
|---------|------------|------------------|------------------------------------------|
| 0.1     | 2026-03-09 | OBC Systems Team | Initial CDR baseline FMEA — all 25 fault IDs from `fault_ids.h`; MRD hazards H-1..H-5; severity/RPN analysis; FDIR response mapping |
| 0.2     | 2026-03-10 | OBC Systems Team | SRR-OBC-001 ACT-15: assign open-item owners and target dates; update SRS reference to v2.1; payload OI-2 partially resolved by SyRS §7.5 (SYS-F-500..504) |

## Table of Contents

1. [Introduction](#1-introduction)
2. [Applicable Documents](#2-applicable-documents)
3. [Acronyms and Definitions](#3-acronyms-and-definitions)
4. [FMEA Methodology](#4-fmea-methodology)
5. [System Boundary and Scope](#5-system-boundary-and-scope)
6. [Hazard Inputs from MRD](#6-hazard-inputs-from-mrd)
7. [Software Fault FMEA](#7-software-fault-fmea)
   - 7.1 [Estimator Subsystem](#71-estimator-subsystem-0x0100)
   - 7.2 [Controller Subsystem](#72-controller-subsystem-0x0200)
   - 7.3 [Actuator Subsystem](#73-actuator-subsystem-0x0300)
   - 7.4 [Sensor Subsystem](#74-sensor-subsystem-0x0400)
   - 7.5 [Timing Subsystem](#75-timing-subsystem-0x0500)
   - 7.6 [Watchdog Subsystem](#76-watchdog-subsystem-0x0600)
   - 7.7 [Command Subsystem](#77-command-subsystem-0x0700)
   - 7.8 [Thermal Subsystem](#78-thermal-subsystem-0x0800)
   - 7.9 [EPS Subsystem](#79-eps-subsystem-0x0900)
   - 7.10 [Telemetry Subsystem](#710-telemetry-subsystem-0x0a00)
8. [Single-Point Failure Analysis](#8-single-point-failure-analysis)
9. [RPN Summary and Priorities](#9-rpn-summary-and-priorities)
10. [Mitigation Status](#10-mitigation-status)
11. [Open Items](#11-open-items)
12. [References](#12-references)

---

## 1. Introduction

### 1.1 Purpose

This document is the Failure Modes and Effects Analysis (FMEA) for the CubeSat
OBC flight software. It systematically identifies each software fault that the
Fault Manager can detect (`fault_ids.h`), assesses its effects and severity, and
maps it to the implemented FDIR response.

The FMEA drives:

- Completeness verification of the fault detection catalogue
- Risk Priority Number (RPN) ranking for CDR action tracking
- Traceability from MRD mission hazards (§6.2.1) to FDIR implementation

### 1.2 Scope

- **In scope**: All 25 fault IDs defined in `include/fault_ids.h` v0.1;
  MRD-OBC-001 §6.2.1 hazards H-1..H-5; software FDIR response in `fault_manager.c`.
- **Out of scope**: Hardware-level FMEA (EEE parts, PCB trace failures, radiation
  SEU beyond software recovery); payload faults (payload undefined — OI-1 in MRD).

---

## 2. Applicable Documents

| ID              | Title                                              | Version |
|-----------------|----------------------------------------------------|---------|
| MRD-OBC-001     | Mission Requirements Document / ConOps             | 1.1     |
| FAULT-DES-001   | Fault Manager Design Document                      | 0.2     |
| EPS-DES-001     | Electrical Power System Design Document            | 0.1     |
| ADCS-DES-001    | ADCS Design Document                               | 1.0     |
| FMM-DES-001     | Flight Mode Manager Design Document                | 0.3     |
| FSW-SDD-001     | Flight Software Design Description                 | 0.2     |
| SRS-OBC-001     | Software Requirements Specification                | 2.0     |
| ECSS-Q-ST-30-02C | Failure modes, effects (and criticality) analysis | —       |

---

## 3. Acronyms and Definitions

| Term  | Definition |
|-------|-----------|
| FMEA  | Failure Mode and Effects Analysis |
| FMECA | FMEA + Criticality Analysis |
| FDIR  | Fault Detection, Isolation and Recovery |
| FM    | Flight Mode (`FM_BOOT`, `FM_SAFE`, `FM_DETUMBLE`, `FM_NOMINAL`, `FM_DIAGNOSTIC`) |
| FMM   | Flight Mode Manager |
| RPN   | Risk Priority Number = Severity × Occurrence × Detectability (each 1–5 scale) |
| SPF   | Single-Point Failure — one failure alone causes mission loss |
| WDT   | Watchdog Timer (TPS3431 hardware) |
| HWM   | High Water Mark (FreeRTOS stack metric) |
| DLA   | Data Layer Abstraction |

---

## 4. FMEA Methodology

### 4.1 Scoring Scales

**Severity (S):**

| Score | Level    | Effect on Mission |
|-------|----------|-------------------|
| 5     | CRITICAL | Mission loss or permanent safe-mode; MO cannot be met |
| 4     | MAJOR    | Significant degradation; primary MO at risk |
| 3     | MODERATE | Partial function lost; secondary MO affected |
| 2     | MINOR    | Isolated anomaly; no MO impact |
| 1     | NEGLIGIBLE | Logged only; transparent to operations |

**Occurrence (O):**

| Score | Description | Approximate Rate |
|-------|-------------|-----------------|
| 5     | Frequent    | Multiple times per orbit |
| 4     | Probable    | Once per day |
| 3     | Occasional  | Once per week |
| 2     | Remote      | Once per mission year |
| 1     | Improbable  | Theoretically possible |

**Detectability (D):**

| Score | Description | Mechanism |
|-------|-------------|-----------|
| 1     | Certain     | Fault flag set; logged; HK telemetry reflects state |
| 2     | High        | Fault logged; not propagated to HK |
| 3     | Moderate    | Detectable by ground via trend analysis |
| 4     | Low         | Detectable only with FM_DIAGNOSTIC telemetry |
| 5     | Undetectable | No observability |

**RPN = S × O × D** (max 125). Actions required if RPN ≥ 40 or S = 5.

### 4.2 FDIR Response Legend

| Code | Response |
|------|----------|
| LOG  | Event logged to flash ring buffer (class per severity) |
| HK   | Reflected in next HK telemetry packet `fault_flags` bitmask |
| FS   | `fmm_force_safe()` called → FM_SAFE transition |
| WDT  | Hardware TPS3431 WDT fires within 8 s → reboot |
| AUTO-CLR | `FAULT_LEVEL_WARNING` auto-cleared after 30 s without re-raise |

---

## 5. System Boundary and Scope

```
┌────────────────────────────────────────────────────────────────┐
│  CubeSat OBC Flight Software (RP2350 / FreeRTOS)               │
│                                                                │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────────┐    │
│  │Estimator │  │Controller│  │ Actuator │  │   Sensor     │    │
│  │(EKF)     │  │(LQR/Bdot)│  │(RW, MTQ) │  │(IMU,Mag,Tmp) │    │
│  └─────┬────┘  └─────┬────┘  └─────┬────┘  └──────┬───────┘    │
│        │             │             │              │            │
│        └─────────────┴─────────────┴──────────────┘            │
│                              │                                 │
│                   ┌──────────▼──────────┐                      │
│                   │   Fault Manager     │  ← fault_ids.h       │
│                   │   `fault_manager.c` │    25 fault IDs      │
│                   └──────────┬──────────┘                      │
│                              │ CRITICAL → fmm_force_safe()     │
│              ┌───────────────▼──────────────────┐              │
│              │  Flight Mode Manager (FMM)       │              │
│              └──────────────────────────────────┘              │
└────────────────────────────────────────────────────────────────┘
```

---

## 6. Hazard Inputs from MRD

The following top mission hazards (MRD-OBC-001 §6.2.1) are the primary FMEA
drivers. Each maps to one or more fault IDs in the catalogue below.

| Hazard | Description | Fault IDs Covering It | Mitigation |
|--------|------------|----------------------|------------|
| H-1 | Battery depletion → OBC power loss | `FAULT_EPS_VBATT_LOW/CRITICAL/EMERGENCY` | EPS Schmidt trigger; load shedding; OBC rail always on |
| H-2 | ADCS tumble → uncontrolled rotation | `FAULT_EST_DIVERGENCE`, `FAULT_CTRL_DEADLINE_MISS`, `FAULT_ACT_MTQ_FAULT` | FM_DETUMBLE autonomous; B-dot available in any mode |
| H-3 | OBC crash / WDT reset | `FAULT_WDT_KICK_MISSED`, `FAULT_TIMING_DEADLINE_MISS` | TPS3431 HW WDT; boot state logged to flash |
| H-4 | RF link loss | `FAULT_TLM_QUEUE_OVERFLOW` (indirect) | FM_SAFE listen-only; FDIR continues without GS |
| H-5 | Event log corruption | (flash driver — OI-2 in FSW-SDD-001) | Ring-buffer headers; Class A CRC; power-fail-safe write |

---

## 7. Software Fault FMEA

### 7.1 Estimator Subsystem (0x0100)

| Fault ID | Value | Failure Mode | Local Effect | Mission Effect | S | O | D | RPN | FDIR Response | Mitigation |
|----------|-------|--------------|-------------|----------------|---|---|---|-----|---------------|------------|
| `FAULT_EST_GYRO_TIMEOUT` | 0x0101 | IMU gyro read times out (I²C bus hang or MPU-6050 unresponsive) | EKF propagation starved; attitude estimate degrades | Attitude unknown; pointing lost; H-2 risk elevated | 4 | 2 | 1 | 8 | LOG + HK; escalate to ERROR after repeated misses | `imu_read()` returns error; task raises fault; DLA marks `imu_valid = 0` |
| `FAULT_EST_MAG_TIMEOUT` | 0x0102 | Magnetometer read times out | Magnetometer yaw update lost | Attitude drift in yaw; detumble degrades | 3 | 2 | 1 | 6 | LOG + HK; WARNING auto-clear 30 s | `mag_read()` returns error; sensor read task raises fault |
| `FAULT_EST_DIVERGENCE` | 0x0103 | EKF covariance blows up; `‖P‖ > P_max` | EKF outputs unusable | LQR drives actuators on bad input; potential tumble (H-2) | 4 | 1 | 2 | 8 | LOG + HK; ERROR — no autonomous FS | EKF divergence check in `ekf_update()`; reset EKF state on divergence |
| `FAULT_EST_QUAT_NORM` | 0x0104 | Quaternion norm ‖q‖ ≠ 1 ± ε after EKF update | Attitude representation invalid | Invalid pointing command; actuator saturation risk | 3 | 1 | 1 | 3 | LOG + HK; ERROR | `ekf_update()` re-normalizes quaternion; fault raised if re-norm fails |

### 7.2 Controller Subsystem (0x0200)

| Fault ID | Value | Failure Mode | Local Effect | Mission Effect | S | O | D | RPN | FDIR Response | Mitigation |
|----------|-------|--------------|-------------|----------------|---|---|---|-----|---------------|------------|
| `FAULT_CTRL_OUTPUT_SATURATED` | 0x0201 | LQR/B-dot output exceeds actuator limit | Torque command clipped | Pointing transient; MO-2/MO-3 less efficient | 2 | 3 | 1 | 6 | LOG; WARNING auto-clear | `lqr_compute()` clamps output; logs saturation counter |
| `FAULT_CTRL_RATE_LIMIT` | 0x0202 | Commanded angular rate exceeds safe limit | Rate limiter active; reduced maneuver bandwidth | Slower detumble; MO-2 timeline extends | 2 | 2 | 1 | 4 | LOG; WARNING/ERROR | Rate limiter in attitude control task; hard-coded envelope |
| `FAULT_CTRL_DEADLINE_MISS` | 0x0203 | AttitudeCtrl task misses 100 ms period | Control loop executes late or skipped | Open-loop interval; attitude drifts; H-2 risk | 3 | 1 | 1 | 3 | LOG + HK; ERROR | FreeRTOS deadline check in `vAttitudeControlTask`; `vTaskDelayUntil` |

### 7.3 Actuator Subsystem (0x0300)

| Fault ID | Value | Failure Mode | Local Effect | Mission Effect | S | O | D | RPN | FDIR Response | Mitigation |
|----------|-------|--------------|-------------|----------------|---|---|---|-----|---------------|------------|
| `FAULT_ACT_RW_OVERCURRENT` | 0x0301 | Reaction wheel motor draws > rated current | Driver shutdown or brownout risk | Loss of precision pointing (Phase 2) | 4 | 1 | 2 | 8 | LOG + HK; ERROR; RW disabled | INA219 current monitor (Phase 2); PWM duty-cycle limit in firmware |
| `FAULT_ACT_RW_SPEED_LIMIT` | 0x0302 | Reaction wheel speed exceeds max RPM | Mechanical stress; momentum saturated | Momentum dump required; temporary loss of pointing | 3 | 2 | 1 | 6 | LOG; WARNING; trigger momentum dump | Speed encoder feedback (Phase 2); `momentum_dump_trigger()` |
| `FAULT_ACT_MTQ_FAULT` | 0x0303 | Magnetorquer driver reports fault (overcurrent, driver IC fault) | MTQ output corrupted or lost | Detumble impaired; H-2 risk | 4 | 1 | 2 | 8 | LOG + HK; ERROR; MTQ disabled | PWM driver health check in `mtq_set_dipole()`; fault raised on HAL error |

### 7.4 Sensor Subsystem (0x0400)

| Fault ID | Value | Failure Mode | Local Effect | Mission Effect | S | O | D | RPN | FDIR Response | Mitigation |
|----------|-------|--------------|-------------|----------------|---|---|---|-----|---------------|------------|
| `FAULT_SENS_IMU_I2C_ERROR` | 0x0401 | I²C bus error on IMU transaction (NACK, timeout, bus stuck) | IMU read fails; DLA data stale | Attitude estimation pauses; H-2 risk | 4 | 2 | 1 | 8 | LOG + HK; ERROR | I²C retry (×3) in `pico_i2c.c`; bus reset on repeated failure |
| `FAULT_SENS_IMU_DATA_STALE` | 0x0402 | IMU data not refreshed within staleness window | EKF uses old data | Attitude drift; degraded MO-1/MO-2 | 3 | 2 | 1 | 6 | LOG; WARNING auto-clear | DLA staleness timeout check in `data_layer.h`; HK carries `imu_valid` |
| `FAULT_SENS_TEMP_OUT_RANGE` | 0x0403 | Temperature sensor reads outside –40°C to +85°C | Possible hardware thermal stress | Board reliability risk; no immediate mission impact | 2 | 1 | 1 | 2 | LOG; WARNING auto-clear | ADC4 driver plausibility check; thermal control actions Phase 2 |

### 7.5 Timing Subsystem (0x0500)

| Fault ID | Value | Failure Mode | Local Effect | Mission Effect | S | O | D | RPN | FDIR Response | Mitigation |
|----------|-------|--------------|-------------|----------------|---|---|---|-----|---------------|------------|
| `FAULT_TIMING_DEADLINE_MISS` | 0x0501 | Generic FreeRTOS task deadline missed (any task other than AttitudeCtrl) | Task period violated; DLA data potentially stale | Depends on affected task; worst case: telemetry gap or sensor gap | 3 | 1 | 1 | 3 | LOG + HK; WARNING/ERROR | `vTaskDelayUntil` in all periodic tasks; HWM monitoring |

### 7.6 Watchdog Subsystem (0x0600)

| Fault ID | Value | Failure Mode | Local Effect | Mission Effect | S | O | D | RPN | FDIR Response | Mitigation |
|----------|-------|--------------|-------------|----------------|---|---|---|-----|---------------|------------|
| `FAULT_WDT_KICK_MISSED` | 0x0601 | WDT kick not delivered within 8 s window | TPS3431 asserts RESET; OBC reboots | OBC reset; H-3; FM_BOOT sequence; data in-flight lost | 4 | 1 | 1 | 4 | **WDT hardware reset** — `fault_report()` logged to scratch; post-reset recovery | `watchdog_hal_kick()` in `vHealthMonitorTask`; pre-reset state written to `watchdog_hw->scratch[0..3]` |

### 7.7 Command Subsystem (0x0700)

| Fault ID | Value | Failure Mode | Local Effect | Mission Effect | S | O | D | RPN | FDIR Response | Mitigation |
|----------|-------|--------------|-------------|----------------|---|---|---|-----|---------------|------------|
| `FAULT_CMD_UNKNOWN` | 0x0701 | Unrecognised command opcode received on CSP port 20 | Command rejected; ACK `result=0x02` (invalid) | Operator receives negative ACK; no state change | 1 | 2 | 1 | 2 | LOG; WARNING auto-clear | `vCommandTask` whitelist check; NACK with `reason` byte |
| `FAULT_CMD_QUEUE_FULL` | 0x0702 | Command queue overflow (burst of uplink TCs) | Commands dropped | Operator commands lost; must retransmit | 2 | 1 | 1 | 2 | LOG + HK; WARNING auto-clear | CSP RX buffer limited; command task processes within 1 s |

### 7.8 Thermal Subsystem (0x0800)

| Fault ID | Value | Failure Mode | Local Effect | Mission Effect | S | O | D | RPN | FDIR Response | Mitigation |
|----------|-------|--------------|-------------|----------------|---|---|---|-----|---------------|------------|
| `FAULT_THERM_OVER_TEMP` | 0x0801 | Temperature > +85°C (RP2350 absolute max) | Possible silicon damage; reliability degraded | Board failure risk; could cause permanent mission loss | 5 | 1 | 1 | 5 | LOG + HK; **CRITICAL → FS** | Internal ADC4 monitor; Phase 2: active thermal control loop |
| `FAULT_THERM_UNDER_TEMP` | 0x0802 | Temperature < –20°C operational min | Driver timing margins may be violated | Transient I²C/SPI errors; sensor noise increases | 3 | 1 | 1 | 3 | LOG; WARNING auto-clear | Heater resistor planned (Phase 2); operational constraint until then |

### 7.9 EPS Subsystem (0x0900)

| Fault ID | Value | Failure Mode | Local Effect | Mission Effect | S | O | D | RPN | FDIR Response | Mitigation |
|----------|-------|--------------|-------------|----------------|---|---|---|-----|---------------|------------|
| `FAULT_EPS_VBATT_LOW` | 0x0901 | V\_batt < 7.4 V (falling) | `ENERGY_LOW` state; optional payload shed | Reduced capability; EPS FSM active | 2 | 3 | 1 | 6 | LOG + HK; WARNING auto-clear on recovery | Schmidt hysteresis 7.4 V ↓ / 7.5 V ↑; published to DLA |
| `FAULT_EPS_VBATT_CRITICAL` | 0x0902 | V\_batt < 7.0 V (falling) | `ENERGY_CRITICAL`; attitude loads shed; FM_SAFE requested | FM_SAFE entry; MO-1/MO-2 suspended; H-1 active | 4 | 2 | 1 | 8 | LOG + HK; **CRITICAL → FS** | Hysteresis 7.0 V ↓ / 7.15 V ↑; load shedding in EPS FSM |
| `FAULT_EPS_VBATT_EMERGENCY` | 0x0903 | V\_batt < 6.6 V (falling) | `ENERGY_EMERGENCY`; all non-OBC rails shed | OBC-only survival; H-1 imminent; mission data loss risk | 5 | 1 | 1 | 5 | LOG + HK; **CRITICAL → FS** (forced) | EMERGENCY threshold is last line before shutdown; OBC rail always on |
| `FAULT_EPS_OVERCURRENT` | 0x0904 | Bus overcurrent detected (INA219 Phase 2) | Overcurrent protection active; rail may shed | Load lost; possible hardware damage | 4 | 1 | 2 | 8 | LOG + HK; ERROR; rail shed command | Phase 2 INA219 driver; Phase 1: ADC0 + software limit check |
| `FAULT_EPS_READ_ERROR` | 0x0905 | EPS telemetry read failure (ADC timeout or I²C error on INA219) | Battery voltage unknown | EPS FSM cannot classify energy state; defaults to ENERGY_LOW | 3 | 2 | 1 | 6 | LOG + HK; ERROR; default to safe energy state | `eps_hal_read()` returns error; EPS FSM holds last known state |

### 7.10 Telemetry Subsystem (0x0A00)

| Fault ID | Value | Failure Mode | Local Effect | Mission Effect | S | O | D | RPN | FDIR Response | Mitigation |
|----------|-------|--------------|-------------|----------------|---|---|---|-----|---------------|------------|
| `FAULT_TLM_QUEUE_OVERFLOW` | 0xA01 | CSP TX FIFO / queue full; HK packet dropped | Ground does not receive HK this cycle | Data gap in HK time series; H-4 indicator | 2 | 2 | 2 | 8 | LOG; WARNING auto-clear | At 1 Hz × 42 B, well within 115200 baud throughput; flow-control in `csp_send()` |

---

## 8. Single-Point Failure Analysis

A Single-Point Failure (SPF) is defined as any single fault that alone results
in **loss of primary mission objectives** (MO-1 through MO-5) with no
recovery path.

| SPF Candidate | Fault ID(s) | Failure Mechanism | SPF? | Rationale |
|---------------|------------|-------------------|------|-----------|
| EPS emergency voltage collapse | `FAULT_EPS_VBATT_EMERGENCY` | Solar array or battery failure below 6.6 V | **Yes** (hardware) | Software cannot recover from complete power loss; OBC rail HW-protected |
| I²C bus stuck-low | `FAULT_SENS_IMU_I2C_ERROR` | Bus stuck (external event / ESD) | **Yes — if no recovery** | I²C bus reset is implemented; if hardware failure persists, ADCS lost |
| WDT kick miss in all tasks | `FAULT_WDT_KICK_MISSED` | Deadlock or runaway loop blocking HealthMon | **No** — WDT resets OBC | TPS3431 provides HW recovery; post-reset boot re-enters FM_SAFE |
| Flash log corruption | H-5 | Power loss during flash write | **No** — design mitigation | CRC-protected headers; ring buffer survives partial write |
| Over-temperature | `FAULT_THERM_OVER_TEMP` | Extended sunlit period without thermal control | **Conditional** | Phase 1: no active heater/cooler; operational thermal control via GS command |

---

## 9. RPN Summary and Priorities

Items with RPN ≥ 8 or S = 5 requiring CDR action:

| Priority | Fault ID | RPN | S | Action Required |
|----------|----------|-----|---|-----------------|
| 1 | `FAULT_THERM_OVER_TEMP` | 5 (S=5) | 5 | Phase 2 thermal control; operational thermal limit defined in SVVP |
| 2 | `FAULT_EPS_VBATT_EMERGENCY` | 5 (S=5) | 5 | Battery sizing min 2S 2000 mAh; verify in POWER-BDG-001 |
| 3 | `FAULT_EPS_VBATT_CRITICAL` | 8 | 4 | Load shedding tested in SVVP-OBC-001 T-EPS-04 |
| 4 | `FAULT_EST_GYRO_TIMEOUT` | 8 | 4 | IMU retry logic verified; I²C bus-reset path tested |
| 5 | `FAULT_SENS_IMU_I2C_ERROR` | 8 | 4 | I²C retry ×3 + bus reset — verify in integration tests |
| 6 | `FAULT_ACT_RW_OVERCURRENT` | 8 | 4 | Phase 2 INA219 required before RW activation |
| 7 | `FAULT_ACT_MTQ_FAULT` | 8 | 4 | MTQ fault injection test required (SVVP T-ACT-01) |
| 8 | `FAULT_EPS_OVERCURRENT` | 8 | 4 | Phase 2 INA219 driver needed |
| 9 | `FAULT_TLM_QUEUE_OVERFLOW` | 8 | 2 | Low actual risk at 1 Hz; no further action CDR |

---

## 10. Mitigation Status

| ID | Mitigation | Implementation | Status | Verification |
|----|-----------|---------------|--------|-------------|
| M-01 | EPS Schmidt-trigger hysteresis (7.4/7.0/6.6 V thresholds) | `eps_monitor.c`, `eps_thresholds.h` | Implemented | T-EPS-01..04 in SVVP-OBC-001 |
| M-02 | FreeRTOS WDT kick in `vHealthMonitorTask` | `health_monitor_task.c:33` | Implemented | T-WDT-01/02 |
| M-03 | I²C retry ×3 + bus reset on persistent error | `pico_i2c.c` | Implemented | T-SENS-01 |
| M-04 | EKF divergence check + quaternion re-normalization | `ekf.c` | Implemented | Unit test `test_ekf_divergence` |
| M-05 | `fault_manager_report()` CRITICAL → `fmm_force_safe()` | `fault_manager.c` | Implemented | T-FMGR-01..03 |
| M-06 | Stack overflow canary + WDT scratch logging | `FreeRTOSConfig.h` + `main.c` | Implemented | `test_watchdog_recovery` |
| M-07 | Rate limiter in attitude control task | `attitude_control_task.c` | Implemented | T-CTRL-01 |
| M-08 | INA219 overcurrent monitor (Phase 2) | Not yet implemented | **Planned Ph2** | T-EPS-05 (SVVP) |
| M-09 | Active thermal control loop (Phase 2) | Not yet implemented | **Planned Ph2** | T-THERM-01 (SVVP) |
| M-10 | W25Qxx real flash backend for log persistence | Not yet implemented | **Planned Ph3** | T-LOG-03 (SVVP) |

---

## 11. Open Items

| OI  | Description | Priority | Owner | Target Date | Status |
|-----|-------------|----------|-------|-------------|--------|
| OI-1 | `FAULT_COMM_*` IDs not defined — communication subsystem has no fault IDs in `fault_ids.h`; tracked FSW-SDD-001 OI-2 / COMMS-DES-001 OI-5 | Medium | Communications Lead | CDR (2026-06-30) | Open |
| OI-2 | Payload fault IDs undefined — payload concept now defined in SyRS §7.5 (SYS-F-500..504, ACT-04); `FAULT_PAYLOAD_OVERCURRENT` stub added (SYS-F-502). Full fault catalogue at CDR when hardware confirmed. | Medium | Systems Eng. Lead | CDR (2026-06-30) | Partially Resolved |
| OI-3 | FMECA criticality number (C = β × λ × t) not computed — requires component failure rate data from parts manufacturer | Low | Hardware Lead | QR (2026-09-30) | Open |
| OI-4 | SPF for over-temperature in Phase 1 — no autonomous thermal control; mitigated by operational procedure only | High | Software Lead | CDR (2026-06-30): add thermal watchdog to FMM; QR: validate with thermal model |  Open |

---

## 12. References

| Ref | Document |
|-----|----------|
| [1] | FAULT-DES-001 v0.2 — Fault Manager Design Document |
| [2] | FSW-SDD-001 v0.2 — Flight Software Design Description |
| [3] | EPS-DES-001 v0.1 — Electrical Power System Design Document |
| [4] | MRD-OBC-001 v1.1 — Mission Requirements Document §6.2.1 (Hazards) |
| [5] | SRS-OBC-001 v2.1 — Software Requirements Specification |
| [6] | `include/fault_ids.h` v0.1 — Fault ID Catalogue (25 IDs, 10 subsystems) |
| [7] | ECSS-Q-ST-30-02C — Failure modes, effects (and criticality) analysis |
| [8] | ADCS-DES-001 v1.0 — ADCS Design Document |
| [9] | FMM-DES-001 v0.3 — Flight Mode Manager Design Document |
