# OBC Hardware & CDH Design Document

**Document ID**: OBC-DES-001
**Version**: 0.1
**Date**: 2026-03-07
**Status**: Draft — CDR In Progress
**Standard**: ECSS-E-ST-40C §5.3, ECSS-E-ST-10-02C
**Project**: CubeSat OBC Flight Software (RP2350 / Pico 2W)

---

## Table of Contents

1. [Introduction](#1-introduction)
2. [Applicable Documents](#2-applicable-documents)
3. [Acronyms and Definitions](#3-acronyms-and-definitions)
4. [System Context](#4-system-context)
5. [Hardware Platform](#5-hardware-platform)
6. [Memory Map](#6-memory-map)
7. [GPIO Pin Assignment](#7-gpio-pin-assignment)
8. [Bus Architecture](#8-bus-architecture)
9. [FreeRTOS Configuration](#9-freertos-configuration)
10. [Task Model and Scheduling](#10-task-model-and-scheduling)
11. [Boot and Initialisation Sequence](#11-boot-and-initialisation-sequence)
12. [Power Architecture](#12-power-architecture)
13. [Firmware Build System](#13-firmware-build-system)
14. [Host vs Target Build Differences](#14-host-vs-target-build-differences)
15. [Performance Budget](#15-performance-budget)
16. [Open Items](#16-open-items)
17. [References](#17-references)

---

## 1. Introduction

### 1.1 Purpose

This document describes the hardware platform and Command & Data Handling (CDH)
architecture of the CubeSat OBC. It defines the processor, memory, peripheral
interfaces, GPIO allocation, power rail, boot sequence, FreeRTOS configuration,
and task scheduling model. It is a CDR deliverable per ECSS-E-ST-40C §5.3.

### 1.2 Scope

This document covers:

- The Raspberry Pi Pico 2W (RP2350) hardware platform
- All peripheral assignments (I2C, UART, PWM, ADC, GPIO)
- FreeRTOS kernel configuration (`config/FreeRTOSConfig.h`)
- Task model: creation, priorities, stack sizes, periods
- Boot sequence from `main()` through `vStartupTask()`
- Host simulation build differences (`PICO_BUILD` guard)
- Performance and timing budget

It does **not** cover:
- ADCS algorithm design (→ ADCS-DES-001)
- Communications / TT&C (→ COMMS-DES-001)
- EPS electronics detailed design (→ EPS-DES-001)
- Fault management software (→ FAULT-DES-001)

### 1.3 Document Status

This is version 0.1 — initial CDR draft. All content reflects the implemented
firmware as of dev `c09eaa9`. Open items are listed in Section 16.

---

## 2. Applicable Documents

| ID | Title | Version |
|---|---|---|
| SAD-OBC-001 | System Architecture Document | 1.0 |
| BOM-OBC-001 | Bill of Materials | 1.0.1 |
| ICD-OBC-001 | Interface Control Document | 1.1 |
| ADCS-DES-001 | ADCS Design Document | 1.0 |
| COMMS-DES-001 | Communications / TT&C Design Document | 0.1 |
| EPS-DES-001 | Electrical Power System Design Document | 0.2 |
| FAULT-DES-001 | Fault Manager Design Document | 0.2 |
| SDP-OBC-001 | Software Development Plan | 1.0 |

---

## 3. Acronyms and Definitions

| Acronym | Definition |
|---|---|
| CDH | Command and Data Handling |
| OBC | On-Board Computer |
| MCU | Microcontroller Unit |
| SMP | Symmetric Multi-Processing |
| FPU | Floating-Point Unit |
| SRAM | Static Random-Access Memory |
| RTOS | Real-Time Operating System |
| HAL | Hardware Abstraction Layer |
| DLA | Data Layer Abstraction |
| CSP | CubeSat Space Protocol |
| EPS | Electrical Power System |
| ADCS | Attitude Determination and Control System |
| PWM | Pulse-Width Modulation |
| ADC | Analog-to-Digital Converter |
| CDC | Communications Device Class (USB) |
| WDT | Watchdog Timer |
| HWM | High Water Mark (FreeRTOS stack metric) |
| WCET | Worst-Case Execution Time |
| DWT | Data Watchpoint and Trace (ARM Cortex-M timing unit) |
| TID | Total Ionizing Dose |
| SEE | Single-Event Effect |
| COTS | Commercial Off-The-Shelf |

---

## 4. System Context

```
                        ┌─────────────────────────────────┐
                        │      Raspberry Pi Pico 2W       │
                        │         (RP2350 / OBC)          │
                        │                                 │
   Ground Station ◄────►│ UART1 GPIO8/9 (CSP/KISS TT&C)   │
                        │                                 │
   USB Debug Host ◄────►│ USB CDC (UART0 stdout)          │
                        │                                 │
   MPU-6050 IMU ◄──────►│ I2C0 GPIO4/5 (400 kHz)          │
   HMC5883L Mag ◄──────►│                                 │
                        │                                 │
   Reaction Wheel ×3 ◄──│ PWM GPIO6/7/10                  │
   Magnetorquer ×3 ◄────│ PWM GPIO14/15/16                │
                        │                                 │
   Vbatt sense ─────────│ ADC0 GPIO26                     │
   Ext. Watchdog KA ◄───│ GPIO20 (WDI kick)               │
   Status LED ◄─────────│ GPIO25 (CYW43439)               │
                        └─────────────────────────────────┘
```

---

## 5. Hardware Platform

### 5.1 Processor

| Parameter | Value |
|---|---|
| Board | Raspberry Pi Pico 2W |
| SoC   | RP2350 |
| CPU   | Dual Cortex-M33 (ARMv8-M Mainline, Thumb-2) |
| Clock | 133 MHz (default SDK); up to 150 MHz |
| FPU   | FPv5-SP-D16 (single-precision hardware) |
| Architecture | Armv8-M |
| Wireless | CYW43439 (Wi-Fi 802.11 b/g/n + Bluetooth 5.2) |

The RP2350 contains two Cortex-M33 cores. At the SRR/CDR baseline, **only Core 0
is active** — `configNUMBER_OF_CORES = 1`. See OI-1.

### 5.2 Memory

| Resource | Size | Used (v0.7.1, no IMU) |
|---|---|---|
| Flash | 2 MB (internal QSPI) | ~536 KB (full UF2 image) |
| SRAM | 520 KB | — |
| Heap (FreeRTOS) | 60 KB (configured) | ~3.6 KB allocated (60,416 bytes free) |
| Task stack total | 9 × 2048 words × 4 B = 72 KB | see §10 |

### 5.3 Wireless Module

The CYW43439 Wi-Fi/BT chip is on an SPI bus internal to the Pico 2W. Firmware
accesses it through `pico/cyw43_arch.h`. Current use:

- Status LED: `cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, val)`
- Wi-Fi: not used for TT&C (UART1/KISS used instead). Future option for
  ground testing at short range.

`cyw43_arch_poll()` is called from `vLedBlinkTask` immediately after each GPIO
write to flush the SPI command before the task yields.

### 5.4 Radiation and Reliability Note

The Pico 2W is **COTS (not space-grade)**. For flight:

- TID and SEE susceptibility must be characterized against the mission radiation
  environment (SSO 600 km, ~97° inclination)
- Conformal coating and latch-up protection on power rails recommended
- Software watchdog (FreeRTOS stack overflow detection + external TPS3431/GPIO20)
  provides the primary reset path in case of SEU-induced hangs

---

## 6. Memory Map

### 6.1 Flash Layout (RP2350, 2 MB)

```
0x10000000 ┌───────────────────────────┐
           │  Pico SDK bootloader (2nd)│  ~8 KB
0x10002000 ├───────────────────────────┤
           │  Application code + data  │  ~520 KB (typical)
           │  (.text, .rodata, .data)  │
           │                           │
           ├───────────────────────────┤
           │  (free flash)             │
0x10200000 └───────────────────────────┘ (2 MB end)
```

Flash backend (`src/core/flash_backend_stub.c`) — stub implementation currently.
Full flash write/erase (for parameter storage) deferred to CDR/TRR.

### 6.2 SRAM Layout (RP2350, 520 KB)

```
0x20000000 ┌───────────────────────────┐
           │  .bss + .data (static)    │
           │  (system_state, services) │
           ├───────────────────────────┤
           │  FreeRTOS heap (60 KB)    │
           │  (task stacks, TCBs,      │
           │   CSP buffers, queues)    │
           ├───────────────────────────┤
           │  (free SRAM)              │
0x20082000 └───────────────────────────┘ (520 KB end)
```

Minimum free heap measured on hardware: **60,416 bytes** at v0.7.1 with all
9 tasks running.

---

## 7. GPIO Pin Assignment

Complete RP2350 / Pico 2W GPIO assignment as defined in `config/pico_pins.h`:

| GPIO | Function | Direction | Peripheral | Notes |
|---|---|---|---|---|
| GPIO0 | UART0 TX | OUT | UART0 | Debug / CDC stdout (`UART0_TX_PIN`) |
| GPIO1 | UART0 RX | IN | UART0 | Debug / CDC stdin (`UART0_RX_PIN`) |
| GPIO2 | I2C1 SDA | Bidir | I2C1 | Future expansion (`I2C1_SDA_PIN`) |
| GPIO3 | I2C1 SCL | Bidir | I2C1 | Future expansion (`I2C1_SCL_PIN`) |
| GPIO4 | I2C0 SDA | Bidir | I2C0 | MPU-6050 + HMC5883L (`I2C0_SDA_PIN`) |
| GPIO5 | I2C0 SCL | Bidir | I2C0 | MPU-6050 + HMC5883L (`I2C0_SCL_PIN`) |
| GPIO6 | PWM3A | OUT | PWM | Reaction Wheel motor 1 (`RW_MOTOR1_PIN`) |
| GPIO7 | PWM3B | OUT | PWM | Reaction Wheel motor 2 (`RW_MOTOR2_PIN`) |
| GPIO8 | UART1 TX | OUT | UART1 | CSP/KISS TT&C (`UART1_TX_PIN`) |
| GPIO9 | UART1 RX | IN | UART1 | CSP/KISS TT&C (`UART1_RX_PIN`) |
| GPIO10 | PWM5A | OUT | PWM | Reaction Wheel motor 3 (`RW_MOTOR3_PIN`) |
| GPIO14 | PWM7A | OUT | PWM | Magnetorquer X axis (`MAG_X_PIN`) |
| GPIO15 | PWM7B | OUT | PWM | Magnetorquer Y axis (`MAG_Y_PIN`) |
| GPIO16 | PWM0A | OUT | PWM | Magnetorquer Z axis (`MAG_Z_PIN`) |
| GPIO20 | WDI kick | OUT | GPIO | External watchdog (`WATCHDOG_PIN`) |
| GPIO25 | Status LED | OUT | CYW43439 | Onboard LED via Wi-Fi chip |
| GPIO26 | ADC0 | IN | ADC | Battery voltage sense (`ADC_VBATT_PIN`) |
| GPIO27 | ADC1 | IN | ADC | External temp sensor (`ADC_TEMP_PIN`) |
| ADC4 | Temp (int) | IN | ADC | RP2350 internal temperature sensor |

**Conflict note**: GPIO4/GPIO5 are assigned to I2C0 in `pico_pins.h`. The BOM
(§11) flags that I2C0 and UART1 are separate build configurations (or
time-multiplexed) on GPIO4/GPIO5. Current firmware activates I2C0 only;
UART1 is on GPIO8/GPIO9. See OI-2.

**I2C pull-ups**: 4.7 kΩ pull-up resistors on GPIO4 (SDA) and GPIO5 (SCL) are
**required** for both MPU-6050 and HMC5883L. See BOM-OBC-001 §10 item #9.

---

## 8. Bus Architecture

### 8.1 I2C0 — Sensor Bus

| Property | Value |
|---|---|
| Peripheral | `i2c0` |
| Pins | GPIO4 (SDA), GPIO5 (SCL) |
| Speed | 400 kHz (Fast Mode) |
| Pull-ups | 4.7 kΩ (external, required) |
| Devices | MPU-6050 (addr 0x68), HMC5883L (addr 0x1E) |
| Driver | `src/drivers/i2c/pico_i2c.c` (Pico), `src/drivers/i2c/host_i2c.c` (host stub) |

The I2C bus is initialised in `vStartupTask` via `i2c_bus_init(I2C_SDA_PIN, I2C_SCL_PIN, 400000)`.
Both devices share the bus; the sensor read task accesses them sequentially (no
concurrent I2C transactions from multiple tasks).

### 8.2 UART0 — Debug Console

| Property | Value |
|---|---|
| Peripheral | `uart0` |
| Pins | GPIO0 (TX), GPIO1 (RX) |
| Baud | 115200 8N1 |
| Use | `stdio_init_all()` debug output / USB CDC |
| Driver | Pico SDK `pico/stdio_uart.h` |

Firmware uses `printf()` for debug output. On Pico hardware, USB CDC is the
primary debug path (`stdio_usb`). `stdout` is set unbuffered (`_IONBF`) to
ensure every `printf` is immediately flushed to USB.

### 8.3 UART1 — TT&C (CSP/KISS)

| Property | Value |
|---|---|
| Peripheral | `uart1` |
| Pins | GPIO8 (TX), GPIO9 (RX) |
| Baud | 115200 8N1 |
| Protocol | CSP v2 over KISS framing |
| Driver | `src/drivers/uart/pico_usart.c` |

See COMMS-DES-001 for full TT&C protocol stack design.

### 8.4 PWM — Actuator Control

Six PWM channels drive reaction wheel motors and magnetorquer coils:

| Channel | GPIO | Function | Driver |
|---|---|---|---|
| PWM3A | GPIO6 | RW Motor 1 | `src/actuators/reaction_wheel.c` |
| PWM3B | GPIO7 | RW Motor 2 | `src/actuators/reaction_wheel.c` |
| PWM5A | GPIO10 | RW Motor 3 | `src/actuators/reaction_wheel.c` |
| PWM7A | GPIO14 | MTQ X | `src/actuators/magnetorquer.c` |
| PWM7B | GPIO15 | MTQ Y | `src/actuators/magnetorquer.c` |
| PWM0A | GPIO16 | MTQ Z | `src/actuators/magnetorquer.c` |

PWM driver hardware initialization is Pico-only (`PICO_BUILD`). On host builds,
torque/dipole commands are accepted and logged but no PWM output occurs.

### 8.5 ADC — Power Monitoring

| Channel | GPIO | Signal | Use |
|---|---|---|---|
| ADC0 | GPIO26 | V_batt / divider | `eps_monitor` battery state |
| ADC1 | GPIO27 | Ext. temperature | Optional external sensor |
| ADC4 | Internal | RP2350 die temp | Temperature task |

Voltage sense: resistor divider `R1=330 kΩ, R2=100 kΩ` →
$V_{ADC} = V_{batt} \times \frac{R_2}{R_1+R_2} = V_{batt} \times 0.233$.
At `V_batt = 4.2 V` (full): $V_{ADC} = 0.98\,V$.
Temperature driver: `src/drivers/temperature/pico_temp.c` (RP2350 ADC4).

---

## 9. FreeRTOS Configuration

Key parameters from `config/FreeRTOSConfig.h`:

| Parameter | Value | Rationale |
|---|---|---|
| `configUSE_PREEMPTION` | 1 | Preemptive scheduling |
| `configNUMBER_OF_CORES` | **1** | SMP disabled (OI-1) |
| `portUSE_SECURE_CONTEXT` | 0 | ARM_CM33_NTZ (no TrustZone) |
| `configCPU_CLOCK_HZ` | 133,000,000 | 133 MHz |
| `configTICK_RATE_HZ` | 1000 | 1 ms tick resolution |
| `configMAX_PRIORITIES` | 5 | Valid range: 0 (Idle) to 4 |
| `configMINIMAL_STACK_SIZE` | 128 words | Idle task |
| `configTOTAL_HEAP_SIZE` | 60 × 1024 = 61,440 B | FreeRTOS heap |
| `configCHECK_FOR_STACK_OVERFLOW` | 2 | Fill + check on task switch |
| `configUSE_MALLOC_FAILED_HOOK` | 1 | Trap heap exhaustion |
| `configUSE_IDLE_HOOK` | 0 | No idle hook |
| `configUSE_TICK_HOOK` | 0 | No tick hook |
| `configUSE_STATS_FORMATTING_FUNCTIONS` | 0 | No runtime stats |
| `configUSE_TASK_NOTIFICATIONS` | 1 | Used in command task |

**Port**: `ARM_CM33_NTZ` — ARM Cortex-M33 without TrustZone secure world.
Selected specifically to fix PendSV stack corruption observed with the `ARM_CM0`
port on RP2350 (SyRS-OBC-001 SYS-P-002 rationale).

**Stack overflow detection**: When `configCHECK_FOR_STACK_OVERFLOW = 2`, FreeRTOS
fills the stack with a canary pattern and checks it on every context switch. If
overflow is detected, `vApplicationStackOverflowHook(task, name)` is called, which
writes the task name to `watchdog_hw->scratch[1..3]`, sets magic `0xDEAD0001` in
`scratch[0]`, and triggers a watchdog reset. On next boot, `main()` reads and
prints the offending task name.

---

## 10. Task Model and Scheduling

### 10.1 Task Summary

All tasks are created inside `vStartupTask` (priority 4) after the FreeRTOS
scheduler starts. This ensures all kernel primitives (spinlock on RP2350) are
fully initialized before any queue, semaphore, or mutex creation.

| Task | Function | Priority | Stack (words) | Period | HWM (free words) | Stack used (B) |
|---|---|---|---|---|---|---|
| Startup | `vStartupTask` | 4 → 1† | 2048 | one-shot → 5 s idle | — | — |
| SensorRead | `vSensorReadTask` | 4 | 2048 | 100 ms (10 Hz) | 1908 | 560 |
| AttitudeCtrl | `vAttitudeControlTask` | 3 | 2048 | 100 ms (10 Hz) | 1952 | 384 |
| Telemetry | `vTelemetryTask` | 2 | 2048 | 500 ms (2 Hz) | 1868 | 720 |
| Command | `vCommandTask` | 2 | 2048 | event-driven (CSP) | 1894 | 616 |
| HealthMon | `vHealthMonitorTask` | 1 | 2048 | 5000 ms (0.2 Hz) | 1974 | 296 |
| LEDBlink | `vLedBlinkTask` | 1 | 2048 | 200/800 ms (Pico) | 1947 | 404 |
| Heartbeat | `vHeartbeatTask` | 1 | 2048 | 2000 ms (Pico) | 1930 | 472 |
| CSPRouter | `vCspRouterTask` | 3 | 2048 | event-driven | — | — |
| UART_RX | KISS poll loop | 4 | 2048 | polling | — | — |

† `vStartupTask` lowers its own priority to 1 via `vTaskPrioritySet(NULL, tskIDLE_PRIORITY + 1)`
after task creation, then loops printing ALIVE heartbeats every 5 s. This avoids
the `vTaskDelete` code path, which has known issues on the SMP port with 1 core.

### 10.2 Priority Assignment Rationale

| Priority | Level | Tasks | Rationale |
|---|---|---|---|
| 4 | Critical | SensorRead, UART_RX, Startup (init) | Sensor data freshness; UART RX must never drop KISS bytes |
| 3 | High | AttitudeCtrl, CSPRouter | Control law runs after fresh sensor data; CSP above normal tasks |
| 2 | Normal | Telemetry, Command | Downlink and uplink at standard priority |
| 1 | Low | HealthMon, LEDBlink, Heartbeat, Startup (idle) | Background / diagnostic tasks |
| 0 | Idle | FreeRTOS idle task | CPU idle when no runnable tasks |

### 10.3 Timing Budget

| Task | Rate | Measured worst-case | CPU Budget |
|---|---|---|---|
| SensorRead | 10 Hz | ~200 µs (host) | Validated Phase 2; HW WCET pending (OI-3) |
| AttitudeCtrl | 10 Hz | ~500 µs (EKF + LQR, host) | Validated Phase 4 |
| Telemetry | 2 Hz | ~50 µs (CSP prep) | Negligible |
| Command | event | ~30 µs | Negligible |
| HealthMon | 0.2 Hz | ~10 µs | Negligible |

Control loop jitter measured in Phase 2: **± 22 µs** on hardware at 10 Hz — well
within the NFR-1 requirement of < 10 ms jitter.

FreeRTOS timer task runs at `configMAX_PRIORITIES - 2 = 3`, same level as
AttitudeCtrl. Software timers used: none in the current firmware.

---

## 11. Boot and Initialisation Sequence

```
main()
  │
  ├── [Pico only] stdio_init_all() — USB CDC + UART0 init
  ├── [Pico only] wait USB connect (30 × 100 ms)
  ├── [Pico only] check watchdog_hw->scratch[0] for overflow magic
  │
  ├── xTaskCreate(vStartupTask, prio=4)
  │
  └── vTaskStartScheduler()  ← kernel spinlocks init here
          │
          └── vStartupTask()    [Priority 4]
                │
                ├── system_state_init()
                │     └── zero DLA state; set FM = FM_BOOT
                │
                ├── comm_init()
                │     ├── csp_init(OBC_ADDRESS=10)
                │     ├── csp_usart_open_and_add_kiss_interface(UART1)
                │     ├── csp_rtable_load("1/255 KISS")
                │     └── [Pico] xTaskCreate(vCspRouterTask, prio=3)
                │
                ├── fault_manager_init()
                │     └── zero 32-slot fault table
                │
                ├── eps_monitor_init()
                │     └── init EPS state machine (NOMINAL)
                │
                ├── [Pico only] i2c_bus_init(GPIO4, GPIO5, 400000)
                │
                ├── mpu6050_init()  → system_state_set_available(imu_ok, ...)
                ├── temperature_init()
                │
                ├── xTaskCreate(vSensorReadTask,      prio=4)
                ├── xTaskCreate(vAttitudeControlTask, prio=3)
                ├── xTaskCreate(vTelemetryTask,       prio=2)
                ├── xTaskCreate(vCommandTask,         prio=2)
                ├── xTaskCreate(vHealthMonitorTask,   prio=1)
                ├── [Pico] xTaskCreate(vLedBlinkTask, prio=1)
                ├── [Pico] xTaskCreate(vHeartbeatTask,prio=1)
                │
                └── vTaskPrioritySet(NULL, 1)  → becomes ALIVE loop
```

**Boot time target**: operational state within 5 s of power-on (NFR-7). USB CDC
enumeration wait is capped at 3 s; remaining subsystem init is < 100 ms.

---

## 12. Power Architecture

Refer to EPS-DES-001 for the full EPS software design. This section summarises
the hardware power topology for completeness in the OBC context.

### 12.1 Power Rail Summary

| Rail | Voltage | Source | Loads |
|---|---|---|---|
| VBATT | 3.0–4.2 V | LiPo 18650 (Samsung 30Q, 3000 mAh) | MT3608 boost, TP4056 charger |
| 5V bus | 5 V | MT3608 boost converter | OBC Pico 2W, sensors, actuator drivers |
| 3.3 V | 3.3 V | Pico 2W onboard LDO (VREG_OUT) | MCU I/O, I2C pull-ups |
| Solar input | ~5.5 V | 6V 1W mono panel + 1N5819 diode | TP4056 charger (Vin = 4.5–8 V) |

### 12.2 Battery Voltage Monitoring

VBATT is sensed via resistor divider `R1=330 kΩ, R2=100 kΩ` on ADC0/GPIO26:

$$V_{ADC} = V_{batt} \times \frac{100}{330+100} = V_{batt} \times 0.233$$

The EPS monitor (`src/services/eps/eps_monitor.c`) reads this ADC channel and
implements a 4-state hysteresis FSM:

| State | Vbatt threshold | Firmware action |
|---|---|---|
| `ENERGY_NOMINAL` | > 3.6 V | All subsystems nominal |
| `ENERGY_LOW` | 3.3–3.6 V | Log low-battery warning |
| `ENERGY_CRITICAL` | 3.0–3.3 V | Raise `FAULT_EPS_VBATT_CRITICAL` → FMM → SAFE |
| `ENERGY_EMERGENCY` | < 3.0 V | Raise `FAULT_EPS_VBATT_EMERGENCY` (FAULT_LEVEL_CRITICAL) |

### 12.3 Power Consumption Estimates

| Scenario | Consumption |
|---|---|
| OBC + comms only (idle) | ~0.5 W |
| Nominal (all subsystems active) | ~2 W |
| Maximum (full actuator drive) | ~3.5 W |

At 2 W nominal with 3000 mAh @ 3.7 V (11.1 Wh): **~5.5 h autonomy** from battery.
For flight LEO (600 km SSO, 35 min eclipse): minimum battery capacity
$= 2\,\text{W} \times 35/60\,\text{h} = 1.17\,\text{Wh}$; sizing target with
50% margin = **1.75 Wh** (see BOM-OBC-001 §9 for flight panel sizing analysis).

### 12.4 External Watchdog

The TPS3431 (or equivalent MCP1316/MAX706) hardware watchdog is connected on
GPIO20 (WDI input). The software kick is performed by `watchdog_hal_feed()` called
from `vHealthMonitorTask` every 5 s. Watchdog timeout: **3 s**. If the firmware
hangs and HealthMonitorTask misses its kick, the TPS3431 asserts hardware reset.

Software watchdog: RP2350 internal watchdog. FreeRTOS stack overflow
(`configCHECK_FOR_STACK_OVERFLOW = 2`) writes the task name to watchdog scratch
registers and triggers an internal watchdog reset. See §9 and §11.

---

## 13. Firmware Build System

### 13.1 CMake Targets

| Build Dir | PICO_BUILD | Target | Artefact |
|---|---|---|---|
| `build/` | OFF | Host test firmware + unit tests | `build/src/cubesat_obc_firmware`, `build/tests/unit/test_*` |
| `build_pico/` | ON | Pico flash image | `build_pico/src/cubesat_obc_pico.uf2` |
| `build_emu/` | ON (partial) | Emulator image | `build_emu/` |

### 13.2 Conditional Compilation

The `PICO_BUILD` CMake variable controls platform-specific code inclusion:

```c
#ifdef PICO_BUILD
    // Pico SDK hardware code:
    // - gpio, i2c, uart, watchdog, cyw43, pwm
    // - csp_usart_open_and_add_kiss_interface()
    // - vCspRouterTask, vLedBlinkTask, vHeartbeatTask creation
    // - stdio_init_all(), USB CDC wait loop
    // - i2c_bus_init()
    // - watchdog_hw scratch registers
#else
    // Host stubs:
    // - src/drivers/i2c/host_i2c.c
    // - src/drivers/temperature/host_temp.c
    // - src/core/flash_backend_stub.c
    // - src/services/watchdog/watchdog_hal_host.c
    // - CSP router task NOT created (no UART)
#endif
```

### 13.3 Key Source Files

| File | Description |
|---|---|
| `src/obc_main.c` | `main()` + `vStartupTask` — entry point and task creation |
| `src/freertos_hooks.c` | Stack overflow hook, malloc failed hook |
| `config/FreeRTOSConfig.h` | FreeRTOS kernel configuration |
| `config/pico_pins.h` | All GPIO/I2C/UART/ADC pin definitions |
| `config/library.cmake` | CMake module definitions per subsystem |

---

## 14. Host vs Target Build Differences

| Feature | Host (PICO_BUILD=OFF) | Target (PICO_BUILD=ON) |
|---|---|---|
| I2C bus | `host_i2c.c` stub (returns mock data) | `pico_i2c.c` (real `i2c0` hardware) |
| UART (TT&C) | No UART driver; CSP router not started | `pico_usart.c` + UART1 GPIO8/9 |
| Temperature | `host_temp.c` stub (returns 25.0°C) | `pico_temp.c` (RP2350 ADC4) |
| Flash backend | `flash_backend_stub.c` (RAM buffer) | RP2350 internal flash (planned) |
| Watchdog HAL | `watchdog_hal_host.c` (no-op) | `watchdog_hal_pico.c` (RP2350 WDT) |
| LED control | Not created | `vLedBlinkTask` with CYW43439 |
| Heartbeat task | Not created | `vHeartbeatTask` (2 s printf) |
| CSP router task | Not created | Created in `comm_init()` |
| USB CDC wait | Not present | 30 × 100 ms USB enumeration wait |
| Stack overflow | FreeRTOS hook fires | Hook + writes watchdog scratch + reset |
| `printf()` output | `stdout` (terminal) | USB CDC (unbuffered) |

---

## 15. Performance Budget

### 15.1 Stack Usage (v0.7.1, Pico 2W, no IMU connected)

| Task | Stack (words) | HWM (free) | Max used (words) | Headroom |
|---|---|---|---|---|
| SensorRead | 2048 | 1908 | 140 | 93% |
| AttitudeCtrl | 2048 | 1952 | 96 | 95% |
| Telemetry | 2048 | 1868 | 180 | 91% |
| Command | 2048 | 1894 | 154 | 92% |
| HealthMon | 2048 | 1974 | 74 | 96% |
| LEDBlink | 2048 | 1947 | 101 | 95% |
| Heartbeat | 2048 | 1930 | 118 | 94% |

All tasks show ≥ 91% headroom. Stacks could be reduced to 512 words each if
memory pressure arises; current 2048-word sizing is conservative by design.

Peak stack user: Telemetry (180 words / 720 bytes) — driven by `printf` with floats
+ EKF state copy + CSP buffer handling.

### 15.2 Heap Usage

| Allocation | Size | Count | Total |
|---|---|---|---|
| FreeRTOS TCB | ~120 B | 9 tasks | ~1.1 KB |
| Task stacks | 2048 words × 4 B = 8 KB | 9 tasks | ~72 KB |
| CSP buffers | ~256 B each | configurable | ~4 KB |
| FreeRTOS heap configured | 60 KB | — | 61,440 B |
| Free at runtime (v0.7.1) | — | — | **60,416 B** |

Measured minimum ever free heap: 60,416 B — well above the 32 KB NFR floor
(SYS-P-004 in SyRS-OBC-001).

### 15.3 CPU Load Estimate (Core 0, 133 MHz)

| Task | Rate | Est. cycles/iter | CPU fraction |
|---|---|---|---|
| SensorRead (I2C + EKF) | 10 Hz | ~100 K | ~0.75% |
| AttitudeCtrl (LQR) | 10 Hz | ~50 K | ~0.37% |
| Telemetry | 2 Hz | ~5 K | ~0.008% |
| FreeRTOS overhead | — | ~1 K/tick | ~0.75% |
| **Total estimated** | — | — | **< 5%** |

Formal WCET measurement via DWT cycle counter is deferred to Phase 6 (OI-3).

---

## 16. Open Items

| ID | Description | Priority | Owner | Status |
|---|---|---|---|---|
| OI-1 | SMP (`configNUMBER_OF_CORES=2`) disabled — needs Phase 6 HIL boot stability validation before enabling | High | SW Lead | Open |
| OI-2 | GPIO4/GPIO5 used by I2C0; BOM §11 notes conflict with UART1 assignment on same pins — requires pin reservation audit and `pico_pins.h` update | High | HW/SW Lead | Open |
| OI-3 | WCET for all tasks not yet measured on hardware via DWT cycle counter — required before CDR timing budget sign-off | High | SW Lead | Open |
| OI-4 | Flash backend is a stub (`flash_backend_stub.c`) — full RP2350 flash read/write needed for parameter persistence | Medium | SW Lead | Open |
| OI-5 | External watchdog GPIO20 (TPS3431) not yet fully integrated in firmware kick loop — `watchdog_hal_pico.c` pending final GPIO assignment | Medium | HW/SW Lead | Open |
| OI-6 | `config.h` defines `I2C_SDA_PIN=16` / `I2C_SCL_PIN=17` — conflicts with `pico_pins.h` `I2C0_SDA_PIN=4` / `I2C0_SCL_PIN=5`. Requires alignment | Medium | SW Lead | Open |
| OI-7 | CPU load formal measurement not completed — DWT-based profiling needed for TRR | Low | SW Lead | Open |
| OI-8 | CYW43439 Wi-Fi not used for TT&C — decision to keep as LED-only or enable for ground testing to be documented | Low | HW/SW Lead | Open |

---

## 17. References

| Reference | Description |
|---|---|
| RP2350 Datasheet | Raspberry Pi RP2350 Technical Reference Manual |
| Pico 2W Datasheet | Raspberry Pi Pico 2W Product Brief |
| FreeRTOS Reference | FreeRTOS V10.x Reference Manual |
| ARM Cortex-M33 TRM | ARM Cortex-M33 Technical Reference Manual (DDI0553) |
| ECSS-E-ST-40C | Software Engineering Standard |
| SAD-OBC-001 | System Architecture Document v1.0 |
| BOM-OBC-001 | Bill of Materials v1.0.1 |
| COMMS-DES-001 | Communications Subsystem Design Document v0.1 |
| EPS-DES-001 | Electrical Power System Design Document v0.2 |
