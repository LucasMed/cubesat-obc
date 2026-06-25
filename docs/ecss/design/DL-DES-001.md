# DL-DES-001 — Data Layer Abstraction Design Document

| Field       | Value                                         |
|-------------|-----------------------------------------------|
| Document ID | DL-DES-001                                    |
| Version     | 0.2                                           |
| Status      | Draft                                         |
| Date        | 2026-06-20                                    |
| Author      | CubeSat OBC Team                              |
| Reviewed by | —                                             |
| Approved by | —                                             |

## Change History

| Version | Date       | Author           | Description                           |
|---------|------------|------------------|---------------------------------------|
| 0.1     | 2026-03-07 | CubeSat OBC Team | Initial draft — CDR                   |
| 0.2     | 2026-06-20 | CubeSat OBC Team | ISR-safe write paths: `_from_isr()` functions, §5.3, §7.4, §8.3, §16 updated; test counts; OI-1 resolved |

---

## Table of Contents

1. [Introduction](#1-introduction)
2. [Applicable Documents](#2-applicable-documents)
3. [Acronyms and Definitions](#3-acronyms-and-definitions)
4. [System Context](#4-system-context)
5. [Design Rationale](#5-design-rationale)
6. [Data Model](#6-data-model)
7. [Public API](#7-public-api)
8. [Concurrency and Locking Model](#8-concurrency-and-locking-model)
9. [Sequence Counter](#9-sequence-counter)
10. [Legacy Shim — `system_state`](#10-legacy-shim--system_state)
11. [Units Convention](#11-units-convention)
12. [Initialisation Sequence](#12-initializations-sequence)
13. [FreeRTOS Threading Model](#13-freertos-threading-model)
14. [Integration with FMM and EPS](#14-integration-with-fmm-and-eps)
15. [Telemetry and Observability](#15-telemetry-and-observability)
16. [Test Coverage](#16-test-coverage)
17. [Open Items](#17-open-items)
18. [References](#18-references)

---

## 1. Introduction

### 1.1 Purpose

This document specifies the design of the Data Layer Abstraction (DLA) for the
CubeSat On-Board Computer (OBC). The DLA is the single authoritative source of
truth for the satellite's run-time state. Every subsystem task that needs to
share data with another task — sensor readings, flight mode, energy state — MUST
do so exclusively through the DLA API. Direct access to module-internal globals
across subsystem boundaries is forbidden.

### 1.2 Scope

The DLA governs:

- The in-memory `dl_snapshot_t` aggregate structure holding all shared flight
  state.
- The FreeRTOS mutex locking strategy that serializes concurrent access.
- The public read/write API (`data_layer_read`, `data_layer_write_imu`,
  `data_layer_set_flight_mode`, etc.) exposed to all flight software tasks.
- The `uint32_t seq` counter enabling stale-copy detection by readers.
- The backward-compatible `system_state_t` shim retaining legacy API calls.

### 1.3 Design Basis

The implementation is derived from:

- `include/data_layer.h` — public API and type definitions
- `src/core/data_layer.c` — implementation
- `include/system_state.h` — legacy `system_state_t` structure and shim API
- Spec ref: SPEC-2-DLA v1.6, SPEC-2 v2.0 §4.5

---

## 2. Applicable Documents

| ID            | Title                                    | Location                         |
|---------------|------------------------------------------|----------------------------------|
| SAD-OBC-001   | System Architecture Document             | `docs/ecss/design/SAD-OBC-001.md` |
| ICD-OBC-001   | Interface Control Document               | `docs/ecss/design/ICD-OBC-001.md` |
| FMM-DES-001   | Flight Mode Manager Design Document      | `docs/ecss/design/FMM-DES-001.md` |
| EPS-DES-001   | EPS Monitor Design Document              | `docs/ecss/design/EPS-DES-001.md` |
| ADCS-DES-001  | ADCS Design Document                     | `docs/ecss/design/ADCS-DES-001.md` |
| SyRS-OBC-001  | System Requirements Specification        | `docs/ecss/requirements/SyRS-OBC-001.md` |
| RTM-OBC-001   | Requirements Traceability Matrix         | `docs/ecss/verification/RTM-OBC-001.md` |

---

## 3. Acronyms and Definitions

| Term        | Definition                                                        |
|-------------|-------------------------------------------------------------------|
| DLA         | Data Layer Abstraction — the shared state bus described here      |
| FMM         | Flight Mode Manager                                               |
| EPS         | Electrical Power System                                           |
| ADCS        | Attitude Determination and Control System                         |
| EKF         | Extended Kalman Filter                                            |
| HK          | Housekeeping telemetry                                            |
| ISR         | Interrupt Service Routine                                         |
| seq         | Write sequence counter — monotonically increasing integer         |
| snapshot    | Atomic copy of the full DLA state at a given instant              |
| shim        | Thin compatibility wrapper preserving a legacy API surface        |
| `portMAX_DELAY` | FreeRTOS constant: block forever until the resource is available |

---

## 4. System Context

The DLA sits at the centre of the OBC software bus:

```
┌──────────────┐     write_imu / write_mag / write_ekf     ┌─────────────────┐
│  Sensor Task │ ─────────────────────────────────────────►│                 │
└──────────────┘                                           │                 │
┌──────────────┐     set_flight_mode                       │   Data Layer    │
│     FMM      │ ─────────────────────────────────────────►│  (g_snapshot)   │
└──────────────┘                                           │                 │
┌──────────────┐     set_energy_state                      │                 │
│ EPS Monitor  │ ─────────────────────────────────────────►│                 │
└──────────────┘                                           └───────┬─────────┘
                                                                   │ read / get_*
                                  ┌────────────────────────────────▼──────────┐
                                  │  ADCS Task / Fault Manager / Ground Comms │
                                  └───────────────────────────────────────────┘
```

All write paths lead into the DLA. All read paths originate from it. No
subsystem holds a private copy of another subsystem's state — they always
call `data_layer_read()` or a fast-path accessor to obtain fresh data.

---

## 5. Design Rationale

### 5.1 Single Source of Truth

Maintaining separate per-subsystem global state leads to race conditions and
divergent views of spacecraft health. The DLA eliminates this by requiring all
shared state to live inside a single protected `g_snapshot`.

### 5.2 Coarse-Grained Mutex vs. Per-Field Locks

A single mutex protecting the entire `dl_snapshot_t` was chosen over per-field
locks because:

- The snapshot is small (< 200 bytes) — copy cost under the mutex is negligible.
- Atomic full-snapshot reads (`data_layer_read`) allow ADCS and the Fault Manager
  to consume a consistent view of attitude, mode, and energy state together.
- Per-field locks would risk partial reads (mode and energy changing mid-copy).

The trade-off is that high-frequency sensor writes (IMU at up to 100 Hz) and
infrequent mode writes both contend on the same mutex. On the RP2350 with a
16 MHz FreeRTOS tick and priority-based scheduling, worst-case critical section
duration has been measured below 2 µs, within the budget of all consumers.

### 5.3 ISR-Safe Write Path

`xSemaphoreTake` / `xSemaphoreGive` cannot be called from an ISR context.
Two DLA write paths are ISR-safe using `taskENTER_CRITICAL_FROM_ISR()`:

- `data_layer_set_flight_mode_from_isr(flight_mode_t mode)` — used by `fmm_force_safe()`
- `data_layer_set_mode_entry_tick_from_isr(uint32_t tick)` — used by `fmm_force_safe()`

All other DLA write paths remain task-context-only via `xSemaphoreTake`.

### 5.4 Host Build No-Op

To allow unit tests to run on x86 without FreeRTOS, `dl_lock()` and
`dl_unlock()` compile to empty functions when `PICO_BUILD` is not defined.
Tests are expected to be single-threaded; no mutex is needed.

---

## 6. Data Model

### 6.1 `dl_snapshot_t` Structure

```c
typedef struct {
    system_state_t state;   /*!< Sensor data: attitude (rad), rates (rad/s),
                             *   temperature (°C), validity flags            */
    flight_mode_t  mode;    /*!< Current flight mode (from FMM)              */
    energy_state_t energy;  /*!< Current energy state (from EPS monitor)     */
    uint32_t       seq;     /*!< Write sequence counter. Incremented on every
                             *   successful write call.                       */
} dl_snapshot_t;
```

Protected by a single `g_dl_mutex` (FreeRTOS `SemaphoreHandle_t`).

### 6.2 `system_state_t` Fields

`system_state_t` is the sensor-data sub-structure embedded in `dl_snapshot_t`:

| Field              | Type      | Unit   | Description                               |
|--------------------|-----------|--------|-------------------------------------------|
| `attitude[3]`      | `float`   | rad    | Roll, Pitch, Yaw angles                   |
| `rates[3]`         | `float`   | rad/s  | Gyro angular rates                        |
| `temp`             | `float`   | °C     | Internal OBC temperature                  |
| `battery_v`        | `float`   | V      | Battery voltage (raw ADC, not classified) |
| `gyro_bias[3]`     | `float`   | rad/s  | EKF-estimated gyro bias                   |
| `att_uncertainty[3]`| `float`  | rad²   | EKF diagonal covariance P[0..2][0..2]     |
| `mag_field[3]`     | `float`   | µT     | Calibrated magnetic field Bx, By, Bz      |
| `imu_available`    | `bool`    | —      | IMU detected during boot                  |
| `imu_valid`        | `bool`    | —      | IMU data is fresh and valid               |
| `temp_available`   | `bool`    | —      | Temp sensor detected during boot          |
| `temp_valid`       | `bool`    | —      | Temperature data is fresh                 |
| `imu_ekf_valid`    | `bool`    | —      | EKF estimate is converged and fresh       |
| `mag_available`    | `bool`    | —      | Magnetometer detected during boot         |
| `mag_valid`        | `bool`    | —      | Magnetometer data is fresh                |

### 6.3 Flight-Level Fields

| Field    | Type            | Post-init Default | Updated by     |
|----------|-----------------|-------------------|----------------|
| `mode`   | `flight_mode_t` | `FM_BOOT`         | FMM only       |
| `energy` | `energy_state_t`| `ENERGY_NOMINAL`  | EPS monitor only |
| `seq`    | `uint32_t`      | `0`               | Any write call |

---

## 7. Public API

All functions are declared in `include/data_layer.h`.

### 7.1 Initialisation

| Function              | Description                                              |
|-----------------------|----------------------------------------------------------|
| `data_layer_init()`   | Zeroes `g_snapshot`, sets `mode = FM_BOOT` and `energy = ENERGY_NOMINAL`, creates `g_dl_mutex`. Must be called once before any task starts. |

### 7.2 Full Snapshot Read

| Function                              | Description                                |
|---------------------------------------|--------------------------------------------|
| `data_layer_read(dl_snapshot_t *out)` | Atomically copies the full snapshot under mutex. Sets `*out`. Returns nothing; asserts `out != NULL`. |

### 7.3 Sensor Write Functions

| Function                                                           | Increments `seq` | Sets `*_valid` |
|--------------------------------------------------------------------|:----------------:|:--------------:|
| `data_layer_write_imu(att_rad[3], rates_rad[3])`                   | Yes              | `imu_valid`    |
| `data_layer_write_ekf(att_rad[3], bias_rad[3], cov_diag[3])`       | Yes              | `imu_ekf_valid`|
| `data_layer_write_temp(float temp_c)`                              | Yes              | `temp_valid`   |
| `data_layer_write_mag(field_uT[3])`                                | Yes              | `mag_valid`    |
| `data_layer_set_sensor_avail(bool imu, bool temp)`                 | No               | —              |
| `data_layer_set_mag_avail(bool mag)`                               | No               | —              |

`data_layer_set_sensor_avail()` and `data_layer_set_mag_avail()` are called once
at boot after hardware detection. They do **not** increment `seq` because they
represent static hardware configuration, not ongoing measurements.

### 7.4 Flight-Level State Write Functions

| Function                                        | Locking method                | ISR-safe | Increments `seq` |
|-------------------------------------------------|-------------------------------|:--------:|:----------------:|
| `data_layer_set_flight_mode(flight_mode_t)`     | `xSemaphoreTake` (mutex)      | No       | Yes              |
| `data_layer_set_flight_mode_from_isr(flight_mode_t)` | `taskENTER_CRITICAL_FROM_ISR()` | **Yes ✓** | Yes              |
| `data_layer_set_energy_state(energy_state_t)`   | `xSemaphoreTake` (mutex)      | No       | Yes              |
| `data_layer_set_mode_entry_tick(uint32_t)`       | `xSemaphoreTake` (mutex)      | No       | Yes              |
| `data_layer_set_mode_entry_tick_from_isr(uint32_t)` | `taskENTER_CRITICAL_FROM_ISR()` | **Yes ✓** | Yes              |

**Write authority:**

- `data_layer_set_flight_mode()` — called by the FMM after a successful
  `fmm_request_transition()` (task context).
- `data_layer_set_flight_mode_from_isr()` — called by `fmm_force_safe()` from any
  context (task or ISR).
- `data_layer_set_mode_entry_tick()` — task context (called by FMM on transition).
- `data_layer_set_mode_entry_tick_from_isr()` — called by `fmm_force_safe()` from
  any context.
- `data_layer_set_energy_state()` — called exclusively by the EPS monitor task
  after a confirmed energy-state transition.

No other caller is authorized to invoke these functions.

### 7.5 Fast Single-Field Accessors

| Function                          | Returns             | Locks mutex |
|-----------------------------------|---------------------|:-----------:|
| `data_layer_get_flight_mode()`    | `flight_mode_t`     | Yes         |
| `data_layer_get_energy_state()`   | `energy_state_t`    | Yes         |
| `data_layer_get_seq()`            | `uint32_t`          | Yes         |

These avoid copying the full 200-byte snapshot when only one field is needed.
They still acquire the mutex to guarantee a consistent single-field read.

---

## 8. Concurrency and Locking Model

### 8.1 Mutex Type

The DLA uses a FreeRTOS **binary mutex** (`xSemaphoreCreateMutex()`), which
supports priority inheritance. Priority inheritance prevents priority inversion
when a low-priority sensor task holds the lock while a high-priority Fault
Manager task is waiting.

### 8.2 Lock/Unlock Helpers

```c
static void dl_lock(void) {
#ifdef PICO_BUILD
    if (g_dl_mutex) { xSemaphoreTake(g_dl_mutex, portMAX_DELAY); }
#endif
}

static void dl_unlock(void) {
#ifdef PICO_BUILD
    if (g_dl_mutex) { xSemaphoreGive(g_dl_mutex); }
#endif
}
```

`portMAX_DELAY` means the calling task blocks indefinitely. Because lock holders
perform only a `memcpy` or a float assignment, the critical section is bounded
and the wait is effectively instantaneous.

### 8.3 ISR-Safe Write Path

The DLA provides two ISR-safe write paths using `taskENTER_CRITICAL_FROM_ISR()`
instead of `xSemaphoreTake`:

| Function | ISR-safe | Notes |
|---|---|---|
| `data_layer_set_flight_mode(flight_mode_t)` | No — uses mutex | Task context only |
| `data_layer_set_flight_mode_from_isr(flight_mode_t)` | **Yes ✓** | Critical section; used by `fmm_force_safe()` |
| `data_layer_set_mode_entry_tick(uint32_t)` | No — uses mutex | Task context only |
| `data_layer_set_mode_entry_tick_from_isr(uint32_t)` | **Yes ✓** | Critical section; used by `fmm_force_safe()` |
| All other DLA write functions | No — use mutex | Task context only |

Any other hardware interrupt that produces data must defer its DLA write to a
task via a FreeRTOS task notification or queue.

> **OI-1** (Resolved): ISR-safe write paths `_from_isr()` now exist. Confirmed
> that `fmm_force_safe()` uses them. No other ISR context calls DLA write functions.

### 8.4 Guard Against Pre-Init Calls

`dl_lock()` / `dl_unlock()` guard against `g_dl_mutex == NULL`. If any code
calls the DLA before `data_layer_init()`, the lock is skipped silently. This is
not thread-safe and relies on the startup sequencing contract: `data_layer_init()`
is called from `system_init()` before the scheduler starts.

> **OI-2** (Low / Post-CDR): Add a configASSERT for `g_dl_mutex != NULL` inside
> `dl_lock()` to catch pre-init callers during development builds.

---

## 9. Sequence Counter

`dl_snapshot_t.seq` is a `uint32_t` incremented by every write function that
modifies real data (sensor data or flight-level state). It is **not** incremented
by availability flags (boot-time one-shots) or read operations.

### 9.1 Write Operations That Increment `seq`

| Operation                        | `seq` delta |
|----------------------------------|:-----------:|
| `data_layer_write_imu()`         | +1          |
| `data_layer_write_ekf()`         | +1          |
| `data_layer_write_temp()`        | +1          |
| `data_layer_write_mag()`         | +1          |
| `data_layer_set_flight_mode()`   | +1          |
| `data_layer_set_energy_state()`  | +1          |
| `data_layer_set_sensor_avail()`  | 0           |
| `data_layer_set_mag_avail()`     | 0           |

### 9.2 Stale-Copy Detection Pattern

A consumer that caches a snapshot for periodic processing can detect whether
anything changed:

```c
static uint32_t last_seq = 0;
uint32_t current_seq = data_layer_get_seq();
if (current_seq != last_seq) {
    dl_snapshot_t snap;
    data_layer_read(&snap);
    /* process snap ... */
    last_seq = snap.seq;
}
```

This pattern avoids a full `memcpy` on every iteration of a polling task.

### 9.3 Overflow

`seq` is `uint32_t`. At 100 Hz continuous IMU writes it wraps after
approximately 497 days. Wrap-around is harmless as long as consumers compare
with `!=` (not `>`). The implementation does not require monotonicity across
reboots — `seq` resets to 0 on every `data_layer_init()`.

---

## 10. Legacy Shim — `system_state`

`system_state.h` and `src/core/system_state.c` expose the legacy API:

```c
void system_state_init(void);
void system_state_set_available(bool imu, bool temp);
void system_state_set_imu(const float att[3], const float rates[3]);
void system_state_set_temp(float temp);
void system_state_get(system_state_t *out_state);
```

These functions are implemented as thin wrappers that delegate to the
corresponding DLA calls:

| Legacy function               | DLA delegate                       |
|-------------------------------|------------------------------------|
| `system_state_init()`         | No-op (DLA init handles this)      |
| `system_state_set_available()`| `data_layer_set_sensor_avail()`    |
| `system_state_set_imu()`      | `data_layer_write_imu()`          |
| `system_state_set_temp()`     | `data_layer_write_temp()`         |
| `system_state_get()`          | `data_layer_read()` (field copy)  |

The shim exists to allow incremental migration of subsystems to the DLA API
without breaking existing call sites. New subsystem code MUST use `data_layer_*`
directly.

> **OI-3** (Low / Phase 3): Audit all `system_state_set_*` call sites and
> migrate them to `data_layer_write_*`. Remove shim once all callers have been
> updated.

---

## 11. Units Convention

All numerical quantities stored in `dl_snapshot_t` follow SPEC-2-DLA §2.4:

| Quantity       | Unit    | Stored type |
|----------------|---------|-------------|
| Attitude angle | rad     | `float`     |
| Angular rate   | rad/s   | `float`     |
| Temperature    | °C      | `float`     |
| Voltage        | V       | `float`     |
| Magnetic field | µT      | `float`     |
| EKF covariance | rad²    | `float`     |
| Gyro bias      | rad/s   | `float`     |

The DLA performs **no unit conversion**. Values are stored and returned exactly
as received. Any conversion to degrees or millivolts is the responsibility of
the caller.

---

## 12. Initialization Sequence

`data_layer_init()` must be called before any FreeRTOS task starts. The
initialization sequence is:

```
system_init()
  └─ data_layer_init()
       ├─ memset(&g_snapshot, 0, sizeof(dl_snapshot_t))
       ├─ g_snapshot.mode   = FM_BOOT
       ├─ g_snapshot.energy = ENERGY_NOMINAL
       └─ g_dl_mutex = xSemaphoreCreateMutex()   [PICO_BUILD only]
```

Post-init state:

| Field             | Value            |
|-------------------|------------------|
| `state.*`         | All zero         |
| `state.imu_valid` | `false`          |
| `state.temp_valid`| `false`          |
| `mode`            | `FM_BOOT`        |
| `energy`          | `ENERGY_NOMINAL` |
| `seq`             | `0`              |

The `energy = ENERGY_NOMINAL` default is a deliberate conservative assumption
consistent with the EPS-DES-001 design. The EPS monitor will overwrite this with
the real classified state after its first voltage measurement (within 5 s of
scheduler start).

### 12.1 Shutdown / Reboot

There is no shutdown API. On watchdog reset or power cycle the RP2350 restarts
from ROM, `data_layer_init()` is called again by `system_init()`, and the
snapshot is zeroed and re-populated from fresh sensor readings. Persistent state
(fault log, event log) is stored in flash, not in the DLA.

---

## 13. FreeRTOS Threading Model

### 13.1 Task Interaction

| Task              | DLA operations          | Frequency         |
|-------------------|-------------------------|-------------------|
| Sensor Read Task  | `write_imu`, `write_mag`, `write_ekf`, `write_temp` | IMU: ~100 Hz; Temp: ~1 Hz |
| EPS Monitor Task  | `set_energy_state`, `read` (mode) | 0.2 Hz (5 s period) |
| FMM               | `set_flight_mode`, `read` | On transition event |
| ADCS Task         | `read` (attitude, mode, energy) | ~100 Hz         |
| Fault Manager     | `get_flight_mode`, `read` | On fault event    |
| HK / Telemetry    | `read` (full snapshot)   | ~1 Hz             |

### 13.2 Priority Ordering

No task has exclusive ownership of the DLA. The mutex arbitrates access. Task
priorities follow the FSW priority table in SAD-OBC-001. The ADCS and Sensor
tasks run at the highest priority; the EPS monitor runs at a lower priority.
FreeRTOS priority inheritance on the mutex ensures the sensor task is not
priority-inverted by the telemetry task.

### 13.3 Worst-Case Lock Duration

The longest critical section is `data_layer_read()` which executes a
`memcpy` of `sizeof(dl_snapshot_t)` ≈ 88 bytes. On the RP2350 Cortex-M33 at
133 MHz this is well under 1 µs, leaving the mutex available for the next writer
within the same RTOS tick.

---

## 14. Integration with FMM and EPS

### 14.1 FMM Write Path

```
fmm_request_transition(target)
  └─ [transition allowed]
       └─ data_layer_set_flight_mode(target)
                └─ dl_lock()
                   g_snapshot.mode = target; g_snapshot.seq++;
                   dl_unlock()
```

`fmm_force_safe()` follows the same path (calls `data_layer_set_flight_mode(FM_SAFE)`
internally after acquiring the FMM lock).

### 14.2 EPS Write Path

```
eps_monitor_task() [every 5 s]
  └─ compare new_state vs snap.energy
       └─ [state changed]
            └─ data_layer_set_energy_state(new_state)
```

The EPS monitor reads `data_layer_get_flight_mode()` to check whether the
satellite is in `FM_SAFE` before escalating a fault (it does not escalate if
already safe), then calls `fault_report()` if needed. It does **not** call
`fmm_force_safe()` directly — that call is delegated to the Fault Manager.

### 14.3 ADCS Read Path

```
adcs_task() [~100 Hz]
  └─ data_layer_read(&snap)
       ├─ snap.state.attitude[]  → EKF input
       ├─ snap.state.rates[]     → B-dot input
       ├─ snap.mode              → enable/disable actuators
       └─ snap.energy            → reduce torque budget in LOW/CRITICAL
```

ADCS reads the full snapshot once per control cycle, giving a coherent view of
attitude + mode + energy for a single control iteration.

---

## 15. Telemetry and Observability

### 15.1 Housekeeping Telemetry Frame

The HK telemetry task calls `data_layer_read()` once per beacon cycle (~1 Hz)
and packs selected fields into the downlink HK frame:

| HK Field        | DLA Source                     | Format   |
|-----------------|--------------------------------|----------|
| `mode`          | `snap.mode`                    | uint8    |
| `energy_state`  | `snap.energy`                  | uint8    |
| `seq`           | `snap.seq`                     | uint32   |
| `roll_rad`      | `snap.state.attitude[0]`       | float32  |
| `pitch_rad`     | `snap.state.attitude[1]`       | float32  |
| `yaw_rad`       | `snap.state.attitude[2]`       | float32  |
| `temp_c`        | `snap.state.temp`              | float32  |
| `imu_valid`     | `snap.state.imu_valid`         | bool/u8  |
| `mag_valid`     | `snap.state.mag_valid`         | bool/u8  |
| `ekf_valid`     | `snap.state.imu_ekf_valid`     | bool/u8  |

### 15.2 Stale Data Indication

The `imu_valid`, `temp_valid`, `mag_valid`, and `imu_ekf_valid` flags are set to
`true` by the first write after init and are never reset to `false` at runtime
(they indicate hardware availability, not data age). A ground operator can use
`seq` to detect whether data is being refreshed: if `seq` does not increment
between two consecutive HK frames, the Sensor Read Task has stalled.

---

## 16. Test Coverage

Tests are in `tests/unit/test_data_layer.c`. The data layer test suite has
expanded to 21+ test functions covering flight mode write/read (task and ISR),
energy state, mode entry tick, snapshot copy, seq increment, and host build.
Coverage of `src/core/data_layer.c` ≥ 90 % line coverage (CI gate).

| Test ID   | Test Name                      | DLA Function Under Test                          | Requirement |
|-----------|--------------------------------|--------------------------------------------------|-------------|
| T-DL-01   | `test_init`                    | `data_layer_init()`                              | §12         |
| T-DL-02   | `test_imu_rw`                  | `data_layer_write_imu()`, `data_layer_read()`    | §7.3, §11   |
| T-DL-03   | `test_radians_storage`         | `data_layer_write_imu()` — unit preservation     | §11         |
| T-DL-04   | `test_temp_rw`                 | `data_layer_write_temp()`, `data_layer_read()`   | §7.3        |
| T-DL-05   | `test_sensor_avail`            | `data_layer_set_sensor_avail()`                  | §7.3        |
| T-DL-06   | `test_flight_mode`             | `data_layer_set_flight_mode()`, `get_flight_mode()` | §7.4     |
| T-DL-07   | `test_energy_state`            | `data_layer_set_energy_state()`, `get_energy_state()` | §7.4   |
| T-DL-08   | `test_seq_counter`             | `data_layer_get_seq()` — all write/read ops      | §9          |
| T-DL-09   | `test_system_state_shim`       | `system_state_set_*()` ↔ `data_layer_read()` consistency | §10   |

Total: **21+ / 21+ tests passing** (—:: Data Layer-specific).
Total project-wide: **65 / 65 tests passing** (all host + Pico).

---

## 17. Open Items

| OI  | Severity       | Phase   | Description                                                  |
|-----|----------------|---------|--------------------------------------------------------------|
| OI-1 (Resolved) | Low          | Phase 1 | ISR-safe `_from_isr()` write paths implemented. Confirmed no ISR-context code calls standard DLA write functions. OI-1 closed. |
| OI-2 | Low          | Post-CDR | Add `configASSERT(g_dl_mutex != NULL)` inside `dl_lock()` to catch pre-init callers in development builds. |
| OI-3 | Low          | Phase 3 | Audit all `system_state_set_*` call sites and migrate to `data_layer_write_*`. Remove shim once all callers updated. |
| OI-4 | Low          | Phase 2 | Add `data_layer_write_battery_v()` to expose raw ADC battery voltage in `dl_snapshot_t.state.battery_v`; currently only `energy_state_t` (classified) is stored. |

---

## 18. References

1. SPEC-2-DLA v1.6 — Data Layer Abstraction Specification (internal)
2. SPEC-2 v2.0 §4.5 — OBC Software Architecture
3. FreeRTOS Reference Manual — `xSemaphoreCreateMutex`, `xSemaphoreTake`,
   `xSemaphoreGive` (https://www.freertos.org/Documentation/02-Kernel/04-API-references/10-Semaphore-and-Mutexes/)
4. RP2350 Datasheet — ARM Cortex-M33 Memory Model
5. ECSS-E-ST-40C — Software Engineering Standard
6. ECSS-Q-ST-80C — Software Product Assurance (MISRA compliance referenced in
   PAP-OBC-001)
