# FMM-DES-001 — Flight Mode Manager Design Document

| Field       | Value                                         |
|-------------|-----------------------------------------------|
| Document ID | FMM-DES-001                                   |
| Version     | 0.2                                           |
| Status      | Draft                                         |
| Date        | 2026-03-06                                    |
| Author      | CubeSat OBC Team                              |
| Reviewed by | —                                             |
| Approved by | —                                             |

**Change History**

| Version | Date       | Author           | Description          |
|---------|------------|------------------|----------------------|
| 0.1     | 2026-03-06 | CubeSat OBC Team | Initial draft — PDR  |
| 0.2     | 2026-03-07 | CubeSat OBC Team | CDR review: ISR safety correction (§8.4, §13), FM_BOOT exit clarification (§5.1), mode change event (§8.6, §15), OI-5 added |

---

## Table of Contents

1. [Introduction](#1-introduction)
2. [Applicable Documents](#2-applicable-documents)
3. [Acronyms and Definitions](#3-acronyms-and-definitions)
4. [System Context](#4-system-context)
5. [Flight Mode Enumeration](#5-flight-mode-enumeration)
6. [Allowed-Transition Matrix](#6-allowed-transition-matrix)
7. [State Machine Diagram](#7-state-machine-diagram)
8. [FMM API](#8-fmm-api)
9. [Fault Integration](#9-fault-integration)
10. [EPS Integration](#10-eps-integration)
11. [Subsystem Behaviour per Mode](#11-subsystem-behaviour-per-mode)
12. [Data Layer Interface](#12-data-layer-interface)
13. [FreeRTOS Threading Model](#13-freertos-threading-model)
14. [Fault IDs](#14-fault-ids)
15. [Verification and Test Mapping](#15-verification-and-test-mapping)
16. [Open Items](#16-open-items)
17. [References](#17-references)

---

## 1. Introduction

### 1.1 Purpose

This document specifies the design of the Flight Mode Manager (FMM) for the
CubeSat On-Board Computer (OBC). The FMM is the authority for all flight mode
state changes; every subsystem that must adapt its behavior to operational
context queries the FMM rather than maintaining its own mode variable.

### 1.2 Scope

The FMM governs five discrete operational modes (`FM_BOOT`, `FM_SAFE`,
`FM_DETUMBLE`, `FM_NOMINAL`, `FM_DIAGNOSTIC`) and enforces the allowed-transition
rules defined in §6. It integrates with the Fault Manager to force an immediate
`FM_SAFE` transition on any `FAULT_LEVEL_CRITICAL` event.

### 1.3 Design Basis

The implementation is derived from:
- `include/flight_mode.h` — public API and type definitions
- `src/services/fmm/flight_mode_manager.c` — state machine and transition logic
- Spec ref: SPEC-2-FMM v1.1, SPEC-2 v2.0 §4.1

---

## 2. Applicable Documents

| ID            | Title                                    | Location                              |
|---------------|------------------------------------------|---------------------------------------|
| SAD-OBC-001   | System Architecture Document             | `docs/ecss/design/SAD-OBC-001.md`     |
| ADCS-DES-001  | ADCS Design Document                     | `docs/ecss/design/ADCS-DES-001.md`    |
| ICD-OBC-001   | Interface Control Document               | `docs/ecss/design/ICD-OBC-001.md`     |
| SyRS-OBC-001  | System Requirements Specification        | `docs/ecss/requirements/SyRS-OBC-001.md` |
| RTM-OBC-001   | Requirements Traceability Matrix         | `docs/ecss/verification/RTM-OBC-001.md` |

---

## 3. Acronyms and Definitions

| Term      | Definition                                                           |
|-----------|----------------------------------------------------------------------|
| FMM       | Flight Mode Manager                                                  |
| FM        | Flight Mode                                                          |
| ADCS      | Attitude Determination and Control System                            |
| EKF       | Extended Kalman Filter                                               |
| EPS       | Electrical Power System                                              |
| FDIR      | Fault Detection, Isolation and Recovery                              |
| OBC       | On-Board Computer                                                    |
| ISR       | Interrupt Service Routine                                            |
| HK        | Housekeeping telemetry                                               |
| CSP       | CubeSat Space Protocol                                               |

---

## 4. System Context

The FMM is a service module (`src/services/fmm/`) that sits between the system
initialization layer and all operational tasks. It owns no FreeRTOS task of its
own; instead it exposes a thread-safe API consumed by tasks and interrupt
handlers.

```
┌──────────────────────────────────────────────────────────────────┐
│                        OBC Flight Software                       │
│                                                                  │
│  ┌────────────────┐    request_transition()    ┌──────────────┐  │
│  │  Ground (CSP)  │ ────────────────────────►  │     FMM      │  │
│  └────────────────┘                            │   service    │  │
│                                                │              │  │
│  ┌────────────────┐    fmm_force_safe()        │  state held  │  │
│  │ Fault Manager  │ ────────────────────────►  │  in Data     │  │
│  └────────────────┘                            │  Layer       │  │
│                                                └──────┬───────┘  │
│  ┌────────────────┐    fmm_get_mode() /               │          │
│  │  ADCS / EPS /  │ ◄── data_layer_get_flight_mode()  │          │
│  │  Telemetry     │                                   │          │
│  └────────────────┘                                   │          │
│                                            data_layer_set_       │
│                                            flight_mode()         │
└──────────────────────────────────────────────────────────────────┘
```

---

## 5. Flight Mode Enumeration

Defined in `include/flight_mode.h`:

```c
typedef enum {
    FM_BOOT       = 0,  /* Power-on / hardware initialization          */
    FM_SAFE       = 1,  /* Minimum power, fault recovery               */
    FM_DETUMBLE   = 2,  /* Angular-rate reduction via B-dot law        */
    FM_NOMINAL    = 3,  /* Normal three-axis attitude control          */
    FM_DIAGNOSTIC = 4,  /* Ground-commanded diagnostic / testing mode  */
    FM_COUNT            /* Sentinel — not a valid mode                 */
} flight_mode_t;
```

Higher numeric values indicate progressively more nominal operation; this
ordering is used only for documentation — the transition rules are defined
exclusively by the matrix in §6.

### 5.1 Mode Descriptions

| Mode          | Numeric | Entry Condition                                   | Exit Condition                                |
|---------------|---------|---------------------------------------------------|-----------------------------------------------|
| FM_BOOT       | 0       | Power-on reset; set by `flight_mode_manager_init()` | **Current**: ground command `BOOT→DETUMBLE` only. **Phase 2**: automatic when angular rate estimate available from ADCS. |
| FM_SAFE       | 1       | Any `FAULT_LEVEL_CRITICAL` event; or `fmm_force_safe()` | Ground command `SAFE→DETUMBLE` only   |
| FM_DETUMBLE   | 2       | Ground command or auto from FM_BOOT/FM_SAFE        | `ω < 0.05 rad/s` sustained → FM_NOMINAL (Phase 2); or fault |
| FM_NOMINAL    | 3       | Ground command from FM_DETUMBLE/FM_DIAGNOSTIC      | Ground command or fault                       |
| FM_DIAGNOSTIC | 4       | Ground command from FM_NOMINAL only               | Ground command; or any fault                  |

---

## 6. Allowed-Transition Matrix

```
From \ To  │ BOOT  SAFE  DETUMBLE  NOMINAL  DIAGNOSTIC
───────────┼──────────────────────────────────────────
BOOT       │  —     ✓      ✓         ✗         ✗
SAFE       │  ✗     —      ✓         ✗         ✗
DETUMBLE   │  ✗     ✓      —         ✓         ✗
NOMINAL    │  ✗     ✓      ✓         —         ✓
DIAGNOSTIC │  ✗     ✓      ✗         ✓         —
```

**Special rules:**

1. **FM_SAFE is always reachable** from any mode — `fmm_request_transition(FM_SAFE)`
   bypasses the matrix row check and is always accepted.
2. **FAULT_LEVEL_CRITICAL blocks all non-SAFE transitions** — `fmm_request_transition()`
   returns `FMM_ERR_FAULT_BLOCK` if a CRITICAL fault is active and the target is
   not FM_SAFE.
3. **`fmm_force_safe()`** additionally bypasses the fault-level check but is
   **not ISR-safe** (uses mutex via Data Layer). See OI-5.

Implementation (`src/services/fmm/flight_mode_manager.c`):

```c
static const uint8_t g_allowed[FM_COUNT][FM_COUNT] = {
    /*               BOOT  SAFE  DETUMBLE  NOMINAL  DIAGNOSTIC */
    /* FM_BOOT       */ {0, 1, 1, 0, 0},
    /* FM_SAFE       */ {0, 0, 1, 0, 0},
    /* FM_DETUMBLE   */ {0, 1, 0, 1, 0},
    /* FM_NOMINAL    */ {0, 1, 1, 0, 1},
    /* FM_DIAGNOSTIC */ {0, 1, 0, 1, 0},
};
```

---

## 7. State Machine Diagram

```
                 ┌─────────────────────────────────────────────────┐
                 │          FAULT_LEVEL_CRITICAL  /  fmm_force_safe│
                 │          (from any mode)                        │
                 ▼                                                 │
  ┌──────────┐  rate ok   ┌──────────────┐  ω < threshold   ┌──────────────┐
  │ FM_BOOT  │ ──────────►│ FM_DETUMBLE  │ ───────────────► │  FM_NOMINAL  │
  └──────────┘            └──────────────┘                  └──────┬───────┘
       │                        ▲  │                               │
       │ any fault / cmd        │  │ fault                         │ cmd
       ▼                        │  ▼                               ▼
  ┌──────────┐  cmd DETUMBLE    │  ┌──────────────────────────────────────┐
  │ FM_SAFE  │ ─────────────────┘  │           FM_DIAGNOSTIC              │
  └──────────┘                     └──────────────────────────────────────┘
                                         │ cmd NOMINAL / fault
                                         ▼
                                    FM_NOMINAL / FM_SAFE
```

**Transition trigger sources:**

| Source                   | API called                      | Context               |
|--------------------------|---------------------------------|-----------------------|
| Ground command (CSP)     | `fmm_request_transition(target)`| Telemetry task        |
| Fault Manager (CRITICAL) | `fmm_force_safe()`              | Fault Manager task    |
| Watchdog timeout         | `fmm_force_safe()`              | Health Monitor task (⚠️ not ISR — see OI-5) |
| EPS CRITICAL energy      | `fmm_force_safe()` via fault    | EPS Monitor task      |
| Automatic (rate < thr.)  | `fmm_request_transition(FM_NOMINAL)` | ADCS task (Phase 2) |

---

## 8. FMM API

Declared in `include/flight_mode.h`.

### 8.1 Initialisation

```c
void flight_mode_manager_init(void);
```

Sets the mode to `FM_BOOT` via `data_layer_set_flight_mode()`. Must be called
once during `system_init()` before any task is created.

### 8.2 Query Mode

```c
flight_mode_t fmm_get_mode(void);
```

Returns the current mode. Subsystems that
poll frequently should prefer `data_layer_get_flight_mode()` from an already-read
snapshot to avoid repeated mutex acquisitions.

### 8.3 Request Transition

```c
fmm_result_t fmm_request_transition(flight_mode_t target);
```

Evaluates in order:

1. Range-check `target < FM_COUNT` → `FMM_ERR_INVALID`
2. Current == target → `FMM_OK` (no-op)
3. Target ≠ FM_SAFE && highest fault ≥ CRITICAL → `FMM_ERR_FAULT_BLOCK`
4. Matrix `g_allowed[current][target] == 0` → `FMM_ERR_NOT_ALLOWED`
5. `data_layer_set_flight_mode(target)` → `FMM_OK`

**Return codes:**

| Code                 | Value | Meaning                                         |
|----------------------|-------|-------------------------------------------------|
| `FMM_OK`             | 0     | Transition accepted (or already in target mode) |
| `FMM_ERR_INVALID`    | 1     | Target is out of range                          |
| `FMM_ERR_NOT_ALLOWED`| 2     | Transition not in allowed matrix                |
| `FMM_ERR_FAULT_BLOCK`| 3     | Active CRITICAL fault blocks the transition     |

### 8.4 Force Safe

```c
void fmm_force_safe(void);
```

Writes `FM_SAFE` directly to the Data Layer. Bypasses both the matrix and the
fault-level check.

> ⚠️ **Not ISR-safe.** Despite the comment in the source code, `data_layer_set_flight_mode()`
> uses `xSemaphoreTake()` (a FreeRTOS mutex), which **cannot be called from ISR context**.
> All current callers (`fault_manager.c`, `health_monitor_task.c`) run in task context.
> See OI-5 for the planned ISR-safe path.

Logging: a `LOG_EVT_SAFE_ENTRY` (0x0001, Class A) event is emitted by the Fault
Manager after calling `fmm_force_safe()`.

### 8.5 Mode Name

```c
const char *fmm_mode_name(flight_mode_t mode);
```

Returns a constant null-terminated string (`"BOOT"`, `"SAFE"`, `"DETUMBLE"`,
`"NOMINAL"`, `"DIAGNOSTIC"`, or `"UNKNOWN"`). Used exclusively for logging.

### 8.6 Mode Change Event

Every successful transition accepted by `fmm_request_transition()` or
`fmm_force_safe()` shall emit a `LOG_EVT_MODE_CHANGE` (0x0101, Class B)
persistent log event carrying the old and new mode values.

```
[LOG_EVT_MODE_CHANGE | old_mode | new_mode | timestamp_us]
```

This event is the primary input for:
- Ground-station FDIR analysis
- Post-pass telemetry review
- Anomaly investigation (mode thrashing, unexpected SAFE entries)

> **Implementation note (OI-5 dependency):** The event emission is currently
> handled by `fault_manager.c` only for `FM_SAFE` entry. A dedicated
> `LOG_EVT_MODE_CHANGE` call inside `fmm_request_transition()` is planned as
> part of OI-5 / Phase 2 hardening.

---

## 9. Fault Integration

The FMM integrates with the Fault Manager through two paths:

### 9.1 Passive: fault-level query on every transition

`fmm_request_transition()` calls `fault_get_highest_level()` before evaluating
the matrix. If the return value is ≥ `FAULT_LEVEL_CRITICAL` and the target is not
`FM_SAFE`, the transition is refused with `FMM_ERR_FAULT_BLOCK`.

A `__attribute__((weak))` stub in `flight_mode_manager.c` returns
`FAULT_LEVEL_NONE` when `fault_manager.c` is not linked (host/test builds).

### 9.2 Active: immediate forced transition

`fault_manager.c` declares `fmm_force_safe()` as an `extern` and calls it
whenever `fault_report()` receives a `FAULT_LEVEL_CRITICAL` event:

```c
// src/services/fault/fault_manager.c
extern void fmm_force_safe(void);
...
if (level >= FAULT_LEVEL_CRITICAL) {
    fmm_force_safe();
}
```

This guarantees `FM_SAFE` is entered before the fault record is even written to
the Data Layer.

### 9.3 Fault severity mapping

| Fault level    | FMM effect                                          |
|----------------|-----------------------------------------------------|
| NONE / WARNING | None — FMM state unchanged                          |
| ERROR          | Anomaly logged; non-SAFE transitions still allowed  |
| CRITICAL       | `fmm_force_safe()` called; all non-SAFE transitions blocked |

---

## 10. EPS Integration

The EPS Monitor (`src/services/eps/eps_monitor.c`) detects low-battery
conditions and drives FM_SAFE via the Fault Manager — it does **not** call
`fmm_request_transition()` directly (which would bypass the fault audit log):

| EPS energy state   | Action                                                    |
|--------------------|-----------------------------------------------------------|
| `ENERGY_NORMAL`    | No FMM effect                                             |
| `ENERGY_LOW`       | `fault_report(FAULT_EPS_VBATT_LOW, FAULT_LEVEL_WARNING)`  |
| `ENERGY_CRITICAL`  | `fault_report(FAULT_EPS_VBATT_CRITICAL, FAULT_LEVEL_CRITICAL)` → `fmm_force_safe()` |
| `ENERGY_EMERGENCY` | `fault_report(FAULT_EPS_OVERCURRENT, FAULT_LEVEL_CRITICAL)` → `fmm_force_safe()` |

---

## 11. Subsystem Behaviour per Mode

Each subsystem reads the flight mode from the Data Layer snapshot at the start
of its control loop iteration.

| Subsystem            | FM_BOOT | FM_SAFE     | FM_DETUMBLE         | FM_NOMINAL            | FM_DIAGNOSTIC |
|----------------------|---------|-------------|---------------------|-----------------------|---------------|
| ADCS (attitude ctrl) | No output | No output | B-dot momentum dump | LQR (or PID fallback) | PID           |
| Sensor read task     | Active  | Active      | Active              | Active                | Active        |
| Telemetry task       | Minimal HK | Minimal HK (temp only, attitude zeroed) | Full ADCS TLM | Full ADCS TLM | Full ADCS TLM |
| EPS Monitor          | Active  | Active      | Active              | Active                | Active        |
| Health Monitor       | Active  | Active      | Active              | Active                | Active        |
| CSP / Ground comms   | Listen  | Listen      | Listen              | Listen + uplink       | Listen + uplink |
| Reaction wheels      | Off     | Off         | Off (MTQ only)      | Active (Phase 2)      | Active (Phase 2) |

ADCS controller dispatch is defined in detail in ADCS-DES-001 §2.

---

## 12. Data Layer Interface

The FMM stores the active mode in the global OBC snapshot via:

```c
// Write (FMM internal only)
void data_layer_set_flight_mode(flight_mode_t mode);

// Read (any task, thread-safe)
flight_mode_t data_layer_get_flight_mode(void);
```

The mode field lives in `obc_snapshot_t.mode` (`include/data_layer.h:54`).
`data_layer_set_flight_mode()` uses `xSemaphoreTake()` (a FreeRTOS mutex) and is
therefore safe from **task context only**. It must not be called from ISR context.
See OI-5 for the planned ISR-safe write path.

---

## 13. FreeRTOS Threading Model

The FMM owns no dedicated task. All API functions are synchronous and
re-entrant-safe:

| Function                    | Internal locking                  | ISR-safe |
|-----------------------------|-----------------------------------|----------|
| `flight_mode_manager_init()`| None (called before scheduler)    | No       |
| `fmm_get_mode()`            | `xSemaphoreTake` mutex (via DL)   | No       |
| `fmm_request_transition()`  | `xSemaphoreTake` mutex (via DL)   | No       |
| `fmm_force_safe()`          | `xSemaphoreTake` mutex (via DL)   | **No** ⚠️ |
| `fmm_mode_name()`           | None (read-only constant table)   | Yes      |

> ⚠️ **ISR-safe path not yet implemented.** The Data Layer uses `xSemaphoreTake()`
> (FreeRTOS mutex) for all writes. None of the FMM write functions may be called
> from interrupt context. See OI-5.

---

## 14. Fault IDs

The fault IDs that trigger FMM transitions are defined in `include/fault_ids.h`.
The following IDs result in `FAULT_LEVEL_CRITICAL` and therefore force `FM_SAFE`:

| Fault ID                    | Subsystem  | Condition                                         |
|-----------------------------|------------|---------------------------------------------------|
| `FAULT_EST_DIVERGENCE`      | Estimator  | EKF state diverged beyond recovery threshold      |
| `FAULT_WDT_KICK_MISSED`     | Watchdog   | Hardware watchdog not kicked within timeout       |
| `FAULT_EPS_VBATT_CRITICAL`  | EPS        | Battery voltage below critical threshold          |
| `FAULT_EPS_OVERCURRENT`     | EPS        | Bus overcurrent detected                          |
| `FAULT_THERM_OVER_TEMP`     | Thermal    | Over-temperature threshold exceeded               |

Faults classified as `FAULT_LEVEL_WARNING` or `FAULT_LEVEL_ERROR` do not trigger
FM_SAFE but are logged to the event ring buffer.

---

## 15. Verification and Test Mapping

### 15.1 Unit Tests (host build)

| Test ID       | Description                                                  | Pass Criterion                                      |
|---------------|--------------------------------------------------------------|-----------------------------------------------------|
| T-FMM-01      | `flight_mode_manager_init()` sets mode to FM_BOOT            | `fmm_get_mode() == FM_BOOT`                         |
| T-FMM-02      | All allowed matrix transitions accepted                      | `fmm_request_transition()` returns `FMM_OK` for each ✓ cell |
| T-FMM-03      | All disallowed matrix transitions refused                    | Returns `FMM_ERR_NOT_ALLOWED` for each ✗ cell       |
| T-FMM-04      | `FM_SAFE` reachable from any mode                            | `fmm_request_transition(FM_SAFE)` returns `FMM_OK` from all 4 other modes |
| T-FMM-05      | Out-of-range target returns `FMM_ERR_INVALID`                | `fmm_request_transition(FM_COUNT)` == `FMM_ERR_INVALID` |
| T-FMM-06      | Same-mode request is a no-op (`FMM_OK`)                      | `fmm_request_transition(current)` == `FMM_OK`       |
| T-FMM-07      | `FAULT_LEVEL_CRITICAL` blocks non-SAFE transitions           | Returns `FMM_ERR_FAULT_BLOCK` with mock fault active |
| T-FMM-08      | `fmm_force_safe()` sets FM_SAFE unconditionally              | Mode == FM_SAFE after call from any starting mode   |
| T-FMM-09      | `fmm_mode_name()` returns correct strings                    | String equality for all 5 modes + UNKNOWN case      |
| T-FMM-10      | `LOG_EVT_MODE_CHANGE` emitted on every accepted transition   | Event ring buffer contains correct old/new mode values after each transition |

### 15.2 Integration Tests

| Test ID        | Description                                                  | Pass Criterion                                      |
|----------------|--------------------------------------------------------------|-----------------------------------------------------|
| T-FMM-INT-01   | Fault Manager CRITICAL event triggers FM_SAFE transition     | Mode == FM_SAFE after `fault_report(…, CRITICAL)`   |
| T-FMM-INT-02   | FM mode persists across Data Layer snapshot reads            | All tasks read consistent mode value                |
| T-FMM-INT-03   | Ground CSP command drives FM_DETUMBLE → FM_NOMINAL           | Transition accepted and mode updated in telemetry   |
| T-FMM-INT-04   | EPS CRITICAL energy state forces FM_SAFE                     | Mode == FM_SAFE after EPS critical event injection  |

---

## 16. Open Items

| ID   | Description                                                        | Priority |
|------|--------------------------------------------------------------------|----------|
| OI-1 | Automatic FM_BOOT → FM_DETUMBLE transition (rate threshold check) — **current implementation is ground-commanded only** | Medium |
| OI-2 | Automatic FM_DETUMBLE → FM_NOMINAL transition (ω < threshold sustained over N samples) — Phase 2 | Medium |
| OI-3 | FM_SAFE timeout/recovery path (e.g. after successful fault clear) | Low |
| OI-4 | `fmm_request_transition()` ISR-safety evaluation if fault_manager uses mutex | Low |
| OI-5 | **ISR-safe forced safe path**: implement `fmm_force_safe_from_isr()` using `xSemaphoreGiveFromISR()` or a dedicated atomic flag polled by a task. Required before any hardware watchdog or timer ISR needs to trigger FM_SAFE directly. | High |

---

## 17. References

| Reference       | Title                                               |
|-----------------|-----------------------------------------------------|
| ECSS-E-ST-40C   | Software engineering standard                       |
| ECSS-E-ST-10C   | System engineering standard                         |
| SPEC-2-FMM v1.1 | Internal FMM specification (superseded by this doc) |
| ADCS-DES-001    | ADCS Design Document (controller dispatch table)    |
| SAD-OBC-001     | System Architecture Document                        |
