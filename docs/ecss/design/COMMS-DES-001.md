# COMMS-DES-001 — Communications Subsystem Design Document

| Field       | Value                                         |
|-------------|-----------------------------------------------|
| Document ID | COMMS-DES-001                                 |
| Version     | 0.1                                           |
| Status      | Draft                                         |
| Date        | 2026-03-07                                    |
| Author      | CubeSat OBC Team                              |
| Reviewed by | —                                             |
| Approved by | —                                             |

## Change History

| Version | Date       | Author           | Description                                     |
|---------|------------|------------------|-------------------------------------------------|
| 0.1     | 2026-03-07 | CubeSat OBC Team | Initial draft — CDR; documents CSP/KISS TT&C stack |

---

## Table of Contents

1. [Introduction](#1-introduction)
2. [Applicable Documents](#2-applicable-documents)
3. [Acronyms and Definitions](#3-acronyms-and-definitions)
4. [System Context](#4-system-context)
5. [Protocol Stack](#5-protocol-stack)
6. [Physical Layer](#6-physical-layer)
7. [Framing Layer — KISS](#7-framing-layer--kiss)
8. [Network Layer — CSP](#8-network-layer--csp)
9. [CSP Routing Table](#9-csp-routing-table)
10. [Telemetry Downlink Interface](#10-telemetry-downlink-interface)
11. [Command Uplink Interface](#11-command-uplink-interface)
12. [Built-in CSP Services](#12-built-in-csp-services)
13. [FreeRTOS Task Model](#13-freertos-task-model)
14. [Initialisation Sequence](#14-initialization-sequence)
15. [Host Build Behaviour](#15-host-build-behavior)
16. [Error Handling](#16-error-handling)
17. [Test Mapping](#17-test-mapping)
18. [Open Items](#18-open-items)
19. [References](#19-references)

---

## 1. Introduction

This document describes the design of the CubeSat OBC communications subsystem.
The subsystem provides the Telemetry, Tracking and Command (TT&C) link between
the OBC and Ground Station (GS) via the CubeSat Space Protocol (CSP) over a
UART/KISS physical link.

### 1.1 Purpose

- Define the protocol stack and address space used for TT&C
- Document the downlink telemetry packet format and transmission policy
- Document the uplink command interface and command dictionary
- Provide test coverage mapping and open items for CDR review

### 1.2 Scope

In scope:
- `src/core/comm_init.c` — CSP stack initialization
- `src/tasks/telemetry_task.c` — 1 Hz HK telemetry downlink
- `src/tasks/command_task.c` — uplink command reception and dispatch
- `src/drivers/uart/pico_usart.c` — UART1/KISS physical driver

Out of scope (future phases):
- AX.25 / FX.25 link layer
- Hardware radio transceiver interface (UHF/VHF)
- Link budget calculations (`LINK-BDG-001`)
- Encryption / authentication

---

## 2. Applicable Documents

| ID              | Title                                           | Version |
|-----------------|-------------------------------------------------|---------|
| PHASE3_COMM_SPEC | Phase 3 Communication Specification (internal) | —       |
| ICD-OBC-001     | Interface Control Document                      | v1.1    |
| SAD-OBC-001     | Software Architecture Document                  | v1.0    |
| DL-DES-001      | Data Layer Design Document                      | v0.1    |
| FMM-DES-001     | Flight Mode Manager Design Document             | v0.3    |
| EPS-DES-001     | Electrical Power System Design Document         | v0.1    |
| libcsp          | CubeSat Space Protocol library                  | v2.x    |

---

## 3. Acronyms and Definitions

| Term    | Definition                                                          |
|---------|---------------------------------------------------------------------|
| CSP     | CubeSat Space Protocol — lightweight network/transport layer        |
| KISS    | Keep It Simple, Stupid — UART byte-stuffing framing protocol        |
| TT&C   | Telemetry, Tracking and Command                                     |
| GS / GN | Ground Station                                                     |
| OBC     | On-Board Computer                                                   |
| HK      | Housekeeping telemetry                                              |
| FDIR    | Fault Detection, Isolation and Recovery                             |
| DLA     | Data Layer Abstraction (`data_layer.h`)                             |
| FM      | Flight Mode                                                         |

---

## 4. System Context

```
  Ground Station (GS)           OBC (RP2350 / Pico 2W)
  ┌──────────────────┐          ┌──────────────────────────────────┐
  │  CSP addr = 1    │  UART1   │  CSP addr = 10                   │
  │                  │◄────────►│                                  │
  │  Ground Software │  115200  │  ┌───────────┐  ┌─────────────┐  │
  │  (telemetry Rx)  │  KISS    │  │ Telemetry │  │  Command    │  │
  │  (command Tx)    │          │  │ Task      │  │  Task       │  │
  └──────────────────┘          │  │ (1 Hz DL) │  │  (port 20)  │  │
                                │  └─────┬─────┘  └──────┬──────┘  │
                                │        │                │        │
                                │        ▼                ▼        │
                                │     ┌──────────────────────┐     │
                                │     │   Data Layer (DLA)   │     │
                                │     └──────────────────────┘     │
                                └──────────────────────────────────┘
```

The communications subsystem sits between the ground segment and the OBC's
internal Data Layer. It has **no direct access** to actuators or FDIR
mechanisms — all state changes requested by uplink commands are routed through
the Flight Mode Manager (FMM) or other services via the DLA.

---

## 5. Protocol Stack

```
┌─────────────────────────────┐
│  Application Layer          │  csp_telemetry_packet_t  /  csp_command_packet_t
├─────────────────────────────┤
│  Transport Layer (CSP)      │  Connection-less (UDP-like) / Connection-oriented
├─────────────────────────────┤
│  Network Layer (CSP)        │  16-bit address space, routing table
├─────────────────────────────┤
│  Framing Layer (KISS)       │  FEND/FESC byte-stuffing over UART bytes
├─────────────────────────────┤
│  Physical Layer (UART1)     │  115200 baud, 8N1, GPIO8 (TX) / GPIO9 (RX)
└─────────────────────────────┘
```

---

## 6. Physical Layer

### 6.1 Interface Parameters

| Parameter     | Value                          |
|---------------|--------------------------------|
| Peripheral    | UART1 (`uart1`)                |
| TX pin        | GPIO8                          |
| RX pin        | GPIO9                          |
| Baud rate     | 115200 baud                    |
| Data bits     | 8                              |
| Stop bits     | 1                              |
| Parity        | None                           |
| Flow control  | None (hardware flow disabled)  |
| FIFO          | Enabled                        |

> **Note**: `pico_pins.h` defines `UART1_TX_PIN = 8`, `UART1_RX_PIN = 9`.
> The driver in `pico_usart.c` currently hard-codes these pins when `uart1`
> is selected. OI-1 tracks migration to `pico_pins.h` constants.

### 6.2 Driver Architecture

The custom driver (`src/drivers/uart/pico_usart.c`) implements the libcsp
`csp_usart_open()` / `csp_usart_write()` interface for the RP2350:

- **RX**: A dedicated FreeRTOS task (`UART_RX`, 1024 words stack, priority
  `configMAX_PRIORITIES - 1`) polls `uart_is_readable()` in a tight loop with
  a 1-tick yield, calling the registered `rx_callback` byte-by-byte into the
  KISS framer.
- **TX**: `csp_usart_write()` calls `uart_putc_raw()` per byte — no DMA.
  Thread safety is provided by a FreeRTOS mutex (`lock`).

> **OI-2**: The polling RX loop burns CPU at high data rates. A future
> improvement is to switch to UART RX interrupt + ring buffer to reduce idle
> CPU utilisation.

---

## 7. Framing Layer — KISS

KISS (Keep It Simple, Stupid) is a minimal byte-stuffing protocol that frames
CSP packets for transmission over the UART byte stream.

| Symbol | Byte  | Role                              |
|--------|-------|-----------------------------------|
| FEND   | 0xC0  | Frame start/end delimiter         |
| FESC   | 0xDB  | Escape character                  |
| TFEND  | 0xDC  | Escaped FEND (0xDB 0xDC = 0xC0)   |
| TFESC  | 0xDD  | Escaped FESC (0xDB 0xDD = 0xDB)   |

The framer is provided by libcsp (`csp/interfaces/csp_if_kiss.h`).
`comm_init()` calls `csp_usart_open_and_add_kiss_interface()` to register the
KISS interface named `"KISS"` with the CSP router.

---

## 8. Network Layer — CSP

### 8.1 Address Assignment

| Node              | CSP Address | Source                  |
|-------------------|-------------|-------------------------|
| OBC               | 10          | `OBC_ADDRESS` in `comm_init.c` |
| Ground Station    | 1           | `GN_ADDRESS` in `comm_init.c` / `telemetry_task.c` |

### 8.2 Address Space

CSP uses a 16-bit address field. The project reserves:
- Addresses 1–10: used nodes (GS=1, OBC=10)
- Address 255: broadcast

### 8.3 Port Assignment

| Port | Name            | Direction  | Protocol             |
|------|-----------------|------------|----------------------|
| 0    | CSP_PING        | Bi         | Built-in CSP service |
| 1    | CSP_PS          | GS→OBC     | Built-in (process list) |
| 2    | CSP_MEMFREE     | GS→OBC     | Built-in (free memory) |
| 3    | CSP_REBOOT      | GS→OBC     | Built-in (soft reboot)|
| 4    | CSP_BUF_FREE    | GS→OBC     | Built-in (buffer status) |
| 5    | CSP_UPTIME      | GS→OBC     | Built-in (uptime)    |
| 10   | TELEMETRY_PORT  | OBC→GS     | `csp_telemetry_packet_t` |
| 20   | COMMAND_PORT    | GS→OBC     | `csp_command_packet_t` |

### 8.4 CSP Router Task

The libcsp 2.x router does not have a background thread; `csp_route_work()`
must be called repeatedly. In the Pico build a dedicated FreeRTOS task
`vCspRouterTask` (1024 words, priority `tskIDLE_PRIORITY + 3`) calls
`csp_route_work()` every 1 ms via `vTaskDelay(pdMS_TO_TICKS(1))`.

---

## 9. CSP Routing Table

Loaded by `comm_init()` via `csp_rtable_load()`:

```
"1/255 KISS"
```

This routes all packets destined for the GS subnet (address 1, mask 255 = host
route) via the `KISS` interface. No route exists for the Pico loopback build —
see [Section 15](#15-host-build-behavior).

---

## 10. Telemetry Downlink Interface

### 10.1 Transmission Policy

| Parameter             | Value                                  |
|-----------------------|----------------------------------------|
| Rate                  | 1 Hz (1000 ms period)                  |
| Scheduling            | `vTaskDelayUntil()` — rate-monotonic   |
| Task                  | `vTelemetryTask` (calls `vTelemetryTask_Step()`) |
| Destination address   | GN_ADDRESS = 1                         |
| Destination port      | TELEMETRY_PORT = 10                    |
| Source port           | TELEMETRY_PORT = 10                    |
| CSP priority          | `CSP_PRIO_NORM`                        |
| Connection type       | Connection-less (`csp_sendto`)         |
| CSP options           | `CSP_O_NONE`                           |

### 10.2 Packet Structure

`csp_telemetry_packet_t` — 29 bytes, `__attribute__((packed))`:

```c
typedef struct __attribute__((packed))
{
  uint32_t timestamp_ms;  /* 4  B — time since boot [ms]              */
  float    attitude[3];   /* 12 B — Roll, Pitch, Yaw [deg], IEEE-754  */
  float    rates[3];      /* 12 B — Roll, Pitch, Yaw rates [deg/s]    */
  float    temp;          /*  4 B — OBC internal temperature [°C]     */
  uint8_t  flags;         /*  1 B — validity + energy state           */
} csp_telemetry_packet_t; /* Total: 33 bytes */
```

> **Note**: Actual struct size is 33 bytes (4 + 12 + 12 + 4 + 1).
> The `__attribute__((packed))` prevents any padding.

### 10.3 Flags Field Layout

| Bit(s) | Symbol                  | Description                          |
|--------|-------------------------|--------------------------------------|
| 0      | `TLM_FLAG_IMU_VALID`    | 1 = IMU data valid                   |
| 1      | `TLM_FLAG_TEMP_VALID`   | 1 = temperature reading valid        |
| [3:2]  | `TLM_FLAG_ENERGY_SHIFT` | 2-bit energy state (0=NOMINAL…3=EMERGENCY) |
| [7:4]  | —                       | Reserved, set to 0                   |

### 10.4 Flight-Mode Content Policy

| Flight Mode       | Attitude / Rates fields     | Temperature |
|-------------------|-----------------------------|-------------|
| FM_NOMINAL        | Full ADCS data from DLA     | From DLA    |
| FM_DETUMBLE       | Full ADCS data from DLA     | From DLA    |
| FM_DIAGNOSTIC     | Full ADCS data from DLA     | From DLA    |
| FM_BOOT           | Full ADCS data from DLA     | From DLA    |
| **FM_SAFE**       | **All zeroed**              | From DLA    |

In `FM_SAFE`, attitude and rates are set to 0.0f to avoid transmitting
potentially stale / invalid ADCS data. Temperature is always transmitted.

### 10.5 Buffer Allocation

`csp_buffer_get(sizeof(csp_telemetry_packet_t))` allocates from the CSP packet
pool. If the pool is exhausted, `csp_buffer_get()` returns NULL; in that case
`vTelemetryTask_Step()` logs a warning and returns without sending.

---

## 11. Command Uplink Interface

### 11.1 Reception Model

`vCommandTask` runs on a dedicated FreeRTOS task (Pico build; see
[Section 13](#13-freertos-task-model)). It:

1. Binds a CSP socket to `COMMAND_PORT = 20`
2. Calls `csp_listen()` with backlog = 5
3. Blocks on `csp_accept()` with `CSP_MAX_TIMEOUT`
4. For each accepted connection, drains packets with `csp_read()` (50 ms timeout)
5. Dispatches each packet to `process_command_packet()`
6. Closes the connection with `csp_close()`

### 11.2 Packet Structure

`csp_command_packet_t` — variable length up to 33 bytes, `__attribute__((packed))`:

```c
typedef struct __attribute__((packed))
{
  uint8_t cmd_id;       /* 1 B — command identifier (command_id_t) */
  uint8_t payload[32];  /* 0–32 B — command-specific payload        */
} csp_command_packet_t;
```

Minimum valid packet length: 1 byte (cmd_id only). Packets with `length < 1`
are silently dropped.

### 11.3 Command Dictionary

| cmd_id | Symbol        | Payload      | Action                                     | Response         |
|--------|---------------|--------------|--------------------------------------------|------------------|
| 1      | `CMD_ECHO`    | 0–32 bytes   | Echoes packet back to sender via `csp_send()` | Echoed packet |
| 2      | `CMD_REBOOT`  | none         | 100 ms delay, then `watchdog_reboot(0,0,10)` on Pico; logs simulation on host | None |
| 3      | `CMD_SET_MODE`| 1 byte (mode)| Logs requested mode; **not yet fully implemented** — tracked as OI-3 | None |
| —      | unknown       | —            | Packet dropped; warning logged             | None             |

### 11.4 CMD_REBOOT Details

On the Pico build, `CMD_REBOOT` calls `watchdog_reboot(0, 0, 10)` after a
100 ms flush delay (`vTaskDelay(pdMS_TO_TICKS(100))`). This resets the RP2350
via the hardware watchdog; no graceful FMM shutdown is performed.

> **OI-4 (High)**: `CMD_REBOOT` bypasses FMM — it does not call
> `fmm_request_transition(FM_SAFE)` before reboot. This risks corrupting
> in-flight actuator state. A safe-mode-then-reboot sequence should be
> implemented.

### 11.5 CMD_SET_MODE Details

`CMD_SET_MODE` currently only logs the requested mode and takes no action.
Full implementation requires integration with `fmm_request_transition()`.
Tracked as OI-3.

### 11.6 Memory Management

For `CMD_ECHO`, ownership of the CSP packet is transferred to `csp_send()`;
`packet` is set to NULL immediately after. For all other commands, if the
packet was not transferred, `csp_buffer_free(packet)` is called explicitly
before returning. This ensures no CSP buffer leaks on any code path.

---

## 12. Built-in CSP Services

libcsp v2.x registers the following services automatically on `csp_init()`:

| Port | Service        | Behaviour                                     |
|------|----------------|-----------------------------------------------|
| 0    | Ping           | Responds with identical payload, measures RTT |
| 1    | Process list   | Returns FreeRTOS task list (Pico: via `vTaskList`) |
| 2    | Memory free    | Returns `xPortGetFreeHeapSize()` value        |
| 3    | Reboot         | Soft reboot via CSP (see OI-4 for hard reboot)|
| 4    | Buffer free    | Returns CSP buffer pool free count            |
| 5    | Uptime         | Returns `xTaskGetTickCount()` in ticks        |

These services operate independently of `vCommandTask` and respond on their
respective ports without application code.

---

## 13. FreeRTOS Task Model

| Task              | Function           | Stack  | Priority                   | Period  |
|-------------------|--------------------|--------|----------------------------|---------|
| `TelemetryTask`   | `vTelemetryTask`   | 2048 W | `tskIDLE_PRIORITY + 2`     | 1000 ms |
| `CommandTask`     | `vCommandTask`     | 2048 W | `tskIDLE_PRIORITY + 2`     | Blocked |
| `CSPRouter`       | `vCspRouterTask`   | 1024 W | `tskIDLE_PRIORITY + 3`     | 1 ms    |
| `UART_RX`         | `uart_rx_task`     | 1024 W | `configMAX_PRIORITIES - 1` | Polling |

> **Priority rationale**: `CSPRouter` runs at priority 3 to forward packets
> before `TelemetryTask` and `CommandTask` (priority 2) process them. `UART_RX`
> runs at maximum priority to avoid RX FIFO overflow at 115200 baud.

### 13.1 Startup Order

Tasks are created in `main()` (or `obc_firmware_main()`) in the following order:

1. `comm_init()` — registers CSP + KISS interface, creates `vCspRouterTask` and `UART_RX`
2. `vTelemetryTask` — created by application entry point
3. `vCommandTask` — created by application entry point

The CSP router and UART driver must be online before the application tasks
begin sending/receiving.

---

## 14. Initialization Sequence

```
main()
  │
  ├── data_layer_init()           // DLA bus ready
  ├── flight_mode_manager_init()  // FMM ready
  ├── fault_manager_init()        // Fault manager ready
  ├── eps_monitor_init()          // EPS monitoring ready
  │
  ├── comm_init()
  │     ├── csp_init()            // CSP stack, loopback iface registered
  │     ├── [host only]
  │     │     ├── csp_usart_open_and_add_kiss_interface()  // KISS iface
  │     │     └── csp_rtable_load("1/255 KISS")            // routing
  │     └── [Pico only]
  │           └── xTaskCreate(vCspRouterTask)              // CSP router task
  │
  ├── xTaskCreate(vTelemetryTask)
  ├── xTaskCreate(vCommandTask)
  │
  └── vTaskStartScheduler()
```

---

## 15. Host Build Behavior

When `PICO_BUILD` is **not** defined (host unit test build):

| Feature              | Pico build                     | Host build                            |
|----------------------|--------------------------------|---------------------------------------|
| CSP router task      | `vCspRouterTask` created       | Not created; `csp_route_work()` not called |
| UART/KISS interface  | Loopback only (Pico `#else`)   | `csp_usart_open_and_add_kiss_interface()` on `"uart1"` |
| Telemetry send       | `csp_buffer_free()` (no route) | `csp_sendto()` to GN_ADDRESS=1        |
| Reboot command       | `watchdog_reboot()`            | `printf("Simulating reboot")`         |
| Timestamp            | `to_ms_since_boot()`           | `xTaskGetTickCount() * portTICK_PERIOD_MS` |

> **Note — host UART**: the host build calls `csp_usart_open_and_add_kiss_interface()`
> against a mock that asserts device `"uart1"`, baud 115200, and interface name
> `"KISS"` (verified by `test_comm_init.c`).

---

## 16. Error Handling

| Error Condition               | Detection                          | Response                                 |
|-------------------------------|------------------------------------|------------------------------------------|
| KISS interface open failure   | `res != CSP_ERR_NONE`              | Log error, return early; routing table not loaded |
| CSP buffer pool exhausted     | `csp_buffer_get()` returns NULL    | Log warning in `vTelemetryTask_Step()`; packet not sent |
| Invalid uplink packet length  | `packet->length < 1`               | `csp_buffer_free(packet)`; return        |
| Unknown command ID            | `default:` in switch               | Log warning; `csp_buffer_free(packet)` |
| `csp_accept()` timeout        | Returns NULL                       | `continue` (no action; task blocks again)|

No faults are raised to the Fault Manager for communication errors in the
current implementation. OI-5 tracks adding `FAULT_COMM_*` IDs for persistent
downlink loss or repeated buffer exhaustion.

---

## 17. Test Mapping

### 17.1 CSP Initialization — `tests/unit/test_comm_init.c`

| Test ID  | Test Function              | Verifies                                              |
|----------|----------------------------|-------------------------------------------------------|
| T-COM-01 | `test_comm_init_success`   | `csp_init()`, KISS open, routing table all called     |
| T-COM-02 | `test_comm_init_failure`   | KISS failure → routing table NOT loaded; early return |

### 17.2 Telemetry Downlink — `tests/unit/test_telemetry.c`

| Test ID  | Test Function                    | Verifies                                                             |
|----------|----------------------------------|----------------------------------------------------------------------|
| T-TLM-01 | `test_tlm_full_packet_in_nominal`| Full ADCS fields in packet at FM_NOMINAL; CSP address/port/priority  |
| T-TLM-02 | `test_tlm_hk_only_in_safe`       | Attitude/rates zeroed in FM_SAFE; temperature preserved              |
| T-TLM-03 | `test_tlm_sends_in_all_modes`    | `csp_sendto` called in every flight mode                             |
| T-TLM-04 | `test_tlm_energy_state_in_flags` | 4 energy states correctly encoded in flags[3:2]                      |
| T-TLM-05 | `test_tlm_flags_imu_temp`        | Flags bits 0–1 reflect `imu_valid` / `temp_valid`                   |
| T-TLM-06 | `test_tlm_null_buffer`           | No `csp_sendto` when `csp_buffer_get()` returns NULL                 |

### 17.3 Command Uplink — `tests/unit/test_command.c`

| Test ID  | Test Function                    | Verifies                                                  |
|----------|----------------------------------|-----------------------------------------------------------|
| T-CMD-01 | `test_echo_command`              | CMD_ECHO: `csp_send()` called; packet ownership transferred |
| T-CMD-02 | `test_reboot_command`            | CMD_REBOOT: `vTaskDelay(100 ms)` + simulated reboot logged |
| T-CMD-03 | `test_set_mode_command`          | CMD_SET_MODE: logged, no crash                            |
| T-CMD-04 | `test_unknown_command`           | Unknown cmd_id: packet freed, no send                     |
| T-CMD-05 | `test_min_length_packet`         | `length < 1`: packet freed, no dispatch                   |

Total: **13 / 13 tests passing** (T-COM-01..02, T-TLM-01..06, T-CMD-01..05).

---

## 18. Open Items

| OI   | Severity | Phase    | Status | Description                                                                 |
|------|----------|----------|--------|-----------------------------------------------------------------------------|
| OI-1 | Low      | Phase 2  | Open   | `pico_usart.c` hard-codes TX/RX pin numbers (GPIO8/9); migrate to `UART1_TX_PIN` / `UART1_RX_PIN` from `pico_pins.h`. |
| OI-2 | Low      | Phase 3  | Open   | UART RX polling loop burns CPU at high baud; replace with UART RX IRQ + ring buffer. |
| OI-3 | High     | Phase 2  | Open   | `CMD_SET_MODE` is a stub — implement full `fmm_request_transition()` integration for ground-commanded mode changes. |
| OI-4 | High     | Phase 2  | Open   | `CMD_REBOOT` bypasses FMM; add `fmm_request_transition(FM_SAFE)` before reboot to ensure clean actuator shutdown. |
| OI-5 | Medium   | Phase 3  | Open   | No fault IDs for persistent downlink loss / CSP buffer exhaustion; add `FAULT_COMM_*` entries to `fault_ids.h`. |
| OI-6 | Low      | Phase 3  | Open   | Telemetry packet does not include a sequence counter; add `uint16_t seq` to `csp_telemetry_packet_t` for gap detection. |

---

## 19. References

1. PHASE3_COMM_SPEC — Phase 3 Communication Specification (internal,
   `docs/PHASE3_COMM_SPEC.md`)
2. ICD-OBC-001 v1.1 — Interface Control Document (`docs/ecss/design/ICD-OBC-001.md`)
3. SAD-OBC-001 v1.0 — Software Architecture Document (`docs/ecss/design/SAD-OBC-001.md`)
4. DL-DES-001 v0.1 — Data Layer Design Document (`docs/ecss/design/DL-DES-001.md`)
5. FMM-DES-001 v0.3 — FMM Design Document (`docs/ecss/design/FMM-DES-001.md`)
6. libcsp v2.x — CubeSat Space Protocol library (`third_party/libcsp/`)
7. KISS Protocol — TNC-2 Interface Specification (TAPR, 1987)
8. ECSS-E-ST-40C — Software Engineering Standard
9. pico_pins.h — GPIO Pin Definitions (`config/pico_pins.h`)
