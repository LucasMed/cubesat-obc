# FSW-SDD-001 — Flight Software Design Description

| Field            | Value                                              |
|------------------|----------------------------------------------------|
| **Document ID**  | FSW-SDD-001                                        |
| **Title**        | Flight Software Design Description                 |
| **Project**      | CubeSat OBC — RP2350 / Pico 2W                     |
| **Version**      | 0.3                                                |
| **Status**       | Released — PDR Alignment Baseline                  |
| **Date**         | 2026-03-11                                         |
| **Author**       | OBC Systems Team                                   |
| **Review Level** | CDR                                                |
| **Standard**     | ECSS-E-ST-40C §5.5, ECSS-Q-ST-80C                 |

---

## Change History

| Version | Date       | Author           | Description                         |
|---------|------------|------------------|-------------------------------------|
| 0.1     | 2026-03-09 | OBC Systems Team | Initial CDR baseline                |
| 0.2     | 2026-03-09 | OBC Systems Team | Address CDR review observations: SRAM regions table (§13.2), CPU budget estimate (§14), FMM transition conditions (§8.1), EPS execution context (§8.3, §7.7), logger flash driver target (§8.4), HK packet table (§7.5), Command ACK format (§7.6), OI-2/OI-4 clarifications, new OI-8 (heap sizing) |
| 0.3     | 2026-03-11 | OBC Systems Team | PDR Alignment: harmonize I2C pins, update EKF to 7-state quaternion baseline, close resolved OI-1, OI-2, OI-8 |

---

## Table of Contents

