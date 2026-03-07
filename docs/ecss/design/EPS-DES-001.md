# EPS-DES-001 — Electrical Power System Monitor Design Document

| Field            | Value                                      |
|------------------|--------------------------------------------|
| **Document ID**  | EPS-DES-001                                |
| **Title**        | Electrical Power System Monitor Design Document |
| **Project**      | CubeSat OBC — RP2350 / Pico 2W             |
| **Subsystem**    | Electrical Power System (EPS)              |
| **Version**      | 0.1                                        |
| **Status**       | Draft                                      |
| **Date**         | 2026-03-07                                 |
| **Author**       | OBC Software Team                          |
| **Review Level** | PDR                                        |
| **Standard**     | ECSS-E-ST-40C, ECSS-Q-ST-80C              |

---

## Change History

| Version | Date       | Author          | Description                        |
|---------|------------|-----------------|------------------------------------|
| 0.1     | 2026-03-07 | OBC SW Team     | Initial draft                      |

---

## Table of Contents

1. [Introduction](#1-introduction)
2. [Applicable Documents](#2-applicable-documents)
3. [Acronyms and Definitions](#3-acronyms-and-definitions)
4. [System Overview](#4-system-overview)
5. [Energy State Model](#5-energy-state-model)
6. [Voltage Thresholds and Hysteresis](#6-voltage-thresholds-and-hysteresis)
7. [Schmidt-Trigger Algorithm](#7-schmidt-trigger-algorithm)
8. [Power Rail Management](#8-power-rail-management)
9. [FDIR Chain and Fault Reporting](#9-fdir-chain-and-fault-reporting)
10. [Data Layer Interface](#10-data-layer-interface)
11. [Public API](#11-public-api)
12. [Hardware Abstraction Layer](#12-hardware-abstraction-layer)
13. [Concurrency and Locking](#13-concurrency-and-locking)
14. [Telemetry](#14-telemetry)
15. [Event Logging](#15-event-logging)
16. [Test Coverage](#16-test-coverage)
17. [Open Items](#17-open-items)

---

## 1. Introduction

### 1.1 Purpose

This document defines the software design of the EPS Monitor service
(`eps_monitor.c`) for the CubeSat OBC.  The EPS Monitor is responsible for:

- Periodically reading battery voltage, current, and temperature from the
  hardware abstraction layer (HAL).
- Classifying the bus energy level into one of four discrete states using a
  Schmidt-trigger hysteresis model.
- Publishing the energy state to the Data Layer (`data_layer.h`) for consumption
  by all other tasks.
- Triggering autonomous fault reports through the Fault Manager, which in turn
  may request a flight-mode change via the FMM.

### 1.2 Scope

This document covers the software-only design of the EPS Monitor service.
Hardware design of the EPS board (voltage regulators, battery chemistry, charge
controller IC) is out of scope and is covered in `BOM-OBC-001`.

### 1.3 Document Identifier

`EPS-DES-001 v0.1`

---

## 2. Applicable Documents

| ID            | Title                                              | Version |
|---------------|----------------------------------------------------|---------|
| SyRS-OBC-001  | System Requirements Specification                  | latest  |
| SRS-OBC-001   | Software Requirements Specification                | latest  |
| ICD-OBC-001   | Interface Control Document                         | v1.1    |
| BOM-OBC-001   | Bill of Materials                                  | v1.0.1  |
| FMM-DES-001   | Flight Mode Manager Design Document                | v0.3    |
| SAD-OBC-001   | System Architecture Document                       | v1.0    |
| SPEC-2-EPS    | EPS Electrical Specification                       | v1.14   |

---

## 3. Acronyms and Definitions

| Term          | Definition                                                   |
|---------------|--------------------------------------------------------------|
| EPS           | Electrical Power System                                      |
| FDIR          | Failure Detection, Isolation and Recovery                    |
| FMM           | Flight Mode Manager                                          |
| HAL           | Hardware Abstraction Layer                                   |
| HK            | Housekeeping                                                 |
| Schmidt-trigger | Hysteresis comparator: different thresholds for rising/falling transitions |
| V_batt        | Battery bus voltage (nominally 7.4 V)                        |
| DLA           | Data Layer Abstraction (`data_layer.h`)                      |

---

## 4. System Overview

The EPS Monitor is a software service (not a task in its own right) that is
called every 5 seconds from the Health Monitor Task (`health_monitor_task.c`).
It follows a poll-classify-report pattern:

```
Health Monitor Task (5 s period)
        │
        ▼
eps_monitor_tick()
        │
        ├─► eps_hal_read()        — read V_batt, I_batt, T from hardware
        │
        ├─► compute_energy_state() — Schmidt-trigger classification
        │
        ├─► handle_state_change()  — fault_report() or fault_clear()
        │        │
        │        └─► fault_manager (→ FMM if CRITICAL/EMERGENCY)
        │
        └─► data_layer_set_energy_state() — publish to DLA
```

The EPS Monitor does **not** call FMM functions directly.  The single FDIR
authority chain is:

```
EPS → fault_report() → Fault Manager → fmm_force_safe()
```

This ensures all mode changes triggered by EPS are recorded in the fault audit
log maintained by the Fault Manager.

---

## 5. Energy State Model

The EPS Monitor defines four energy states in `eps.h`:

| Value | Enum constant    | Battery voltage | Description                            |
|-------|------------------|-----------------|----------------------------------------|
| 0     | `ENERGY_NOMINAL` | ≥ 7.4 V         | Full authority — all loads allowed     |
| 1     | `ENERGY_LOW`     | 7.0 – 7.4 V     | Non-critical payloads optionally shed  |
| 2     | `ENERGY_CRITICAL`| 6.6 – 7.0 V     | Attitude loads only — FM_SAFE requested|
| 3     | `ENERGY_EMERGENCY`| < 6.6 V        | Minimal loads — immediate FM_SAFE forced|

States are ordered by severity: NOMINAL (0) is best, EMERGENCY (3) is worst.

### 5.1 Subsystem Actions per Energy State

| Subsystem         | NOMINAL            | LOW                      | CRITICAL           | EMERGENCY          |
|-------------------|--------------------|--------------------------|--------------------|--------------------|
| PAYLOAD rail      | Enabled            | Optionally shed          | Disabled           | Disabled           |
| COMMS rail        | Enabled            | Enabled                  | Listen-only        | Disabled           |
| ADCS rail         | Full (RW + MTQ)    | MTQ only                 | Disabled           | Disabled           |
| OBC rail          | Always enabled     | Always enabled           | Always enabled     | Always enabled     |
| Flight mode       | Unchanged          | Unchanged                | FM_SAFE requested  | FM_SAFE forced     |
| Fault raised      | None / cleared     | `FAULT_EPS_VBATT_LOW`    | `FAULT_EPS_VBATT_CRITICAL` | `FAULT_EPS_VBATT_CRITICAL` |

---

## 6. Voltage Thresholds and Hysteresis

Thresholds are sourced from SPEC-2-EPS v1.14 Table 3-1.

### 6.1 Falling (Degradation) Thresholds

Downward transitions are accepted **immediately** at the base threshold for
fast fault response (safety-first policy):

| Transition              | Threshold | Macro                    |
|-------------------------|-----------|--------------------------|
| NOMINAL → LOW           | < 7.4 V   | `VBATT_TH_LOW`           |
| LOW → CRITICAL          | < 7.0 V   | `VBATT_TH_CRITICAL`      |
| CRITICAL → EMERGENCY    | < 6.6 V   | `VBATT_TH_EMERGENCY`     |

### 6.2 Rising (Recovery) Thresholds

Upward transitions are only accepted when voltage exceeds the base threshold
by `VBATT_HYSTERESIS` = 0.1 V, preventing chatter at boundaries:

| Transition              | Effective threshold | Description               |
|-------------------------|---------------------|---------------------------|
| EMERGENCY → CRITICAL    | ≥ 6.7 V             | base (6.6) + 0.1 hysteresis |
| CRITICAL → LOW          | ≥ 7.1 V             | base (7.0) + 0.1 hysteresis |
| LOW → NOMINAL           | ≥ 7.5 V             | base (7.4) + 0.1 hysteresis |

### 6.3 Hysteresis Example

```
V_batt (V)
  7.6 ─────────────────────────────────── NOMINAL
  7.5 ··············recovery threshold (LOW→NOMINAL)
  7.4 ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ degradation threshold
  7.0 ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ degradation threshold
  6.6 ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ degradation threshold
        ↓          ↓ falls         ↑ recovers only at +0.1 V
```

---

## 7. Schmidt-Trigger Algorithm

`compute_energy_state(float v, energy_state_t prev)` implements the hysteresis
comparator in two steps:

**Step 1 — Raw classification** (no hysteresis):

```c
if      (v < VBATT_TH_EMERGENCY) raw = ENERGY_EMERGENCY;
else if (v < VBATT_TH_CRITICAL)  raw = ENERGY_CRITICAL;
else if (v < VBATT_TH_LOW)       raw = ENERGY_LOW;
else                             raw = ENERGY_NOMINAL;
```

**Step 2 — Hysteresis gate**:

- If `raw >= prev` (state is same or worse): return `raw` immediately.
- If `raw < prev` (state would improve): re-classify `v – VBATT_HYSTERESIS`
  using the same thresholds.  This effectively raises each recovery threshold
  by 0.1 V.

The algorithm guarantees:
- Fast response to deterioration (no delay, no hysteresis on downward path).
- Chatter-free recovery (voltage must exceed threshold by 0.1 V to improve state).

---

## 8. Power Rail Management

The EPS Monitor maintains a per-rail enable/disable flag in `g_snapshot.rail_enabled[]`.

### 8.1 Rail Identifiers

| Value | Enum constant      | Description                                 |
|-------|--------------------|---------------------------------------------|
| 0     | `EPS_RAIL_PAYLOAD` | Payload / mission instruments               |
| 1     | `EPS_RAIL_COMMS`   | Communication subsystem (CSP radio)         |
| 2     | `EPS_RAIL_ADCS`    | ADCS (reaction wheels + magnetorquers)      |
| 3     | `EPS_RAIL_OBC`     | On-board computer (cannot be shed)          |

The `EPS_RAIL_OBC` rail cannot be disabled.  Calls to `eps_set_power(EPS_RAIL_OBC, false)` are silently ignored.

### 8.2 Initialization

All rails are enabled by default at `eps_monitor_init()`.  Ground commands can
shed individual non-OBC rails via `eps_set_power()`.

### 8.3 Physical Actuation

At PDR, rail enable/disable is tracked in software only (flag in `g_snapshot`).
Physical GPIO control of power switches (MOSFETs / load switches) is a Phase 2
hardware integration item — see OI-1.

---

## 9. FDIR Chain and Fault Reporting

### 9.1 Authority Chain

The EPS Monitor does **not** call FMM functions directly.  All FDIR actions flow
through the Fault Manager to ensure a single audit log:

```
eps_monitor_tick()
    └── fault_report(fault_id, fault_level)
            └── fault_manager_tick() (next cycle)
                    └── fmm_force_safe()      [if FAULT_LEVEL_CRITICAL]
```

### 9.2 Fault-to-State Mapping

| Energy state          | Fault ID                   | Level                  | Action                      |
|-----------------------|----------------------------|------------------------|-----------------------------|
| ENERGY_LOW            | `FAULT_EPS_VBATT_LOW`      | `FAULT_LEVEL_WARNING`  | Logged; no mode change      |
| ENERGY_CRITICAL       | `FAULT_EPS_VBATT_CRITICAL` | `FAULT_LEVEL_CRITICAL` | Fault Mgr → `fmm_force_safe()` |
| ENERGY_EMERGENCY      | `FAULT_EPS_VBATT_CRITICAL` | `FAULT_LEVEL_CRITICAL` | Same as CRITICAL (no separate fault ID needed) |
| HAL read failure      | `FAULT_EPS_READ_ERROR`     | `FAULT_LEVEL_ERROR`    | Logged; state unchanged (fail-safe) |

### 9.3 Fault Clearing

On state improvement, faults are cleared:

| Recovery               | Faults cleared                                      |
|------------------------|-----------------------------------------------------|
| Any state → NOMINAL    | `FAULT_EPS_VBATT_LOW`, `FAULT_EPS_VBATT_CRITICAL`  |
| CRITICAL/EMERGENCY → LOW | `FAULT_EPS_VBATT_CRITICAL`                       |

### 9.4 Fault IDs

Defined in `include/fault_ids.h`, subsystem block `0x09xx`:

| Macro                      | ID       | Description                        |
|----------------------------|----------|------------------------------------|
| `FAULT_EPS_VBATT_LOW`      | `0x0901` | Voltage below LOW threshold        |
| `FAULT_EPS_VBATT_CRITICAL` | `0x0902` | Voltage below CRITICAL threshold   |
| `FAULT_EPS_OVERCURRENT`    | `0x0903` | Bus overcurrent detected           |
| `FAULT_EPS_READ_ERROR`     | `0x0904` | EPS telemetry read failure         |

---

## 10. Data Layer Interface

The EPS Monitor publishes its output to the shared Data Layer abstraction.

### 10.1 Writes

| Function                        | When called                              |
|---------------------------------|------------------------------------------|
| `data_layer_set_energy_state()` | Every tick, with the new `energy_state_t` |

### 10.2 Reads (by consumers)

| Function                        | Consumer              | Purpose                         |
|---------------------------------|-----------------------|---------------------------------|
| `data_layer_get_energy_state()` | Telemetry Task, ADCS Task | Guard loads and telemetry flags |
| `data_layer_get_snapshot()`     | Telemetry Task        | Full HK snapshot including `energy` field |

The DLA snapshot field is `obc_snapshot_t.energy` (`energy_state_t`, `uint8_t`-equivalent).

---

## 11. Public API

All EPS Monitor functions are declared in `include/eps.h`.

### 11.1 `eps_monitor_init()`

```c
int eps_monitor_init(void);
```

- Performs an initial `eps_hal_read()`.
- Sets all rails enabled.
- Classifies initial energy state (no hysteresis on first read, assumed NOMINAL
  as starting `prev`).
- On HAL failure: sets `ENERGY_NOMINAL` (fail-safe), raises `FAULT_EPS_READ_ERROR`.
- Returns 0 on success, negative on failure.
- Must be called from `system_init()` before the EPS task starts.

### 11.2 `eps_monitor_tick()`

```c
void eps_monitor_tick(void);
```

- Called by the Health Monitor Task every 5 s.
- Reads sensors → classifies state → reports faults → publishes to DLA.
- On HAL failure: raises `FAULT_EPS_READ_ERROR`, keeps previous snapshot, returns.
- No-op if called before `eps_monitor_init()`.

### 11.3 `eps_snapshot_get()`

```c
int eps_snapshot_get(eps_snapshot_t *out);
```

- Thread-safe (protected by critical section on PICO).
- Returns -1 if called before `eps_monitor_init()` or `out == NULL`.
- Copies all fields: `vbatt`, `ibatt`, `temperature`, `state`, `timestamp_ms`,
  `rail_enabled[]`.

### 11.4 `eps_set_power()`

```c
void eps_set_power(eps_rail_t rail, bool enable);
```

- Updates `g_snapshot.rail_enabled[rail]`.
- Silently ignores `EPS_RAIL_OBC` and out-of-range values.
- Thread-safe (critical section).

### 11.5 `eps_get_energy_state()`

```c
energy_state_t eps_get_energy_state(void);
```

- Lightweight non-blocking read of `g_snapshot.state`.
- Does not re-read sensors.

---

## 12. Hardware Abstraction Layer

```c
bool eps_hal_read(float *vbatt, float *ibatt, float *temp);
```

Declared `__attribute__((weak))` in `eps_monitor.c`.  Returns `true` on success,
`false` on sensor failure.

### 12.1 Default (host / unit test) implementation

Returns nominal values: `7.6 V`, `0.5 A`, `25.0 °C`.  These values place the
system in `ENERGY_NOMINAL` by default, allowing host builds and unit tests to
run without real hardware.

### 12.2 Unit test override

Test files provide a **strong symbol** `eps_hal_read()` that injects controlled
voltage/current/temperature values and can simulate sensor read failures
(`return false`).

### 12.3 Production override (Phase 2)

The flight driver layer will provide a strong `eps_hal_read()` that reads from
the actual EPS ADC (INA219 or equivalent).  No source changes are required in
`eps_monitor.c`.

---

## 13. Concurrency and Locking

### 13.1 Locking mechanism

On PICO builds, the EPS Monitor uses `taskENTER_CRITICAL()` / `taskEXIT_CRITICAL()`
(FreeRTOS short critical sections) to protect access to `g_snapshot`.

On host/test builds, the macros are no-ops – FreeRTOS is not present.

### 13.2 Lock scope

The critical section covers only the snapshot read/write operations.  Side-effects
(`fault_report()`, `data_layer_set_energy_state()`) are intentionally called
**outside** the critical section to minimize interrupt latency.

### 13.3 Comparison with FMM

Unlike the FMM Data Layer path (which uses `xSemaphoreTake()` — a mutex, not
ISR-safe), the EPS Monitor uses `taskENTER_CRITICAL()` which is ISR-safe.
`eps_get_energy_state()` can therefore be called from interrupt context if needed.

---

## 14. Telemetry

### 14.1 EPS fields in HK frame

The following EPS fields are included in every HK telemetry beacon:

| HK Field      | C Type    | Source field                      | Units / Encoding                      |
|---------------|-----------|-----------------------------------|---------------------------------------|
| `energy`      | `uint8_t` | `obc_snapshot_t.energy` (DLA)     | `energy_state_t` enum value 0–3       |
| `vbatt`       | `float`   | `eps_snapshot_t.vbatt`            | Volts (IEEE 754 single)               |
| `ibatt`       | `float`   | `eps_snapshot_t.ibatt`            | Amperes, positive = charging          |
| `temperature` | `float`   | `eps_snapshot_t.temperature`      | °C                                    |

### 14.2 Ground software decoding (`energy` field)

| Value | Enum constant     |
|-------|-------------------|
| 0     | ENERGY_NOMINAL    |
| 1     | ENERGY_LOW        |
| 2     | ENERGY_CRITICAL   |
| 3     | ENERGY_EMERGENCY  |

---

## 15. Event Logging

Energy state transitions are logged via the persistent event logger.

| Event ID          | Macro                   | Class | Trigger                                      |
|-------------------|-------------------------|-------|----------------------------------------------|
| `0x0102`          | `LOG_EVT_ENERGY_CHANGE` | B     | Any energy state transition (by Fault Manager) |

Energy state changes that reach CRITICAL/EMERGENCY also generate fault log
entries via `fault_report()` — see §9.

---

## 16. Test Coverage

Tests are in `tests/unit/test_eps_monitor.c`.  All 12 test cases pass.

| ID    | Description                                                     | Status |
|-------|-----------------------------------------------------------------|--------|
| T-01  | `eps_snapshot_get()` returns -1 before init                     | ✅     |
| T-02  | `eps_monitor_init()` succeeds; 7.6 V → `ENERGY_NOMINAL`         | ✅     |
| T-03  | `eps_monitor_tick()` at 7.6 V → `ENERGY_NOMINAL`, no fault      | ✅     |
| T-04  | 7.2 V → `ENERGY_LOW`, `FAULT_EPS_VBATT_LOW` WARNING             | ✅     |
| T-05  | 6.8 V → `ENERGY_CRITICAL`, `FAULT_EPS_VBATT_CRITICAL` CRITICAL  | ✅     |
| T-06  | 6.4 V → `ENERGY_EMERGENCY`, same fault as CRITICAL              | ✅     |
| T-07  | Hysteresis: LOW state + 7.45 V → stays `ENERGY_LOW`             | ✅     |
| T-08  | Hysteresis: LOW state + 7.55 V → recovers to `ENERGY_NOMINAL`   | ✅     |
| T-09  | HAL read failure → `FAULT_EPS_READ_ERROR`, state unchanged       | ✅     |
| T-10  | `eps_set_power()` enables/disables a rail; reflected in snapshot | ✅     |
| T-11  | OBC rail cannot be disabled via `eps_set_power()`               | ✅     |
| T-12  | `eps_get_energy_state()` always matches `snapshot.state`         | ✅     |

---

## 17. Open Items

| OI   | Description                                                                | Priority | Phase |
|------|----------------------------------------------------------------------------|----------|-------|
| OI-1 | Implement physical GPIO actuation for power rails (MOSFET / load switch control) | High | 2 |
| OI-2 | Integrate INA219 (or equivalent ADC IC) strong-symbol `eps_hal_read()` driver | High | 2 |
| OI-3 | Add overcurrent detection: `FAULT_EPS_OVERCURRENT` (0x0903) path not yet implemented | Medium | 2 |
| OI-4 | Define load-shedding policy per rail per energy state (currently manual via `eps_set_power()`) | Medium | 2 |
| OI-5 | Add `timestamp_ms` population in `eps_monitor_tick()` (field exists in snapshot but is always 0) | Low | 1 |
