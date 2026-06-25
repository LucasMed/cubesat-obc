# FAULT-DES-001 — Fault Manager Design Document

| Field       | Value                                         |
|-------------|-----------------------------------------------|
| Document ID | FAULT-DES-001                                 |
| Version     | 0.3                                           |
| Status      | Draft                                         |
| Date        | 2026-06-20                                    |
| Author      | CubeSat OBC Team                              |
| Reviewed by | —                                             |
| Approved by | —                                             |

## Change History

| Version | Date       | Author           | Description                                     |
|---------|------------|------------------|-------------------------------------------------|
| 0.2     | 2026-03-07 | CubeSat OBC Team | Close OI-1: implement `fault_report(FAULT_EPS_VBATT_EMERGENCY, ...)` in `eps_monitor.c`; update test 6; close OI-2 |
| 0.1     | 2026-03-07 | CubeSat OBC Team | Initial draft — CDR; implements EPS-DES-001 OI-6 (`FAULT_EPS_VBATT_EMERGENCY`) |

---

## Table of Contents

1. [Introduction](#1-introduction)
2. [Applicable Documents](#2-applicable-documents)
3. [Acronyms and Definitions](#3-acronyms-and-definitions)
4. [System Context](#4-system-context)
5. [Fault Severity Model](#5-fault-severity-model)
6. [Fault ID Catalogue](#6-fault-id-catalogue)
7. [Internal Data Model](#7-internal-data-model)
8. [Public API](#8-public-api)
9. [State Machine — Fault Lifecycle](#9-state-machine--fault-lifecycle)
10. [Fault Ageing Policy](#10-fault-ageing-policy)
11. [FDIR Authority Chain](#11-fdir-authority-chain)
12. [Concurrency and Locking Model](#12-concurrency-and-locking-model)
13. [FreeRTOS Threading Model](#13-freertos-threading-model)
14. [Integration with FMM](#14-integration-with-fmm)
15. [Integration with EPS Monitor](#15-integration-with-eps-monitor)
16. [Telemetry and Observability](#16-telemetry-and-observability)
17. [Test Coverage](#17-test-coverage)
18. [Open Items](#18-open-items)
19. [References](#19-references)

---

## 1. Introduction

### 1.1 Purpose

This document specifies the design of the Fault Manager (FM) for the CubeSat
On-Board Computer (OBC). The Fault Manager is the single authority for
detecting, recording, classifying, and responding to anomalous conditions in
the flight software. It provides a centralized fault table that all subsystems
write to via `fault_report()`, and it drives Flight Mode Manager (FMM)
transitions when fault severity exceeds the CRITICAL threshold.

### 1.2 Scope

The FM governs:

- A flat 32-slot `fault_entry_t` table keyed by 16-bit fault IDs.
- Four severity levels: `FAULT_LEVEL_NONE`, `FAULT_LEVEL_WARNING`,
  `FAULT_LEVEL_ERROR`, `FAULT_LEVEL_CRITICAL`.
- The CRITICAL-triggered `fmm_force_safe()` pathway.
- Automatic fault ageing: `FAULT_LEVEL_WARNING` entries auto-clear after
  30 `fault_manager_tick()` calls (30 s at 1 Hz); ERROR and CRITICAL entries
  require explicit `fault_clear()`.
- ISR-safe locking via `taskENTER_CRITICAL` / `taskEXIT_CRITICAL`.

### 1.3 Design Basis

The implementation is derived from:

- `include/fault_manager.h` — public API and type definitions
- `include/fault_ids.h` — fault identifier catalogue (all 10 subsystems)
- `src/services/fault/fault_manager.c` — table management and FSM
- Spec ref: SPEC-2-FMM v1.1 §4–6, SPEC-2 v2.0 §4.2

---

## 2. Applicable Documents

| ID            | Title                                    | Location                          |
|---------------|------------------------------------------|-----------------------------------|
| SAD-OBC-001   | System Architecture Document             | `docs/ecss/design/SAD-OBC-001.md` |
| FMM-DES-001   | Flight Mode Manager Design Document      | `docs/ecss/design/FMM-DES-001.md` |
| EPS-DES-001   | EPS Monitor Design Document              | `docs/ecss/design/EPS-DES-001.md` |
| DL-DES-001    | Data Layer Abstraction Design Document   | `docs/ecss/design/DL-DES-001.md`  |
| ICD-OBC-001   | Interface Control Document               | `docs/ecss/design/ICD-OBC-001.md` |
| SyRS-OBC-001  | System Requirements Specification        | `docs/ecss/requirements/SyRS-OBC-001.md` |
| RTM-OBC-001   | Requirements Traceability Matrix         | `docs/ecss/verification/RTM-OBC-001.md`  |

---

## 3. Acronyms and Definitions

| Term       | Definition                                                         |
|------------|--------------------------------------------------------------------|
| FM         | Fault Manager                                                      |
| FMM        | Flight Mode Manager                                                |
| FDIR       | Fault Detection, Isolation, and Recovery                           |
| EPS        | Electrical Power System                                            |
| ISR        | Interrupt Service Routine                                          |
| fault ID   | A 16-bit identifier from `fault_ids.h`; upper byte = subsystem, lower byte = fault index |
| fault table | Flat array of 32 `fault_entry_t` slots managed by the FM        |
| auto-clear | Automatic deactivation of a WARNING fault after the ageing timeout |
| tick       | One call to `fault_manager_tick()` — expected at 1 Hz             |
| WARN_AUTO_CLEAR_TICKS | Number of ticks (30) before an un-re-raised WARNING is auto-cleared |

---

## 4. System Context

The Fault Manager sits between the subsystem tasks (fault reporters) and the
Flight Mode Manager (fault responder):

```
┌──────────────┐  fault_report(id, CRITICAL)   ┌──────────────────────┐
│ EPS Monitor  │ ────────────────────────────► │                      │
└──────────────┘                               │    Fault Manager     │
┌──────────────┐  fault_report(id, WARNING)    │                      │
│  ADCS Task   │ ────────────────────────────► │  g_table[32]         │──► fmm_force_safe()
└──────────────┘                               │  (fault_entry_t)     │       │
┌──────────────┐  fault_report(id, ERROR)      │                      │       ▼
│ Sensor Task  │ ────────────────────────────► └──────────────────────┘   FM_SAFE
└──────────────┘                                       │
                                              fault_get_highest_level()
                                              fault_is_active()
                                              fault_get_event()
                                                       │
                                              ┌────────▼────────┐
                                              │  HK Telemetry / │
                                              │  Ground Station │
                                              └─────────────────┘
```

Every subsystem that detects an anomaly calls `fault_report()`. The FM records
the event, updates severity, and — for CRITICAL faults — immediately calls
`fmm_force_safe()`. No subsystem calls `fmm_force_safe()` directly. This
single FDIR authority chain ensures all mode-changing events are logged in the
fault table before the mode change occurs.

---

## 5. Fault Severity Model

### 5.1 Severity Levels

```c
typedef enum {
    FAULT_LEVEL_NONE     = 0,  /* No active fault (cleared state)   */
    FAULT_LEVEL_WARNING  = 1,  /* Recoverable, informational        */
    FAULT_LEVEL_ERROR    = 2,  /* Degraded operation                */
    FAULT_LEVEL_CRITICAL = 3   /* Immediate safe-mode required      */
} fault_level_t;
```

Levels are ordered: NONE < WARNING < ERROR < CRITICAL.

### 5.2 Level Semantics

| Level    | Autonomous response                             | Auto-clear |
|----------|-------------------------------------------------|:----------:|
| NONE     | Entry is inactive                               | —          |
| WARNING  | Log entry only; no mode change                  | Yes (30 s) |
| ERROR    | Anomaly log entry; no mode change               | No         |
| CRITICAL | Immediate `fmm_force_safe()` call               | No         |

### 5.3 Level Escalation

When `fault_report()` is called for an already-active fault with a **higher**
severity level than currently recorded, the level is updated upward and
`tick_raised` is reset (restarting the ageing window). Downward severity
changes (re-reporting with a lower level) do **not** reduce the stored level —
the FM retains the worst-ever severity until `fault_clear()` is called.

---

## 6. Fault ID Catalogue

Fault IDs are defined in `include/fault_ids.h`. The 16-bit encoding is:

```
Bits [15:8] — subsystem code
Bits  [7:0] — fault index within subsystem
```

### 6.1 Subsystem Base Addresses

| Subsystem   | Base     | Range              |
|-------------|----------|--------------------|
| Estimator   | `0x0100` | `0x0101` – `0x01FF`|
| Controller  | `0x0200` | `0x0201` – `0x02FF`|
| Actuator    | `0x0300` | `0x0301` – `0x03FF`|
| Sensor      | `0x0400` | `0x0401` – `0x04FF`|
| Timing      | `0x0500` | `0x0501` – `0x05FF`|
| Watchdog    | `0x0600` | `0x0601` – `0x06FF`|
| Command     | `0x0700` | `0x0701` – `0x07FF`|
| Thermal     | `0x0800` | `0x0801` – `0x08FF`|
| EPS         | `0x0900` | `0x0901` – `0x09FF`|
| Telemetry   | `0x0A00` | `0x0A01` – `0x0AFF`|

### 6.2 Full Fault ID Table

| Fault ID Macro                  | Value    | Level at raise  | Description                         |
|---------------------------------|----------|:---------------:|-------------------------------------|
| `FAULT_EST_GYRO_TIMEOUT`        | `0x0101` | WARNING/ERROR   | IMU gyro read timeout               |
| `FAULT_EST_MAG_TIMEOUT`         | `0x0102` | WARNING         | Magnetometer read timeout           |
| `FAULT_EST_DIVERGENCE`          | `0x0103` | ERROR           | Estimator state diverged            |
| `FAULT_EST_QUAT_NORM`           | `0x0104` | ERROR           | Quaternion norm out of range        |
| `FAULT_CTRL_OUTPUT_SATURATED`   | `0x0201` | WARNING         | Control output clipped              |
| `FAULT_CTRL_RATE_LIMIT`         | `0x0202` | WARNING/ERROR   | Rate-limit violation                |
| `FAULT_CTRL_DEADLINE_MISS`      | `0x0203` | ERROR           | Control loop deadline missed        |
| `FAULT_ACT_RW_OVERCURRENT`      | `0x0301` | ERROR           | Reaction wheel overcurrent          |
| `FAULT_ACT_RW_SPEED_LIMIT`      | `0x0302` | WARNING         | Reaction wheel speed exceeded       |
| `FAULT_ACT_MTQ_FAULT`           | `0x0303` | ERROR           | Magnetorquer driver fault           |
| `FAULT_SENS_IMU_I2C_ERROR`      | `0x0401` | ERROR           | IMU I²C bus error                   |
| `FAULT_SENS_IMU_DATA_STALE`     | `0x0402` | WARNING         | IMU data not refreshed in time      |
| `FAULT_SENS_TEMP_OUT_RANGE`     | `0x0403` | WARNING         | Temperature sensor out of range     |
| `FAULT_TIMING_DEADLINE_MISS`    | `0x0501` | WARNING/ERROR   | Generic task deadline missed        |
| `FAULT_WDT_KICK_MISSED`         | `0x0601` | CRITICAL        | Watchdog kick not received          |
| `FAULT_CMD_UNKNOWN`             | `0x0701` | WARNING         | Unrecognised command received       |
| `FAULT_CMD_QUEUE_FULL`          | `0x0702` | WARNING         | Command queue overflow              |
| `FAULT_THERM_OVER_TEMP`         | `0x0801` | CRITICAL        | Over-temperature threshold hit      |
| `FAULT_THERM_UNDER_TEMP`        | `0x0802` | WARNING         | Under-temperature threshold hit     |
| `FAULT_EPS_VBATT_LOW`           | `0x0901` | WARNING         | Battery voltage below LOW (7.4 V)   |
| `FAULT_EPS_VBATT_CRITICAL`      | `0x0902` | CRITICAL        | Battery voltage below CRITICAL (7.0 V) |
| `FAULT_EPS_VBATT_EMERGENCY`     | `0x0903` | CRITICAL        | Battery voltage below EMERGENCY (6.6 V) — added in v0.1 (EPS-DES-001 OI-6) |
| `FAULT_EPS_OVERCURRENT`         | `0x0904` | ERROR           | Bus overcurrent detected            |
| `FAULT_EPS_READ_ERROR`          | `0x0905` | ERROR           | EPS telemetry read failure          |
| `FAULT_TLM_QUEUE_OVERFLOW`      | `0x0A01` | WARNING         | Telemetry transmit queue full       |

Total active fault IDs: **25** (`FAULT_ID_COUNT`).

> **Note — EPS-DES-001 OI-6 resolution**: `FAULT_EPS_VBATT_EMERGENCY` (0x0903)
> was added in v0.1. `FAULT_EPS_OVERCURRENT` shifted from
> 0x0903 to 0x0904, and `FAULT_EPS_READ_ERROR` from 0x0904 to 0x0905. The
> source `include/fault_ids.h` is updated accordingly. The EPS monitor
> `eps_monitor.c` ENERGY_EMERGENCY branch was updated in v0.2 to call
> `fault_report(FAULT_EPS_VBATT_EMERGENCY, FAULT_LEVEL_CRITICAL)` (OI-1 closed).

---

## 7. Internal Data Model

### 7.1 `fault_entry_t` — Internal Table Entry

```c
typedef struct {
    fault_event_t event;    /* Public-facing fault record           */
    uint32_t      tick_raised; /* g_tick value when fault was last set */
} fault_entry_t;
```

`g_table[FAULT_TABLE_CAPACITY]` — static array of 32 entries, zero-initialized
by `fault_manager_init()`.

Slot sentinel: `event.id == 0` means the slot is empty. Fault IDs are non-zero
by construction (all subsystem bases start at `0x0100`).

### 7.2 `fault_event_t` — Public Record

```c
typedef struct {
    uint16_t      id;           /* Fault identifier (from fault_ids.h)   */
    fault_level_t level;        /* Current severity level                */
    uint32_t      timestamp_ms; /* xTaskGetTickCount() at first report   */
    uint32_t      count;        /* Number of times fault has been filed  */
    bool          active;       /* true if not yet cleared               */
} fault_event_t;
```

Callers receive a `fault_event_t` copy via `fault_get_event()`. They must not
hold pointers into the table — the internal slot can be reused.

### 7.3 Module State

| Variable              | Type               | Description                           |
|-----------------------|--------------------|---------------------------------------|
| `g_table[32]`         | `fault_entry_t[]`  | Flat fault table                      |
| `g_tick`              | `uint32_t`         | Monotonic tick counter (1 Hz in flight) |

### 7.4 Table Lookup — `find_slot(id)`

Linear search over `FAULT_TABLE_CAPACITY = 32` slots:

1. If `g_table[i].event.id == id` → return slot `i` (existing entry).
2. If `g_table[i].event.id == 0` → remember as `first_empty`.
3. After full scan: if no match, return `first_empty` (new entry slot).
4. If table is full and ID not found: return `FAULT_TABLE_CAPACITY` (overflow).

Complexity: O(32) per call. At the call rates expected from flight software
(< 10 faults/s under anomalous conditions) this is negligible.

---

## 8. Public API

All functions are declared in `include/fault_manager.h`.

### 8.1 Initialisation

| Function                | Description                                              |
|-------------------------|----------------------------------------------------------|
| `fault_manager_init()`  | Zeroes all 32 table entries and resets `g_tick = 0`. Must be called once during `system_init()` before the scheduler starts. |

### 8.2 Core Functions

| Function                                       | Description                                                   |
|------------------------------------------------|---------------------------------------------------------------|
| `fault_report(uint16_t id, fault_level_t lvl)` | Report (raise) a fault. Creates a new entry if the ID is new; increments `count` if already active; calls `fmm_force_safe()` if `lvl == FAULT_LEVEL_CRITICAL`. No-op if `id == 0` or `lvl == FAULT_LEVEL_NONE`. |
| `fault_clear(uint16_t id)`                     | Mark a fault inactive (`active = false`, `level = FAULT_LEVEL_NONE`). Entry remains in the table; `count` is preserved. |
| `fault_is_active(uint16_t id)`                 | Return `true` if the fault is active; `false` if not found or cleared. Thread-safe. |
| `fault_get_highest_level()`                    | Scan all active entries and return the worst `fault_level_t`. Returns `FAULT_LEVEL_NONE` if no active faults. |
| `fault_manager_tick()`                         | Health tick, must be called at 1 Hz. Increments `g_tick` and applies WARNING auto-clear policy. |
| `fault_get_event(uint16_t id, fault_event_t *out)` | Copy the event record for `id` into `*out`. Returns `false` if ID is `0`, not found, or `out == NULL`. |

### 8.3 Guard Conditions

| Condition code path                         | Behavior                              |
|---------------------------------------------|----------------------------------------|
| `fault_report(0, any)`                      | Immediate return (0 is slot sentinel)  |
| `fault_report(any, FAULT_LEVEL_NONE)`       | Immediate return (not a real fault)    |
| `fault_report()` when table is full         | Silently dropped (should never happen with FAULT_ID_COUNT ≤ 32) |
| `fault_get_event(0, out)`                   | Returns `false`                        |
| `fault_get_event(id, NULL)`                 | Returns `false`                        |

---

## 9. State Machine — Fault Lifecycle

Each fault slot transitions through the following states:

```
         fault_report(id, lvl)
              │
     ┌────────▼─────────┐
     │      EMPTY       │  (event.id == 0, event.active == false)
     └────────┬─────────┘
              │ first fault_report → allocate slot, active = true
              │
     ┌────────▼─────────┐       fault_report again
     │      ACTIVE      │◄────────────────────────────────────────────┐
     │  (active = true) │  count++; escalate level if higher; reset   │
     └────────┬─────────┘  tick_raised on escalation                  │
              │                                                       │
       ┌──────┴──────────────────────────────┐                        │
       │                                     │                        │
  fault_clear(id)                     WARNING after                   │
       │                              WARN_AUTO_CLEAR_TICKS ticks     │
       ▼                                     │                        │
  ┌─────────────┐                     ┌──────▼──────┐                 │
  │   INACTIVE  │                     │   INACTIVE  │                 │
  │(explicit    │                     │(auto-clear) │                 │
  │  clear)     │                     └─────────────┘                 │
  │active=false │                                                     │
  └──────┬──────┘                                                     │
         │ fault_report(same id) ─────────────────────────────────────┘
         │ (slot reused; count restarted at 1)
```

Slots are never physically freed — the table entry persists with `active = false`
until the same ID is re-raised, at which point the slot is reused.

---

## 10. Fault Ageing Policy

Spec ref: SPEC-2-FMM §6.

| Level    | Auto-clear policy                                                  |
|----------|--------------------------------------------------------------------|
| WARNING  | Auto-cleared after `WARN_AUTO_CLEAR_TICKS = 30` ticks (30 s at 1 Hz), measured from the last `fault_report()` that set or kept the WARNING active. |
| ERROR    | **Never** auto-cleared. Requires `fault_clear(id)`.               |
| CRITICAL | **Never** auto-cleared. Requires `fault_clear(id)`.               |

Ageing implementation in `fault_manager_tick()`:

```c
for each active slot where level == FAULT_LEVEL_WARNING:
    if (g_tick - e->tick_raised) >= WARN_AUTO_CLEAR_TICKS:
        e->event.active = false
        e->event.level  = FAULT_LEVEL_NONE
```

`tick_raised` is reset whenever `fault_report()` is called for an already-active
WARNING (re-raising resets the ageing window, effectively keeping the warning
alive for another 30 s).

### 10.1 Rationale

- WARNING auto-clear prevents the fault table from filling with benign transient
  events (e.g., momentary I²C glitches) that do not recur.
- ERROR and CRITICAL faults represent persistent anomalies that require operator
  or recovery-software acknowledgement before clearing.

---

## 11. FDIR Authority Chain

The complete Fault Detection, Isolation, and Recovery chain for the OBC is:

```
Hardware / Sensor Anomaly
        │
        ▼
Subsystem Task (EPS Monitor, ADCS, Sensor Read, etc.)
   fault_report(FAULT_EPS_VBATT_CRITICAL, FAULT_LEVEL_CRITICAL)
        │
        ▼
Fault Manager
   ├─ Records entry in g_table (slot, timestamp, count)
   └─ level >= CRITICAL → calls fmm_force_safe()   [outside lock]
                │
                ▼
        Flight Mode Manager
           fmm_force_safe() → FM_SAFE
```

**Rules:**

1. **No subsystem calls `fmm_force_safe()` directly** — only the Fault Manager
   may do so as a consequence of a CRITICAL `fault_report()`.
2. `fmm_force_safe()` is called **after** releasing the FM critical section —
   this avoids the risk of a priority inversion between the FM lock and the
   FMM lock.
3. A CRITICAL fault remains active after `fmm_force_safe()` — the ground must
   receive the fault record via telemetry and issue an explicit `fault_clear()`
   command before recovery to NOMINAL is attempted.

---

## 12. Concurrency and Locking Model

### 12.1 Locking Primitive

The Fault Manager uses `taskENTER_CRITICAL` / `taskEXIT_CRITICAL` (FreeRTOS
Cortex-M33 port) — a brief masking of all maskable interrupts. This makes every
FM operation **ISR-safe**.

```c
static void fm_lock(void)   { taskENTER_CRITICAL(); }
static void fm_unlock(void) { taskEXIT_CRITICAL();  }
```

Host (unit test) builds compile these to empty functions — tests are
single-threaded.

### 12.2 Critical Section Duration

The longest critical section is `fault_manager_tick()`, which iterates over all
32 slots with a branch and compare per slot. On RP2350 Cortex-M33 at 133 MHz
this is measured well under 2 µs, within the interrupt latency budget.

### 12.3 CRITICAL Call Outside Lock

`fmm_force_safe()` is called **outside** the FM critical section:

```c
/* fm_unlock() already called here */
if (level >= FAULT_LEVEL_CRITICAL) {
    fmm_force_safe();  /* acquires its own FMM lock */
}
```

Rationale: `fmm_force_safe()` now uses `data_layer_set_flight_mode_from_isr()`
with `taskENTER_CRITICAL_FROM_ISR()`. Calling it outside the FM lock is the
correct pattern — no nested critical sections, and the function is fully
ISR-safe regardless of caller context.

> **Note (updated v0.33.0)**: `fmm_force_safe()` no longer uses `xSemaphoreTake`.
> It uses `taskENTER_CRITICAL_FROM_ISR()` — **ISR-safe**. See FMM-DES-001 §8.4.

### 12.4 Comparison with DLA Locking

| Component     | Primitive                                | ISR-safe |
|---------------|------------------------------------------|:--------:|
| Fault Manager | `taskENTER_CRITICAL`                     | Yes      |
| Data Layer    | `xSemaphoreCreateMutex` (standard paths) | No       |
| FMM           | `taskENTER_CRITICAL_FROM_ISR()`\*        | **Yes** ✓ |
| EPS Monitor   | `taskENTER_CRITICAL`                     | Yes      |

\* `fmm_force_safe()` only; `fmm_request_transition()` remains task-context.

---

## 13. FreeRTOS Threading Model

### 13.1 Callers of `fault_report()`

| Caller Task           | Example fault IDs                            | Typical level  |
|-----------------------|----------------------------------------------|----------------|
| EPS Monitor Task      | `FAULT_EPS_VBATT_*`, `FAULT_EPS_READ_ERROR`  | WARNING…CRITICAL |
| Sensor Read Task      | `FAULT_SENS_IMU_I2C_ERROR`, `FAULT_SENS_IMU_DATA_STALE` | WARNING…ERROR |
| ADCS / Controller     | `FAULT_CTRL_DEADLINE_MISS`, `FAULT_ACT_RW_OVERCURRENT` | WARNING…ERROR |
| Watchdog Task         | `FAULT_WDT_KICK_MISSED`                      | CRITICAL       |
| Command Handler       | `FAULT_CMD_UNKNOWN`, `FAULT_CMD_QUEUE_FULL`  | WARNING        |
| Thermal Monitor       | `FAULT_THERM_OVER_TEMP`                      | CRITICAL       |

### 13.2 Health Monitor Task (1 Hz tick caller)

A dedicated Health Monitor task at low priority calls `fault_manager_tick()`
once per second. This task also queries `fault_get_highest_level()` to
decide whether to escalate system health state in the housekeeping beacon.

### 13.3 Priority Consideration

Because `fault_report()` uses `taskENTER_CRITICAL()`, any task at any priority
can safely call it. CRITICAL faults that trigger `fmm_force_safe()` will
subsequently block on the FMM mutex — but this is outside the critical section
and subject to normal FreeRTOS priority inheritance on the FMM mutex.

---

## 14. Integration with FMM

The Fault Manager holds a weak-symbol forward declaration:

```c
extern void fmm_force_safe(void);
```

This is resolved at link time by `src/services/fmm/flight_mode_manager.c`
(strong symbol). In unit test builds it is provided by the test harness via
`data_layer_set_flight_mode(FM_SAFE)` (the FMM's own implementation calls this).

### 14.1 Mode Transition Side-Effect

When any `fault_report()` with `FAULT_LEVEL_CRITICAL` is called:

1. The fault is recorded in `g_table` (slot allocated, `active = true`).
2. `fm_unlock()` is called.
3. `fmm_force_safe()` is called — sets `mode = FM_SAFE` in the DLA.
4. The fault entry **remains active** — it is not auto-cleared by the mode change.

### 14.2 Mode-Gated Fault Escalation

The EPS monitor does not escalate to CRITICAL if the satellite is already in
`FM_SAFE` (the DLA `mode` is queried before calling `fault_report()`). The FM
itself does not suppress CRITICAL calls — it always calls `fmm_force_safe()`
regardless of current mode. Redundant `fmm_force_safe()` calls are safe because
the FMM is already in `FM_SAFE` and the call becomes a no-op.

---

## 15. Integration with EPS Monitor

The EPS monitor (`src/services/eps/eps_monitor.c`) is the primary CRITICAL fault
source. The FDIR calls made per energy state are:

| Energy State        | Action taken by EPS monitor                                  |
|---------------------|--------------------------------------------------------------|
| `ENERGY_NOMINAL`    | `fault_clear(FAULT_EPS_VBATT_LOW)`, `fault_clear(FAULT_EPS_VBATT_CRITICAL)`, `fault_clear(FAULT_EPS_VBATT_EMERGENCY)` |
| `ENERGY_LOW`        | `fault_clear(FAULT_EPS_VBATT_CRITICAL)`, `fault_clear(FAULT_EPS_VBATT_EMERGENCY)`, `fault_report(FAULT_EPS_VBATT_LOW, FAULT_LEVEL_WARNING)` |
| `ENERGY_CRITICAL`   | `fault_clear(FAULT_EPS_VBATT_LOW)`, `fault_report(FAULT_EPS_VBATT_CRITICAL, FAULT_LEVEL_CRITICAL)` |
| `ENERGY_EMERGENCY`  | `fault_clear(FAULT_EPS_VBATT_LOW)`, `fault_report(FAULT_EPS_VBATT_EMERGENCY, FAULT_LEVEL_CRITICAL)` |

> ~~**OI-1 (High / Phase 1)**~~: resolved in v0.2. `eps_monitor.c`
> `ENERGY_EMERGENCY` branch now calls
> `fault_report(FAULT_EPS_VBATT_EMERGENCY, FAULT_LEVEL_CRITICAL)`, providing
> independent mission-log visibility for emergency energy events separate from
> CRITICAL battery events. Test 6 in `test_eps_monitor.c` updated to verify.

---

## 16. Telemetry and Observability

### 16.1 HK Telemetry Fields

The HK telemetry task queries the FM once per beacon cycle (~1 Hz):

| HK Field              | FM Source                              | Format   |
|-----------------------|---------------------------------------|----------|
| `fault_highest_level` | `fault_get_highest_level()`           | uint8    |
| `fault_active_count`  | Count of active entries in `g_table`  | uint8    |
| `fault_ids[0..3]`     | IDs of up to 4 most-recent active faults | uint16[4] |

### 16.2 Ground Fault Dump

On command, the ground can request a full fault table dump: 32 × `fault_event_t`
structures (32 × 16 bytes = 512 bytes). This fits within a single telemetry
packet with the 433 MHz downlink frame budget.

### 16.3 Fault Count Persistence

`fault_event_t.count` accumulates across the mission. After a CRITICAL-triggered
`fmm_force_safe()` the count captures how many times that fault recurred. This
supports anomaly investigation.

---

## 17. Test Coverage

Tests are in `tests/unit/test_fault_manager.c`. All 12 tests pass. Coverage of
`src/services/fault/fault_manager.c` ≥ 90 % line coverage (CI gate).

| Test ID      | Test Name                         | Function Under Test                                 | Requirement |
|--------------|-----------------------------------|-----------------------------------------------------|-------------|
| T-FM-01      | `test_init`                       | `fault_manager_init()`                              | §8.1        |
| T-FM-02      | `test_basic_report`               | `fault_report()`, `fault_get_event()`               | §8.2        |
| T-FM-03      | `test_count_increment`            | `fault_report()` — repeat count                     | §8.2        |
| T-FM-04      | `test_clear`                      | `fault_clear()`, `fault_is_active()`                | §8.2        |
| T-FM-05      | `test_highest_level`              | `fault_get_highest_level()`                         | §8.2, §5.1  |
| T-FM-06      | `test_critical_triggers_safe`     | `fault_report(CRITICAL)` → `fmm_force_safe()`       | §11, §14    |
| T-FM-07      | `test_warning_auto_clear`         | `fault_manager_tick()` — WARNING at tick 30         | §10         |
| T-FM-08      | `test_error_not_auto_cleared`     | `fault_manager_tick()` — ERROR/CRITICAL persist     | §10         |
| T-FM-09      | `test_multiple_faults`            | Multiple concurrent fault IDs                       | §7.4        |
| T-FM-10      | `test_reraise_after_clear`        | `fault_report()` after `fault_clear()`              | §9          |
| T-FM-11      | `test_none_level_noop`            | `fault_report(id, FAULT_LEVEL_NONE)` guard          | §8.3        |
| T-FM-12      | `test_get_event_unknown`          | `fault_get_event(0xFFFF)` and `fault_get_event(0)`  | §8.3        |

Total: **12 / 12 tests passing**.

---

## 18. Open Items

| OI   | Severity | Phase   | Status  | Description                                                                |
|------|----------|---------|---------|----------------------------------------------------------------------------|
| OI-1 | High     | Phase 1 | **Closed v0.2** | ~~Update `src/services/eps/eps_monitor.c` ENERGY_EMERGENCY branch to call `fault_report(FAULT_EPS_VBATT_EMERGENCY, FAULT_LEVEL_CRITICAL)`.~~ Implemented and verified in test 6. |
| OI-2 | Low      | Phase 1 | **Closed v0.2** | ~~Add FDIR integration test: simulate `ENERGY_EMERGENCY` → verify `FAULT_EPS_VBATT_EMERGENCY` active + `FM_SAFE` set in one test cycle.~~ Test 6 in `test_eps_monitor.c` now verifies this path. |
| OI-3 | Low      | Phase 2 | Open    | Add `fault_report_from_isr()` variant using `xSemaphoreGiveFromISR` pattern if any ISR-originated fault source is added in future hardware integration. |
| OI-4 | Low      | Post-CDR | Open   | Audit all `fault_ids.h` additions against `FAULT_ID_COUNT` counter going forward. |

---

## 19. References

1. SPEC-2-FMM v1.1 §4–6 — Fault Manager and FDIR specification (internal)
2. SPEC-2 v2.0 §4.2 — OBC Software Architecture — Fault Management
3. FMM-DES-001 v0.3 — FMM Design Document (`docs/ecss/design/FMM-DES-001.md`)
4. EPS-DES-001 v0.2 — EPS Monitor Design Document (`docs/ecss/design/EPS-DES-001.md`)
5. DL-DES-001 v0.1 — Data Layer Design Document (`docs/ecss/design/DL-DES-001.md`)
6. FreeRTOS Reference Manual — `taskENTER_CRITICAL`, `taskEXIT_CRITICAL`
7. ECSS-E-ST-40C — Software Engineering Standard
8. ECSS-Q-ST-80C — Software Product Assurance (PAP-OBC-001)