1. [Introduction](#1-introduction)
2. [Applicable Documents](#2-applicable-documents)
3. [Acronyms and Definitions](#3-acronyms-and-definitions)
4. [Software Architecture Overview](#4-software-architecture-overview)
5. [Software Layer Descriptions](#5-software-layer-descriptions)
6. [Module Inventory](#6-module-inventory)
7. [Task Design](#7-task-design)
8. [Service Layer Design](#8-service-layer-design)
9. [Data Layer Abstraction (DLA)](#9-data-layer-abstraction-dla)
10. [Driver and HAL Layer](#10-driver-and-hal-layer)
11. [Boot Sequence](#11-boot-sequence)
12. [Inter-Task Communication](#12-inter-task-communication)
13. [Memory Management](#13-memory-management)
14. [CPU Budget Estimate](#14-cpu-budget-estimate)
15. [Error Handling and FDIR Integration](#15-error-handling-and-fdir-integration)
16. [Build System and Platform Portability](#16-build-system-and-platform-portability)
17. [Traceability to Requirements](#17-traceability-to-requirements)
18. [Open Items](#18-open-items)
19. [References](#19-references)

---

## 1. Introduction

### 1.1 Purpose

This Flight Software Design Description (FSW-SDD) is the top-level CDR design
document for the CubeSat OBC flight software. It consolidates the software
architecture, module decomposition, task design, data-flow, and inter-module
interfaces into a single navigable reference, cross-linking all subsystem
`*-DES-001` design documents produced for CDR.

Downstream reviewers should read `SAD-OBC-001` for the original architecture
baseline and individual `*-DES-001` documents for per-subsystem details. This
SDD is the entry point.

### 1.2 Scope

This document covers all software executing on the CubeSat OBC RP2350 target
and the corresponding host test build:

- FreeRTOS task model (all tasks, priorities, stack sizes)
- Service layer: FMM, Fault Manager, EPS Monitor, Event Logger, EKF, LQR, DLA
- Driver/HAL layer: I²C, UART, IMU, magnetometer, temperature, watchdog
- Boot sequence and initialization order
- Inter-task communication mechanisms (DLA, direct API calls)
- Memory budget and stack usage

It does **not** cover:
- Ground software or ground-segment protocol stacks
- Pico SDK internals or FreeRTOS kernel internals
- Hardware electrical design (see `BOM-OBC-001`)

### 1.3 Relationship to Other Documents

```
MRD-OBC-001 (mission)
   └── SRS-OBC-001 (software requirements)
         ├── SAD-OBC-001   — architecture baseline (PDR)
         ├── FSW-SDD-001   — this document (CDR consolidation)
         ├── ADCS-DES-001  — ADCS subsystem design
         ├── FMM-DES-001   — Flight Mode Manager design
         ├── EPS-DES-001   — EPS Monitor design
         ├── FAULT-DES-001 — Fault Manager design
         ├── COMMS-DES-001 — TT&C communications design
         ├── DL-DES-001    — Data Layer / storage design
         └── OBC-DES-001   — OBC hardware & CDH design
```

### 1.4 Document Identifier

`FSW-SDD-001 v0.2`

---

## 2. Applicable Documents

| ID              | Title                                              | Version |
|-----------------|----------------------------------------------------|---------|
| MRD-OBC-001     | Mission Requirements Document / ConOps             | 1.1     |
| SRS-OBC-001     | Software Requirements Specification                | 2.0     |
| SyRS-OBC-001    | System Requirements Specification                  | 1.0     |
| SAD-OBC-001     | System Architecture Document                       | 1.0     |
| OBC-DES-001     | OBC Hardware & CDH Design Document                 | 0.1     |
| ADCS-DES-001    | ADCS Design Document                               | 1.0     |
| FMM-DES-001     | Flight Mode Manager Design Document                | 0.3     |
| EPS-DES-001     | Electrical Power System Design Document            | 0.1     |
| FAULT-DES-001   | Fault Manager Design Document                      | 0.2     |
| COMMS-DES-001   | Communications / TT&C Design Document              | 0.1     |
| DL-DES-001      | Data Layer / Storage Design Document               | 0.1     |
| ICD-OBC-001     | Interface Control Document                         | 1.1     |
| BOM-OBC-001     | Bill of Materials                                  | 1.0.1   |
| CODING_STANDARDS.md | CubeSat OBC Coding Standards                   | 1.0     |
| ECSS-E-ST-40C   | Software Engineering Standard                      | —       |
| ECSS-Q-ST-80C   | Software Product Assurance                         | —       |

---

## 3. Acronyms and Definitions

| Acronym | Definition |
|---------|------------|
| ADCS    | Attitude Determination and Control System |
| CDR     | Critical Design Review |
| CSP     | CubeSat Space Protocol |
| DLA     | Data Layer Abstraction |
| EKF     | Extended Kalman Filter |
| EPS     | Electrical Power System |
| FDIR    | Fault Detection, Isolation, and Recovery |
| FM      | Flight Mode |
| FMM     | Flight Mode Manager |
| FSW     | Flight Software |
| GPIO    | General-Purpose Input/Output |
| HAL     | Hardware Abstraction Layer |
| HK      | Housekeeping (telemetry) |
| HIL     | Hardware-in-the-Loop |
| HWM     | High Water Mark (FreeRTOS stack usage metric) |
| I²C     | Inter-Integrated Circuit (serial bus) |
| IMU     | Inertial Measurement Unit |
| ISR     | Interrupt Service Routine |
| KISS    | Keep It Simple, Stupid (framing protocol) |
| LQR     | Linear Quadratic Regulator |
| MRR     | Mission Requirements Review |
| MTQ     | Magnetorquer |
| OBC     | On-Board Computer |
| PDR     | Preliminary Design Review |
| PWM     | Pulse-Width Modulation |
| RW      | Reaction Wheel |
| SDD     | Software Design Description |
| SEU     | Single-Event Upset |
| SMP     | Symmetric Multi-Processing |
| SRR     | System Requirements Review |
| TT&C    | Telemetry, Tracking, and Command |
| UART    | Universal Asynchronous Receiver/Transmitter |
| WDT     | Watchdog Timer |

---

## 4. Software Architecture Overview

### 4.1 Top-Level Layered Architecture

The flight software is structured in four horizontal layers, each with a strict
upward-dependency rule: no lower layer may call into a higher layer.

```
┌─────────────────────────────────────────────────────────────────────────┐
│                         TASK LAYER (Application)                        │
│  SensorRead   AttitudeCtrl   Telemetry   Command   HealthMon            │
│  (FreeRTOS tasks — each owns one subsystem concern)                     │
├─────────────────────────────────────────────────────────────────────────┤
│                           SERVICE LAYER                                 │
│  FlightModeMgr   FaultManager   EPSMonitor   EventLogger                │
│  EKF   LQR   AttitudeControl   MomentumDump   CommInit                  │
├─────────────────────────────────────────────────────────────────────────┤
│                    DATA LAYER ABSTRACTION (DLA)                         │
│         dl_snapshot_t — single shared satellite state bus               │
│   system_state  ·  flight_mode  ·  energy_state  ·  seq counter         │
├─────────────────────────────────────────────────────────────────────────┤
│                        DRIVER / HAL LAYER                               │
│  I²C HAL   UART   IMU (MPU6050)   Magnetometer   Temperature            │
│  Watchdog HAL   ADC   GPIO   Pico SDK BSP                               │
└─────────────────────────────────────────────────────────────────────────┘
```

### 4.2 Architecture Principles

| Principle | Implementation |
|-----------|----------------|
| No dynamic allocation in flight | No `malloc`/`free`; all buffers statically allocated |
| Single shared state bus | All inter-task data via DLA (`data_layer.h`); no direct global sharing |
| Platform portability | `PICO_BUILD` preprocessor guard selects hardware vs. host-stub implementations |
| HAL isolation | Hardware access only through HAL functions; drivers never read hardware registers directly in service code |
| Fault escalation path | All error paths call `fault_manager_report()`; `FAULT_LEVEL_CRITICAL` triggers `fmm_force_safe()` |
| ISR safety | Only `fmm_force_safe()` and watchdog scratch writes are ISR-safe; all other API is task-context only |

---

## 5. Software Layer Descriptions

### 5.1 Task Layer

The task layer contains all FreeRTOS tasks. Tasks are the only entities that
block on `vTaskDelay` or queue operations. Each task has a single, bounded
responsibility:

| Task | Responsibility |
|------|----------------|
| `SensorRead` | Poll IMU and magnetometer; write raw sensor data to DLA |
| `AttitudeCtrl` | Run EKF + LQR pipeline; write attitude estimates to DLA; command actuators |
| `Telemetry` | Read DLA snapshot; serialize HK packet; transmit via CSP/KISS |
| `Command` | Receive uplink CSP packets; decode TC; call FMM/service APIs |
| `HealthMon` | Kick watchdog; check DLA for stale sensor data; report faults |

Tasks communicate exclusively through the DLA. No task-to-task queues or
semaphores are used for data transfer; these are reserved for synchronization
primitives internal to each service (e.g., the CSP router mutex).

### 5.2 Service Layer

The service layer provides stateful, reentrant services to tasks. Services are
initialized in `vStartupTask` before any task begins running. Publicly visible
APIs are defined in the corresponding header files under `include/`.

See §8 for detailed service design.

### 5.3 Data Layer Abstraction

The DLA (`src/core/data_layer.c`) is the single authoritative source for the
satellite's run-time state. It wraps a `dl_snapshot_t` structure behind a
FreeRTOS mutex to provide serialized read/write access from multiple tasks.

See §9 for detailed DLA design.

### 5.4 Driver / HAL Layer

HAL modules provide thin wrappers over the Pico SDK peripheral APIs. Each
peripheral has two implementations — one for `PICO_BUILD` and one stub for
host test builds — selected at compile time.

See §10 for the full driver inventory.

---

## 6. Module Inventory

### 6.1 Source Directory Structure

```
src/
├── obc_main.c                  ← boot entry point; task creation
├── freertos_hooks.c            ← vApplicationStackOverflowHook, idle hook
├── core/
│   ├── comm_init.c             ← CSP node + KISS UART router init
│   ├── data_layer.c            ← DLA — shared satellite state bus
│   ├── event_logger.c          ← persistent ring-buffer event log
│   ├── flash_backend_stub.c    ← flash persistence stub (host)
│   └── system_state.c          ← raw sensor state + mutex
├── services/
│   ├── fmm/flight_mode_manager.c   ← flight mode state machine
│   ├── fault/fault_manager.c       ← fault table + FDIR escalation
│   ├── eps/eps_monitor.c           ← battery voltage → energy FSM
│   ├── log/logger.c                ← log_event() API wrapper
│   ├── adcs/momentum_dump.c        ← reaction-wheel momentum unloading
│   └── watchdog/
│       ├── watchdog_hal_host.c     ← watchdog stub (host)
│       └── watchdog_hal_pico.c     ← TPS3431 kick via GPIO20 (Pico)
├── tasks/
│   ├── sensor_read_task.c          ← vSensorReadTask
│   ├── attitude_control_task.c     ← vAttitudeControlTask
│   ├── telemetry_task.c            ← vTelemetryTask
│   ├── command_task.c              ← vCommandTask
│   └── health_monitor_task.c       ← vHealthMonitorTask
├── control/
│   ├── ekf.c                   ← Extended Kalman Filter (ADCS)
│   ├── lqr.c                   ← LQR attitude controller
│   ├── lqr_schedule.c          ← gain schedule (detumble → nominal)
│   ├── pid_controller.c        ← legacy PID (FM_DIAGNOSTIC)
│   ├── attitude_control.c      ← top-level ADCS control loop
│   ├── closed_loop_sim.c       ← host simulation harness
│   └── quaternion.c            ← quaternion math utilities
├── actuators/
│   ├── magnetorquer.c          ← MTQ PWM drive (B-dot law)
│   └── reaction_wheel.c        ← RW PWM drive (LQR output)
├── dynamics/
│   └── attitude_dynamics.c     ← rigid-body dynamics (simulation)
└── drivers/
    ├── i2c/
    │   ├── host_i2c.c          ← I²C stub (host)
    │   └── pico_i2c.c          ← I²C HAL using Pico SDK
    ├── imu/
    │   └── mpu6050.c           ← MPU6050 driver (I²C)
    ├── mag/
    │   └── hmc5883l.c          ← HMC5883L / LIS3MDL magnetometer driver
    ├── temperature/
    │   ├── host_temp.c         ← temperature stub (host)
    │   └── pico_temp.c         ← RP2350 on-chip ADC temp sensor
    └── uart/
        └── pico_usart.c        ← UART1 HAL for KISS/CSP (Pico)
```

### 6.2 Module Dependency Summary

```
obc_main.c
    ├── system_state (init)
    ├── comm_init    (init)
    ├── fault_manager(init)
    ├── eps_monitor  (init)
    ├── i2c_hal      (init — Pico only)
    ├── mpu6050      (init)
    ├── temperature  (init)
    └── [xTaskCreate → 5–7 tasks]

vSensorReadTask ──reads──► mpu6050, hmc5883l, temperature
                ──writes──► DLA (system_state, mag_field)

vAttitudeControlTask ──reads──► DLA (attitude, rates, mag, mode)
                     ──runs──► EKF → LQR → actuators (MTQ/RW)
                     ──writes──► DLA (att_uncertainty, ekf_valid)

vTelemetryTask ──reads──► DLA snapshot
               ──sends──► CSP HK packet over UART1

vCommandTask  ──receives──► CSP TC from UART1
              ──calls──► fmm_request_transition(), service APIs

vHealthMonitorTask ──reads──► DLA (mode, energy, sensor validity)
                   ──calls──► watchdog_hal_kick()
                   ──calls──► fault_manager_report() (if stale)
```

---

## 7. Task Design

### 7.1 Task Inventory

| Task Name      | Function              | Priority          | Stack (words) | Stack (bytes) | Period    |
|----------------|-----------------------|-------------------|---------------|---------------|-----------|
| `vStartupTask` | Boot sequencer        | 4 → 1 (demoted)   | 2048          | 8 192         | N/A       |
| `SensorRead`   | IMU + magnetometer    | 4                 | 2048          | 8 192         | 10 Hz     |
| `AttitudeCtrl` | EKF + LQR + actuators | 3                 | 2048          | 8 192         | 10 Hz     |
| `Telemetry`    | HK downlink           | 2                 | 2048          | 8 192         | 1 Hz      |
| `Command`      | TC uplink             | 2                 | 2048          | 8 192         | event     |
| `HealthMon`    | WDT kick + fault check| 1                 | 2048          | 8 192         | 1 Hz      |
| `LEDBlink`*    | CYW43439 LED          | 1                 | 2048          | 8 192         | 1 Hz      |
| `Heartbeat`*   | USB-CDC diagnostic    | 1                 | 2048          | 8 192         | 0.5 Hz    |

\* Pico hardware build only. Not present in host test build.

All stacks are statically sized at 2048 words (8 192 bytes). The 2 048-word
size is justified by `newlib printf` with floating-point formatting, the EKF
7-state quaternion state, and the CSP packet buffer, which together exceed 4 KB peak
usage on `AttitudeCtrl`. See OBC-DES-001 §15 for HWM measurements.

### 7.2 Priority Rationale

| Priority | Tasks | Rationale |
|----------|-------|-----------|
| 4 (highest) | `SensorRead`, `vStartupTask` | Sensor data must be freshest; startup must complete before any other task runs at full rate |
| 3 | `AttitudeCtrl` | Control loop must execute at ≥ 10 Hz; pre-empts telemetry and command |
| 2 | `Telemetry`, `Command` | Downlink and uplink are equal priority; both yield when control needs to run |
| 1 (lowest) | `HealthMon`, `LEDBlink`, `Heartbeat` | Watchdog kick only needs to run within the WDT window (not time-critical) |

The idle task runs at priority 0. `configUSE_PREEMPTION = 1` ensures the highest
runnable task always executes.

### 7.3 SensorRead Task (`src/tasks/sensor_read_task.c`)

- Calls `mpu6050_read()` and `hmc5883l_read()` every 100 ms (10 Hz).
- On success: writes `dl_write_state()` with fresh attitude, rates, and mag data.
- On failure: calls `fault_manager_report(FAULT_IMU_TIMEOUT, FAULT_LEVEL_ERROR)`.
- Does **not** perform EKF integration — raw sensor data only.

### 7.4 AttitudeCtrl Task (`src/tasks/attitude_control_task.c`)

- Reads `dl_read_snapshot()` to get latest sensor data and current flight mode.
- Runs EKF integration (`ekf_update()`) at 10 Hz.
- Selects control law based on flight mode:
  - `FM_DETUMBLE`: B-dot law via `magnetorquer_set_torque()`
  - `FM_NOMINAL`: LQR via `lqr_compute()` → `reaction_wheel_set_speed()`
  - `FM_DIAGNOSTIC`: PID via `pid_compute()`
  - `FM_SAFE` / `FM_BOOT`: zero actuator output
- Writes EKF estimate back to DLA after each cycle.

### 7.5 Telemetry Task (`src/tasks/telemetry_task.c`)

- Runs at 1 Hz (`vTaskDelay(pdMS_TO_TICKS(1000))`).
- Calls `dl_read_snapshot()` to get a consistent state copy.
- Serializes a fixed-format HK packet (see table below) via CSP over KISS-framed UART1 (`comm_init`-managed routing).

**HK Telemetry Packet Fields** (CSP port 10, fixed layout, little-endian):

| # | Field | Type | Size | Source |
|---|-------|------|------|--------|
| 1 | `timestamp_ms` | `uint32_t` | 4 B | `xTaskGetTickCount()` |
| 2 | `flight_mode` | `uint8_t` | 1 B | `dl_snapshot.mode` |
| 3 | `energy_state` | `uint8_t` | 1 B | `dl_snapshot.energy` |
| 4 | `roll_rad` | `float` | 4 B | `dl_snapshot.state.attitude[0]` |
| 5 | `pitch_rad` | `float` | 4 B | `dl_snapshot.state.attitude[1]` |
| 6 | `yaw_rad` | `float` | 4 B | `dl_snapshot.state.attitude[2]` |
| 7 | `rate_x_rads` | `float` | 4 B | `dl_snapshot.state.rates[0]` |
| 8 | `rate_y_rads` | `float` | 4 B | `dl_snapshot.state.rates[1]` |
| 9 | `rate_z_rads` | `float` | 4 B | `dl_snapshot.state.rates[2]` |
| 10 | `battery_v` | `float` | 4 B | `dl_snapshot.state.battery_v` |
| 11 | `fault_flags` | `uint32_t` | 4 B | Active fault bitmask |
| 12 | `imu_valid` | `uint8_t` | 1 B | `dl_snapshot.state.imu_valid` |
| 13 | _padding_ | — | 3 B | Alignment |
| **Total** | | | **42 B** | |

### 7.6 Command Task (`src/tasks/command_task.c`)

- Blocks on CSP receive on port 20.
- Dispatches received TCs to service APIs:
  - Mode change → `fmm_request_transition()`
  - Parameter set → subsystem service functions
  - Log dump → `log_read_recent()`
- ACKs each successfully executed TC with a CSP reply packet on the same port:
  - `result` (1 B): `0x00` = accepted, `0x01` = rejected, `0x02` = invalid.
  - `echo_seq` (2 B): sequence number mirrored from the received TC header.
  - `reason` (1 B, on rejection): `fmm_result_t` or driver error code.

### 7.7 HealthMon Task (`src/tasks/health_monitor_task.c`)

- Runs at 1 Hz.
- Calls `eps_monitor_tick()` — evaluates the Schmidt-trigger voltage FSM and
  updates the DLA `energy_state` field (see §8.3).
- Kicks the hardware watchdog via `watchdog_hal_kick()`.
- Reads DLA to verify sensor data freshness (checks `imu_valid`, `mag_valid`).
- If sensor data is older than 2 s (`seq` counter stale): raises
  `FAULT_LEVEL_ERROR` fault.
- If energy state is `ENERGY_CRITICAL`: raises `FAULT_LEVEL_CRITICAL`.

---

## 8. Service Layer Design

### 8.1 Flight Mode Manager (FMM)

**Source**: `src/services/fmm/flight_mode_manager.c`  
**Header**: `include/flight_mode.h`  
**Full design**: `FMM-DES-001`

The FMM implements the five-state flight mode machine. Transition conditions
are shown on each arc; `fmm_force_safe()` is the unconditional FDIR path.

```
FM_BOOT(0) ─[boot done]─► FM_SAFE(1) ─[GS cmd]─► FM_DETUMBLE(2)
                              ▲                          │
         fmm_force_safe()     │              [ω < 2 °/s + GS cmd]
         (FDIR / ISR)         │                          ▼
                              ├──────────────────── FM_NOMINAL(3)
                              │                       ↕ [GS cmd]
                              └──────────────── FM_DIAGNOSTIC(4)
```

**Transition conditions:**

| From | To | Condition |
|------|----|-----------|
| FM_BOOT | FM_SAFE | Boot sequence complete (`vStartupTask` subsystem init done) |
| FM_SAFE | FM_DETUMBLE | Ground command; or autonomous LEOP timeout |
| FM_DETUMBLE | FM_NOMINAL | Ground command after ω ≤ 2 °/s (MO-2) |
| FM_NOMINAL | FM_DIAGNOSTIC | Ground command only |
| FM_DIAGNOSTIC | FM_NOMINAL | Ground command only |
| Any | FM_SAFE | `fmm_force_safe()` — FDIR CRITICAL fault or unconditional GS command |

**Key APIs:**

| Function | Description |
|----------|-------------|
| `flight_mode_manager_init()` | Set initial state `FM_BOOT`; clear transition log |
| `fmm_get_mode()` | Thread-safe read of current mode (critical section) |
| `fmm_request_transition(target)` | Evaluate allowed-transition matrix; log transition if accepted |
| `fmm_force_safe()` | Bypass matrix; ISR-safe; always accepted |
| `fmm_mode_name(mode)` | String name for logging |

The allowed-transition matrix is a compile-time `const bool[FM_COUNT][FM_COUNT]`
array. `FM_SAFE` is reachable from every mode via `fmm_force_safe()` regardless
of the matrix.

### 8.2 Fault Manager

**Source**: `src/services/fault/fault_manager.c`  
**Header**: `include/fault_manager.h`  
**Full design**: `FAULT-DES-001`

Maintains a static fault table (one `fault_event_t` entry per `fault_id_t`).
Each entry records: id, level, timestamp, occurrence count, active flag.

**Fault escalation path:**

```
fault_manager_report(id, FAULT_LEVEL_CRITICAL)
         │
         ├── sets fault_table[id].active = true
         ├── calls log_event(LOG_EVT_FAULT_CRITICAL, LOG_CLASS_CRITICAL, ...)
         └── calls fmm_force_safe()
```

`FAULT_LEVEL_WARNING` and `FAULT_LEVEL_ERROR` are logged but do not trigger a
mode change.

**Fault IDs** are defined in `include/fault_ids.h`. Current categories:

| Prefix | Subsystem |
|--------|-----------|
| `FAULT_IMU_*`  | IMU sensor faults |
| `FAULT_MAG_*`  | Magnetometer faults |
| `FAULT_EPS_*`  | EPS / battery faults |
| `FAULT_COMM_*` | Communications faults |
| `FAULT_FMM_*`  | Flight mode manager faults |
| `FAULT_WDT_*`  | Watchdog faults |

### 8.3 EPS Monitor

**Source**: `src/services/eps/eps_monitor.c`  
**Header**: `include/eps.h`  
**Full design**: `EPS-DES-001`

Implements a two-threshold Schmidt trigger on battery voltage to derive the
`energy_state_t` enumeration:

| State | Voltage Threshold | Behavior |
|-------|-------------------|-----------|
| `ENERGY_NOMINAL`  | Vbatt > 7.0 V (rising) | Full power mode |
| `ENERGY_LOW`      | Vbatt < 7.2 V (falling) | Non-essential loads shed |
| `ENERGY_CRITICAL` | Vbatt < 6.6 V (falling) | All non-OBC loads shed; fault raised |

The monitor reads `battery_v` from the DLA (written by `SensorRead` from ADC0).
`eps_monitor_tick()` is called by `vHealthMonitorTask` at 1 Hz
(`src/tasks/health_monitor_task.c` line 33), which ensures periodic### 8.5 Extended Kalman Filter (EKF)
 
 **Source**: `src/control/ekf.c`  
 **Header**: `include/ekf.h`  
 **Full design**: `ADCS-DES-001`
 
 7-state EKF (attitude quaternion 4D + gyro bias 3-DOF):
 
 - **Predict step**: integrates gyro rates using quaternion kinematics via RK2 midpoint.
 - **Update step**: fuses accelerometer and magnetometer vectors to correct attitude drift.
 - **Output**: attitude quaternions + derived Euler angles (roll/pitch/yaw, radians) + uncertainty
   diagonal `P` written to DLA.
 
 EKF is called by `vAttitudeControlTask` at 10 Hz. Convergence flag
 `imu_ekf_valid` is set when `max(diag(P))` falls below convergence threshold.

### 8.4 Event Logger

**Source**: `src/core/event_logger.c`, `src/services/log/logger.c`
**Header**: `include/logger.h`
**Full design**: `DL-DES-001`

Three-class storage model:

| Class | Enum | Storage | Overwrite Policy |
|---|---|---|---|
| A — Critical | `LOG_CLASS_CRITICAL` | Protected flash segment | Never overwritten |
| B — Operational | `LOG_CLASS_OPERATIONAL` | Operational log segment | Overwritten when segment full |
| C — Info | `LOG_CLASS_INFO` | RAM ring buffer (capacity 64) → flushed to flash | Circular; oldest overwritten |

Each `log_event_t` record is exactly **40 bytes**:
`timestamp_ms(4) + event_id(2) + severity(1) + subsystem(1) + data[32]`.

The flash backend is implemented in `src/core/flash_backend.c` (ACT-16, 2026-03-10).
On Pico builds it uses the Pico SDK `hardware/flash.h` (`flash_range_erase` +
`flash_range_program`) with interrupt protection. The log region occupies the
top 16 KB of the 2 MB internal flash (`0x1FC000`–0x1FFFFF`), using a 4-sector
round-robin scheme with a 16-byte header (magic `OBCLOGV1` + CRC-32 + length).
A recovery path `flash_backend_recover()` replays valid sectors on boot.
Host builds retain the stub in `flash_backend_stub.c`. See SYS-F-304 (`[IMPL]`).

### 8.5 Extended Kalman Filter (EKF)

**Source**: `src/control/ekf.c`  
**Header**: `include/ekf.h`  
**Full design**: `ADCS-DES-001`

7-state EKF (attitude quaternion 4D + gyro bias 3-DOF):

- **Predict step**: integrates gyro rates using quaternion kinematics via RK2 midpoint.
- **Update step**: fuses accelerometer and magnetometer vectors to correct attitude drift.
- **Output**: attitude quaternions + derived Euler angles (roll/pitch/yaw, radians) + uncertainty
  diagonal `P` written to DLA.

EKF is called by `vAttitudeControlTask` at 10 Hz. Convergence flag
`imu_ekf_valid` is set when `max(diag(P))` falls below convergence threshold.

### 8.6 LQR Controller

**Source**: `src/control/lqr.c`, `src/control/lqr_schedule.c`  
**Header**: `include/lqr.h`  
**Full design**: `ADCS-DES-001`

State-feedback controller for three-axis nadir pointing in `FM_NOMINAL`.
Gain matrix **K** (3×6) is computed offline and stored as a compile-time
constant. `lqr_schedule.c` provides a gain-scheduling table for the transition
from detumble to nominal pointing.

### 8.7 Momentum Dump

**Source**: `src/services/adcs/momentum_dump.c`  
**Header**: `include/momentum_dump.h`

Implements cross-product momentum unloading: when reaction-wheel momentum
approaches saturation, fires magnetorquers to dump stored angular momentum
per the cross-product law $\mathbf{\tau}_{MTQ} = k\,(\mathbf{h}_{RW} \times \mathbf{B})$.
Triggered autonomously by `AttitudeCtrl` in `FM_NOMINAL`.

### 8.8 Communications Init

**Source**: `src/core/comm_init.c`  
**Header**: `include/comm_init.h`  
**Full design**: `COMMS-DES-001`

Initializes the CSP node, registers the UART1 KISS interface, and starts the
CSP router task. After `comm_init()` returns, `Telemetry` and `Command` tasks
can use `csp_send()` and `csp_recv()` respectively.

---

## 9. Data Layer Abstraction (DLA)

**Source**: `src/core/data_layer.c`  
**Header**: `include/data_layer.h`  
**Full design**: `DL-DES-001`

### 9.1 Shared State Structure

```c
typedef struct {
    system_state_t state;   // sensor data: attitude [rad], rates [rad/s],
                            // temperature [°C], validity flags
    flight_mode_t  mode;    // current flight mode (from FMM)
    energy_state_t energy;  // current energy state (from EPS Monitor)
    uint32_t       seq;     // write counter — readers detect staleness
} dl_snapshot_t;
```

### 9.2 DLA API

| Function | Access | Description |
|----------|--------|-------------|
| `dl_init()` | Init | Create mutex; zero state; set `seq = 0` |
| `dl_write_state(const system_state_t*)` | Write | Lock mutex; copy state; `seq++`; unlock |
| `dl_write_energy_state(energy_state_t)` | Write | Lock mutex; update energy; `seq++`; unlock |
| `dl_read_snapshot(dl_snapshot_t*)` | Read | Lock mutex; copy snapshot to caller; unlock |

### 9.3 Staleness Detection

Readers compare the `seq` value of two successive snapshots. If `seq` has
not incremented within an expected window (typically 2× the `SensorRead`
period = 200 ms), `HealthMon` raises a `FAULT_LEVEL_ERROR` fault.

### 9.4 Units Convention

Per `SPEC-2-DLA §2.4` (referenced in `data_layer.h`):

| Quantity | Unit |
|----------|------|
| Attitude angles | radians |
| Angular rates | rad/s |
| Temperature | °C |
| Voltage | V |
| Magnetic field | µT |

---

## 10. Driver and HAL Layer

### 10.1 I²C HAL

| File | Target | Description |
|------|--------|-------------|
| `src/drivers/i2c/pico_i2c.c` | Pico | Wraps `i2c_write_blocking` / `i2c_read_blocking` from Pico SDK |
| `src/drivers/i2c/host_i2c.c` | Host | Returns mock data for unit tests |

I²C0 bus runs at 400 kHz on GPIO 4 (SDA) / GPIO 5 (SCL) per `pico_pins.h`.
> **Resolved (OI-1)**: `config.h` harmonized with `pico_pins.h` (4/5) on 2026-03-11.

### 10.2 IMU Driver — MPU6050

**Source**: `src/drivers/imu/mpu6050.c`  
**Header**: `include/drivers/imu/mpu6050.h`

| Function | Description |
|----------|-------------|
| `mpu6050_init()` | Wake up MPU6050; configure 1 kHz gyro ODR; verify WHO_AM_I |
| `mpu6050_read()` | Read 14 bytes (accel + temp + gyro); convert to SI; write to `system_state` |

### 10.3 Magnetometer Driver — HMC5883L / LIS3MDL

**Source**: `src/drivers/mag/hmc5883l.c`  
**Header**: `include/drivers/mag/hmc5883l.h`

Driver supports both HMC5883L (GY-271) and LIS3MDL (same I²C register map
subset used). Configured for 75 Hz ODR; outputs calibrated field in µT.

### 10.4 Temperature Driver

| File | Target | Description |
|------|--------|-------------|
| `src/drivers/temperature/pico_temp.c` | Pico | RP2350 on-chip ADC4 temperature sensor |
| `src/drivers/temperature/host_temp.c` | Host | Returns constant 25.0 °C for tests |

### 10.5 UART Driver — KISS/CSP

**Source**: `src/drivers/uart/pico_usart.c`  
Interface used by `comm_init` for KISS-framed CSP over UART1 at 9600 baud
(E22-400M30S radio interface, GPIO 8 TX / GPIO 9 RX per `pico_pins.h`).

### 10.6 Watchdog HAL

| File | Target | Description |
|------|--------|-------------|
| `src/services/watchdog/watchdog_hal_pico.c` | Pico | Kicks TPS3431 external WDT via GPIO20 toggle |
| `src/services/watchdog/watchdog_hal_host.c` | Host | No-op stub |

Stack overflow hook (`freertos_hooks.c`) writes the offending task name to
`watchdog_hw->scratch[1..3]` and a magic value `0xDEAD0001` to `scratch[0]`
before triggering a reset. On next boot, `obc_main.c` reads `scratch[0]` and
prints the task name.

---

## 11. Boot Sequence

### 11.1 Initialization Call Graph

```
main()
  │
  ├── [Pico only] stdio_init_all(), cyw43_arch_init()
  │
  ├── xTaskCreate(vStartupTask, priority=4)   ← ONLY task created in main()
  │
  └── vTaskStartScheduler()                   ← FreeRTOS takes over
            │
            └── vStartupTask() [priority 4]
                  │
                  ├── system_state_init()     ← zero state + mutex
                  ├── comm_init()             ← CSP node + KISS UART + router task
                  ├── fault_manager_init()    ← zero fault table
                  ├── eps_monitor_init()      ← init energy FSM
                  ├── [Pico] i2c_bus_init()   ← configure I²C0 at 400 kHz
                  ├── mpu6050_init()          ← IMU power-on self-test
                  ├── temperature_init()      ← ADC4 enable
                  ├── system_state_set_available(imu_ok, temp_ok)
                  │
                  ├── xTaskCreate(vSensorReadTask,   priority=4)
                  ├── xTaskCreate(vAttitudeControlTask, priority=3)
                  ├── xTaskCreate(vTelemetryTask,    priority=2)
                  ├── xTaskCreate(vCommandTask,      priority=2)
                  ├── xTaskCreate(vHealthMonitorTask, priority=1)
                  ├── [Pico] xTaskCreate(vLedBlinkTask, priority=1)
                  ├── [Pico] xTaskCreate(vHeartbeatTask, priority=1)
                  │
                  ├── vTaskPrioritySet(NULL, priority=1)   ← demote Startup
                  │
                  └── ALIVE loop [priority 1]
                        └── periodically prints heap + HWM stats
```

### 11.2 Rationale for Startup Inside Task

All FreeRTOS synchronizations objects (queues, mutexes) used by `comm_init`,
`data_layer_init`, etc. are created inside `vStartupTask`, **not** in `main()`.
On the RP2350 `ARM_CM33_NTZ` SMP port, the kernel spinlocks that protect
`taskENTER_CRITICAL` are not initialized until `vTaskStartScheduler()` runs.
Creating sync objects before the scheduler starts causes a spinlock deadlock
on hardware. See `OBC-DES-001 §11.3`.

---

## 12. Inter-Task Communication

### 12.1 Communication Mechanisms

| Mechanism | Used For | Notes |
|-----------|----------|-------|
| DLA (`data_layer.h`) | All sensor data, flight mode, energy state | Primary data bus; mutex-protected |
| CSP sockets | Telemetry TX (port 10), TC RX (port 20) | `libcsp` manages internal queues |
| Direct API calls | FMM transitions, fault reports | Synchronous; protected by critical sections |
| `printf` / USB CDC | Diagnostic logging (Pico only) | Not flight-critical; no mutex needed |

### 12.2 No Direct Task-to-Task Queues

Tasks do not share FreeRTOS queues for data transfer. All data sharing is
mediated by the DLA. This design choice avoids priority inversion between
producers and consumers of sensor data and simplifies task synchronization.

---

## 13. Memory Management

### 13.1 Static Allocation Policy

No dynamic memory allocation (`malloc` / `free`) is used in flight code.
The only heap consumer is the FreeRTOS heap used at initialization for:
- Task stacks (allocated by `xTaskCreate` internally)
- CSP internal queues and router state (allocated during `comm_init`)

### 13.2 SRAM Memory Regions

The RP2350 provides **520 KB** of on-chip SRAM organised as six physical banks.
The Pico SDK accesses them via a striped alias at `0x20000000` (interleaved
across SRAM0–SRAM3 for bandwidth) and an unstriped alias at `0x21000000`.

**Physical SRAM bank map (RP2350):**

| Bank | Base Address | Size | Alias | Default Use |
|------|-------------|------|-------|-------------|
| SRAM0 | `0x20000000` | 128 KB | Striped (Core 0 / Core 1 interleaved) | `.data`, `.bss`, FreeRTOS heap, stack pool |
| SRAM1 | `0x20020000` | 128 KB | Striped | Continuation of heap / task stacks |
| SRAM2 | `0x20040000` | 128 KB | Striped | Continuation; CORE1 stack tail |
| SRAM3 | `0x20060000` | 128 KB | Striped | Reserved / future expansion |
| SRAM4 | `0x20080000` | 4 KB | Unstriped (scratch) | USB + DMA descriptors (Pico SDK) |
| SRAM5 | `0x20081000` | 4 KB | Unstriped (scratch) | USB + DMA descriptors (Pico SDK) |

**Linker-level regions (`memmap_default.ld`):**

| Region | Address Range | Size | FSW Usage |
|--------|--------------|------|-----------|
| Flash XIP | `0x10000000` – `0x101FFFFF` | 2 MB | `.text`, `.rodata`, vector table, constants; NOR log backend (Phase 3) |
| SRAM `.data` / `.bss` | `0x20000000` + | ~30 KB (measured) | Globals, static buffers, FreeRTOS scheduler data structures |
| FreeRTOS heap (`heap_4`) | (static array inside `.bss`) | 128 KB configured | Task TCBs, task stacks, CSP queues, mutexes |
| SRAM remaining | — | ~360 KB | IRQ stack, Pico SDK runtime, CORE1 stack, future `xTaskCreateStatic` pools |

On the **host (Linux/x86) build**, task stacks are backed by the system
allocator (`heap_3.c`), so `xPortGetFreeHeapSize()` reflects available
virtual memory rather than a fixed 60 KB pool.

### 13.3 Heap Budget

| Consumer | Build | Allocation | Notes |
|----------|-------|------------|-------|
| Startup task stack | Both | 8 192 B | `vStartupTask`; exits after init |
| SensorRead stack | Both | 8 192 B | |
| AttitudeCtrl stack | Both | 8 192 B | EKF + LQR matrices |
| Telemetry stack | Both | 8 192 B | |
| Command stack | Both | 8 192 B | |
| HealthMon stack | Both | 8 192 B | |
| LEDBlink stack | Pico only | 8 192 B | GPIO LED blink |
| Heartbeat stack | Pico only | 8 192 B | CAN heartbeat |
| CSP router + queues | Both | ~4 096 B | `libcsp` internal alloc during `comm_init` |
| **Total (Pico build)** | **Pico** | **~82 KB** | All tasks + CSP fit within 128 KB |
| **Total (host build)** | **Host** | **~5 KB** | Host: 5 tasks + CSP; measured free heap 60 416 B |
| **Heap configured** | Both | **128 KB** | `configTOTAL_HEAP_SIZE` in `config/FreeRTOSConfig.h` |

### 13.4 Stack High Water Marks (HWM)

From hardware measurement (OBC-DES-001 §15). HWM = remaining free words
(higher is better; 2048 word stacks):

| Task | HWM (words) | Used (words) | Headroom |
|------|-------------|--------------|----------|
| SensorRead | 1 880 | 168 | 91.8% |
| AttitudeCtrl | 1 820 | 228 | 88.9% |
| Telemetry | 1 910 | 138 | 93.3% |
| Command | 1 950 | 98 | 95.2% |
| HealthMon | 1 960 | 88 | 95.7% |

All tasks have ≥ 88% stack headroom. Stack overflow detection is enabled
(`configCHECK_FOR_STACK_OVERFLOW = 2`).

---

## 14. CPU Budget Estimate

> **Status**: Estimates based on WCET analysis of algorithm complexity and
> measured cycle counts on Cortex-M33 @ 133 MHz. Profiling on hardware is
> tracked under OI-3.

### 14.1 Task CPU Load (Core 0, 133 MHz)

| Task | Period (ms) | Est. WCET (ms) | Est. CPU Load |
|------|------------|----------------|---------------|
| SensorRead | 100 | 3.0 | 3.0% |
| AttitudeCtrl | 100 | 8.0 | 8.0% |
| Telemetry | 1 000 | 4.0 | 0.4% |
| Command | event-driven | 1.0 | < 0.1% |
| HealthMon | 1 000 | 2.0 | 0.2% |
| LEDBlink (Pico) | 500 | 0.1 | < 0.1% |
| Heartbeat (Pico) | 1 000 | 0.5 | < 0.1% |
| **Total estimate** | — | — | **~12–13%** |

### 14.2 Caveats and Assumptions

- WCET estimates assume: IMU I²C transfer at 400 kHz (SensorRead), EKF
  propagation + LQR update (AttitudeCtrl).
- **Core 1** is idle in the CDR single-core baseline (OI-4); all load is on
  Core 0.
- FreeRTOS scheduler overhead (~1–2%) and interrupt service routines (WDT,
  UART RX) are not included; total system load remains well under 20%.
- Margin at CDR: ~80 MIPS headroom against 133 MIPS Core 0 rated frequency.
- Hardware profiling using DWT cycle counters is planned for Phase 2 FM
  qualification testing (OI-3).

---

## 15. Error Handling and FDIR Integration

### 15.1 General Error Handling Policy

- All driver init functions return `int` (0 = success, negative = error).
- All error paths inside tasks call `fault_manager_report()` — never silently
  discard errors.
- No `assert()` in flight code; assertions are replaced by fault reports.

### 15.2 Fault Severity Mapping

| Event | Level | Action |
|-------|-------|--------|
| IMU read timeout | `FAULT_LEVEL_ERROR` | Log; HK alarm flag; no mode change |
| Magnetometer lost | `FAULT_LEVEL_ERROR` | Log; EKF continues with gyro only |
| Battery voltage < 6.6 V | `FAULT_LEVEL_CRITICAL` | Log (Class A); `fmm_force_safe()` |
| Stack overflow detected | `FAULT_LEVEL_CRITICAL` | Watchdog scratch write → reset |
| CSP router init failed | `FAULT_LEVEL_CRITICAL` | Log; `fmm_force_safe()` |

### 15.3 Watchdog Recovery

The `HealthMon` task kicks the TPS3431 external watchdog every 1 s.
If `HealthMon` is blocked (stack overflow, deadlock, priority inversion),
the WDT fires after its timeout, resetting the OBC:

```
WDT fires
   │
   └── RP2350 reset
         │
         └── main() on next boot:
               reads watchdog_hw->scratch[0]
               if scratch[0] == 0xDEAD0001:
                 prints "WDT reset — offending task: <name from scratch[1..3]>"
               then proceeds with normal boot
```

This mechanism provides crash diagnosis without requiring an external debugger.

### 15.4 ISR Safety

The following functions are safe to call from ISR context:

| Function | Reason |
|----------|--------|
| `fmm_force_safe()` | Uses `taskENTER_CRITICAL_FROM_ISR()` internally |
| `watchdog_hal_kick()` | Register write only |
| `scratch[*]` writes | Direct register access |

All other service and DLA APIs must be called from task context only.

---

## 16. Build System and Platform Portability

### 16.1 CMake Build Configurations

| Build Dir | Target | `PICO_BUILD` | Purpose |
|-----------|--------|-------------|---------|
| `build/` | x86_64 Linux | OFF | Unit tests (29 tests) |
| `build_pico/` | RP2350 | ON | Flash-ready UF2 |
| `build_emu/` | x86_64 (emulation) | ON (partial) | Emulator tests |

### 16.2 Platform Guard Pattern

Every hardware-dependent file pair uses a `PICO_BUILD` compile-time guard:

```cmake
# src/drivers/i2c/CMakeLists.txt
if(PICO_BUILD)
    target_sources(obc_drivers PRIVATE pico_i2c.c)
else()
    target_sources(obc_drivers PRIVATE host_i2c.c)
endif()
```

This ensures the same source tree produces both a fully testable host binary
and a hardware-flashed Pico image without `#ifdef` pollution in business logic.

### 16.3 Host vs Target Differences

| Aspect | Host (test build) | Pico (target build) |
|--------|-------------------|---------------------|
| I²C | Mock (returns constant data) | Hardware I²C0 via Pico SDK |
| UART / CSP | Loopback or TCP socket | UART1 (GPIO8/9) |
| Watchdog | No-op | GPIO20 → TPS3431 |
| Temperature | Returns 25.0 °C | RP2350 ADC4 on-chip sensor |
| LED | No-op | CYW43439 GPIO |
| Flash logger | RAM ring buffer | NOR flash stub (OI-4) |
| FreeRTOS port | `GCC/ARM_CM33_NTZ` (host posix wrapper) | `GCC/ARM_CM33_NTZ` (hardware) |

---

## 17. Traceability to Requirements

| SRS Requirement | Satisfied By | Design Ref |
|-----------------|--------------|------------|
| SRS-F-101 (IMU @ ≥ 10 Hz) | `vSensorReadTask` (10 Hz loop) | §7.3 |
| SRS-F-201 (EKF attitude) | `vAttitudeControlTask` + `ekf.c` | §7.4, §8.5 |
| SRS-F-301 (LQR pointing) | `lqr.c` + `reaction_wheel.c` | §7.4, §8.6 |
| SRS-F-401 (HK at 1 Hz) | `vTelemetryTask` | §7.5 |
| SRS-F-501 (TC on CSP port 20) | `vCommandTask` | §7.6 |
| SRS-D-001 (FDIR CRITICAL → FM_SAFE) | `fault_manager.c` + `fmm_force_safe()` | §8.1, §8.2 |
| SRS-D-002 (WDT recovery ≤ 10 s) | `watchdog_hal_pico.c` + boot check | §10.6, §11.1 |
| SRS-D-003 (log CRITICAL to flash before FM_SAFE) | `fault_manager_report()` → `log_event(CLASS_CRITICAL)` | §8.2, §8.4 |
| SRS-D-005 (OBC rail uninterruptible) | EPS FSM only sheds payload rail | §8.3 |
| MIS-PB-001 (battery ≥ 37 min) | EPS load shedding at ENERGY_CRITICAL | §8.3 |
| MIS-DB-002 (≥ 320 events stored) | Logger ring buffer capacity 64 → flash flush | §8.4 |

Full traceability matrix is in `RTM-OBC-001`.

---

## 18. Open Items

| OI | Description | Priority | Linked Doc | Status |
|----|-------------|----------|------------|--------|
| OI-1 | I²C pin conflict: `config.h` (GPIO 16/17) vs. `pico_pins.h` (GPIO 4/5) — must resolve before hardware validation | High | OBC-DES-001 OI-6 | Open |
| OI-2 | Flash backend implemented in `src/core/flash_backend.c` (SRR-OBC-001 ACT-16, 2026-03-10): Pico SDK `hardware_flash`, 4-sector round-robin at `0x1FC000`, CRC-32 header, `flash_backend_recover()` for boot replay. SYS-F-304 → `[IMPL]`. | High | OBC-DES-001 OI-4, DL-DES-001 | **CLOSED** |
| OI-3 | `AttitudeCtrl` WCET not yet measured via DWT cycle counter; required for timing budget sign-off | High | OBC-DES-001 OI-3 | Open |
| OI-4 | SMP (Core 1) disabled pending HIL boot stability test; **CDR baseline = single-core operation on Core 0**; dual-core enable planned for v1.0.0 | Medium | OBC-DES-001 OI-1, RMP-OBC-001 RISK-SW-001 | Open |
| OI-5 | Momentum dump trigger threshold not formally verified against RW saturation spec | Medium | ADCS-DES-001 | Open |
| OI-6 | FMEA-OBC-001 not yet written; fault table in fault_ids.h is the interim hazard input source | Medium | MRD-OBC-001 §6.2.1 | Open |
| OI-7 | `vStartupTask` ALIVE loop remains alive post-init at priority 1 — should be replaced by a proper idle monitor or deleted; tracked for v1.0.0 | Low | SAD-OBC-001 | Open |
| OI-8 | Pico hardware build requires ~82 KB FreeRTOS heap (10 tasks × 8 KB + TCBs + CSP ~4 KB) but `configTOTAL_HEAP_SIZE = 60 KB`; host build unaffected (5 tasks, heap_3). Resolution: increase to ≥ 96 KB or migrate to `xTaskCreateStatic` | **High** | config/FreeRTOSConfig.h | Open |

---

## 19. References

| Ref | Document |
|-----|----------|
| [1] | SAD-OBC-001 v1.0 — System Architecture Document |
| [2] | OBC-DES-001 v0.1 — OBC Hardware & CDH Design Document |
| [3] | ADCS-DES-001 v1.0 — ADCS Design Document |
| [4] | FMM-DES-001 v0.3 — Flight Mode Manager Design Document |
| [5] | EPS-DES-001 v0.1 — Electrical Power System Design Document |
| [6] | FAULT-DES-001 v0.2 — Fault Manager Design Document |
| [7] | COMMS-DES-001 v0.1 — Communications / TT&C Design Document |
| [8] | DL-DES-001 v0.1 — Data Layer / Storage Design Document |
| [9] | ICD-OBC-001 v1.1 — Interface Control Document |
| [10] | BOM-OBC-001 v1.0.1 — Bill of Materials |
| [11] | MRD-OBC-001 v1.1 — Mission Requirements Document |
| [12] | SRS-OBC-001 v2.0 — Software Requirements Specification |
| [13] | RTM-OBC-001 — Requirements Traceability Matrix |
| [14] | RMP-OBC-001 v1.0 — Risk Management Plan |
| [15] | ECSS-E-ST-40C — Software Engineering Standard |
