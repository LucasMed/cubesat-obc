# System Architecture Document (SAD) v1.0
## CubeSat OBC Flight Software — Post v0.7.0 Implementation Alignment

**Document ID**: SAD-OBC-001  
**Version**: 1.0  
**Date**: 2026-03-04  
**Status**: ✅ Reflects implemented code (validated on Pico 2W hardware)  
**Branch**: `dev` @ commit `73f8ef8`

---

## 1. Scope

This document describes the **as-built** software architecture of the CubeSat On-Board
Computer (OBC) flight software as of v0.7.0. All blocks described here are implemented,
compiled, and verified against the 29/29 CTest suite and Pico 2W hardware.

Fields marked `[TBD-HW]` require additional hardware instrumentation (planned v1.0.0).

---

## 2. Hardware Platform

| Item | Value |
|------|-------|
| MCU | Raspberry Pi RP2350 |
| Core | Dual Cortex-M33 @ 150 MHz (ARMv8-M, Thumb-2) |
| FPU | FPv5-SP-D16 (hardware single-precision) |
| RAM | 520 KB SRAM |
| Flash | 2 MB |
| WiFi/BT | CYW43439 (LED driven via GPIO) |
| Board | Pico 2W |
| SDK | Raspberry Pi Pico SDK 2.1.1 |

### 2.1 Active Core Configuration (v0.7.0)

> ⚠️ **SMP is currently disabled.**  
> `configNUMBER_OF_CORES = 1` in `config/FreeRTOSConfig.h`.  
> All tasks execute on **Core 0**. Core 1 remains idle.  
> Dual-core SMP will be enabled in v1.0.0 once boot stability is confirmed.

---

## 3. Software Stack

```
┌─────────────────────────────────────────────────────────────────┐
│                     APPLICATION LAYER                           │
│  SensorRead  AttitudeCtrl  Telemetry  Command  HealthMon  LED  │
│                        (FreeRTOS Tasks)                         │
├─────────────────────────────────────────────────────────────────┤
│                       SERVICE LAYER                             │
│  FlightModeMgr  FaultManager  EPSMonitor  Logger  EKF  LQR     │
├─────────────────────────────────────────────────────────────────┤
│                  DATA LAYER ABSTRACTION (DLA)                   │
│         system_state — single shared state store                │
│   IMU · Magnetometer · Temperature · Attitude · Mode · Faults  │
├─────────────────────────────────────────────────────────────────┤
│                     DRIVER / HAL LAYER                          │
│  I2C HAL  Watchdog HAL  UART (CSP/KISS)  CYW43  ADC  GPIO     │
├─────────────────────────────────────────────────────────────────┤
│                      RTOS KERNEL                                │
│         FreeRTOS SMP (ARM_CM33_NTZ port, configCORES=1)         │
├─────────────────────────────────────────────────────────────────┤
│                      HARDWARE                                   │
│                RP2350 Cortex-M33 @ 150 MHz                      │
└─────────────────────────────────────────────────────────────────┘
```

---

## 4. FreeRTOS Task Map

All tasks created inside `vStartupTask` (runs at `configMAX_PRIORITIES - 1 = 4`)
after the scheduler starts, ensuring all kernel primitives are initialised.

| Task | Function | Priority | Stack (words) | Period | HWM (free words) | Used (words) |
|------|----------|----------|---------------|--------|------------------|--------------|
| Startup | `vStartupTask` | 4 (highest) | 2048 | one-shot → idles at P1 | — | — |
| SensorRead | `vSensorReadTask` | **4** | 2048 | 100 ms (10 Hz) | **1908** | 140 |
| AttitudeCtrl | `vAttitudeControlTask` | 3 | 2048 | 100 ms (10 Hz) | **1952** | 96 |
| Telemetry | `vTelemetryTask` | 2 | 2048 | 500 ms (2 Hz) | **1868** | 180 |
| Command | `vCommandTask` | 2 | 2048 | blocking (CSP recv) | **1894** | 154 |
| HealthMon | `vHealthMonitorTask` | 1 | 2048 | 5000 ms (0.2 Hz) | **1974** | 74 |
| LEDBlink | `vLedBlinkTask` | 1 | 2048 | 200/800 ms blink | **1947** | 101 |
| Heartbeat | `vHeartbeatTask` | 1 | 2048 | 2000 ms | **1930** | 118 |

> **HWM interpretation**: `uxTaskGetStackHighWaterMark()` returns the **minimum
> remaining free words** since task creation (high = good). Maximum usage
> across all tasks is Telemetry at 180 words (720 bytes = 8.8% of 2048-word
> stack). All tasks have ≥91% headroom. Stacks could be reduced to 512 words
> if memory pressure arises; current sizing is conservative by design.
> Measured on Pico 2W hardware @ v0.7.1, no IMU connected.

**Priority scale**: 0 = Idle, 1 = Low, 2 = Normal, 3 = High, 4 = Critical.  
SensorRead at P4 ensures the EKF always has fresh data before AttitudeCtrl (P3) runs;  
both P3 and P4 are above the FreeRTOS timer task (P3 = `configMAX_PRIORITIES - 2`).

> `vStartupTask` lowers itself to P1 after task creation and becomes the "ALIVE"
> watchdog print loop, avoiding the problematic `vTaskDelete` code path on
> single-core SMP.

---

## 5. Data Flow Architecture

### 5.1 Sensor → Estimation → Control → Actuation

```
[I2C Bus]
    │
    ├── MPU6050 ──────────────────────────→ [SensorReadTask]
    │   (accel + gyro)                              │
    └── HMC5883L ─────────────────────────→         │
        (magnetometer)                              │
                                                    │ DLA write:
                                                    │  imu_raw, mag_raw
                                                    ▼
                                            [EKF ekf_predict()]
                                            [EKF ekf_update()]
                                            [EKF ekf_update_mag()]
                                                    │
                                              x = [q0, q1, q2, q3,
                                                   bx,  by,   bz]
                                                    │ DLA write:
                                                    │  attitude, gyro_bias
                                                    ▼
                                        [AttitudeControlTask]
                                                    │
                                    ┌───────────────┴───────────────┐
                             FM=DETUMBLE                      FM=NOMINAL
                                    │                               │
                            [MomentumDump]                  ┌──────┴──────┐
                            (B-dot law)              FM=NOMINAL      FM=*
                                    │                       │             │
                                    │                    [LQR]         [PID]
                                    │                  u = -Kx        fallback
                                    └──────────┬────────────┘
                                               │ DLA write:
                                               │  actuator_cmd
                                               ▼
                                    [Magnetorquer + ReactionWheel]
                                         (actuator drivers)
```

### 5.2 Telemetry Flow

```
[DLA] system_state_snapshot()
        │
        ▼
[TelemetryTask] @ 2 Hz
        │  FM guard:
        │  FM_SAFE → HK-only (temp, mode, flags)
        │  FM_NOMINAL → full ADCS packet
        │
        ▼
[CSP packet] ──→ [libcsp router] ──→ [pico_usart/KISS] ──→ UART1 ──→ Ground
                 (loopback in
                  current build)
```

### 5.3 FDIR / SAFE MODE Authority Chain

> SAFE MODE authority is held exclusively by the **Flight Mode Manager (FMM)**.  
> The **only** valid path from any fault event to `FM_SAFE` is through the Fault Manager.  
> Direct EPS → FMM calls (`fmm_request_transition`) were removed in v0.7.1 (ARCH-02).

```
Canonical FDIR chain (single path — implemented v0.7.1):
────────────────────────────────────────────────────────

[EPSMonitor tick]             [HealthMon detects WDT reboot]
    │                                   │
    │ ENERGY_CRITICAL                   │
    │ or ENERGY_EMERGENCY               │
    │                                   │
    ▼                                   ▼
[fault_report(FAULT_EPS_VBATT_CRITICAL,  [fault_report(FAULT_WDT_KICK_MISSED,
             FAULT_LEVEL_CRITICAL)]                   FAULT_LEVEL_CRITICAL)]
    │                                   │
    └─────────────────┬─────────────────┘
                      │
                      ▼
      [fault_manager: level ≥ CRITICAL]
                      │
                      ▼
               [fmm_force_safe()]
                      │
                      ▼
      [FMM: data_layer_set_flight_mode(FM_SAFE)]
                      │
      ┌───────────────┼───────────────┐
      ▼               ▼               ▼
[TelemetryTask:  [AttitudeCtrl:  [HealthMon:
 HK-only mode]   early return,   watchdog still feeds]
                 no actuator
                 output]
```

**Design rationale for single-authority chain:**
- Unified fault audit log: every `FM_SAFE` transition is recorded with timestamp and count
- No concurrent/duplicate transitions from parallel EPS paths
- Single code point for future inhibit logic or priority override
- Formal FDIR traceability: trigger source → fault ID → FMM transition

> **AttitudeCtrl FM_SAFE behaviour**: the task uses an early `return` inside its
> loop body when `current_mode == FM_SAFE` — **not** `vTaskSuspend()`.  
> `vTaskSuspend` is avoided in FSW: it hides the task from runtime analysis and
> requires an external `vTaskResume` call for recovery. The no-IMU startup path
> was likewise converted from `vTaskSuspend` to a 5 s polling loop in v0.7.1.

### 5.4 Flight Mode Transition Table

```
       ┌──────────┬──────────┬──────────┬──────────┬──────────┐
       │ To →     │ FM_BOOT  │ FM_SAFE  │FM_NOMINAL│FM_DETUMBLE│
       ├──────────┼──────────┼──────────┼──────────┼──────────┤
        FM_BOOT   │    —     │    ✅    │    ✅    │    ❌    │
        FM_SAFE   │    ❌    │    —     │    ✅    │    ❌    │
        FM_NOMINAL│    ❌    │    ✅    │    —     │    ✅    │
        FM_DETUMBLE│   ❌    │    ✅    │    ✅    │    —     │
       └──────────┴──────────┴──────────┴──────────┴──────────┘

Rules:
  • FM_SAFE is reachable from ANY mode (fmm_force_safe bypasses table)
  • FAULT_LEVEL_CRITICAL blocks ALL transitions except to FM_SAFE
  • FM_BOOT is a transient mode (exited during vStartupTask)
```

---

## 6. EKF / Control Architecture

### 6.1 Extended Kalman Filter (7-state)

```
State vector:   x  = [q0, q1, q2, q3, bx, by, bz]ᵀ
Measurement:    z1 = accelerometer  → ekf_update()
                z2 = magnetometer   → ekf_update_mag()   (3D fusion)

Propagation:    xₖ₊₁ = f(xₖ, ωₖ)   via RK2 midpoint integrator
```

Gyro bias:      bₓ,ᵧ,z  estimated online; subtracted from ω before integration

Yaw update:     3D vector fusion; Jacobian dh/dq computed online
                Convergence verified: < 5° error in 10 s simulation

### 6.2 Control Dispatch (AttitudeControlTask)

```
FM_BOOT / FM_SAFE  →  return immediately (no actuator output)
FM_DETUMBLE        →  MomentumDump (B-dot law, DLA mag field)
FM_NOMINAL (EKF)   →  LQR    u = -K·x   (ωn=10 rad/s, ζ=1)
FM_NOMINAL (no EKF)→  PID    fallback
```

---

## 7. CSP Communication Stack

```
Ground Station
      │ UART1 @ 115200 baud
      │ KISS framing
      ▼
[pico_usart driver]  (src/drivers/pico_usart.c)
      │
      ▼
[libcsp v1.x]  running on FreeRTOS SMP
      │  Router task (tskIDLE_PRIORITY + 2)
      │  Address: node 1 (OBC)
      │
      ├── Port 20 → [CommandTask]   Echo + Reboot
      └── (all other) → loopback (production: extend here)

Telemetry path (OBC → Ground):
[TelemetryTask] → csp_buffer_get() → csp_sendto() → libcsp router
              → pico_usart → KISS → UART1
```

---

## 8. HAL Abstraction Pattern

All hardware-dependent functions use `__attribute__((weak))` symbols so unit
tests can link stub implementations without Pico SDK:

| HAL Symbol | Production impl | Test stub |
|------------|-----------------|-----------|
| `i2c_bus_read_reg()` | `src/drivers/i2c_interface.c` | test mock |
| `watchdog_hal_feed()` | `src/drivers/watchdog_hal_pico.c` | no-op |
| `watchdog_hal_triggered()` | `src/drivers/watchdog_hal_pico.c` | returns 0 |
| `watchdog_hal_enable()` | `src/drivers/watchdog_hal_pico.c` | no-op |
| `eps_hal_read_vbatt_mv()` | `src/services/eps/eps_hal_pico.c` | returns 3700 |

---

## 9. Memory Map (approximate, Release build)

```
Flash (2 MB total):
  ├── .text (code + rodata) : ~330 KB   (cubesat_obc_pico v0.7.0)
  └── free                  : ~1.7 MB

SRAM (520 KB total):
  ├── .bss (static data)    : ~154 KB
  ├── FreeRTOS heap         : ~240 KB   (configTOTAL_HEAP_SIZE)
  │     ├── free at runtime  : ~60 KB    (xPortGetFreeHeapSize, measured HW)
  │     └── min-ever free    : ~58 KB    (xPortGetMinimumEverFreeHeapSize, HW)
  ├── Task stacks           :  ~64 KB   (8 tasks × 2048 words × 4 bytes)
  └── SDK/system            : remainder
```

---

## 10. Boot Sequence

```
reset_handler
    │
    ├── prvClearUnalignTrap()   [PICO_RUNTIME_INIT_FUNC_PER_CORE @ "00052"]
    │   Clears CCR.UNALIGN_TRP on each core (RP2350 boot workaround)
    │
    ▼
main()
    ├── stdio_init_all()
    ├── setvbuf(stdout, NULL, _IONBF, 0)   [fully unbuffered]
    ├── wait up to 3 s for USB CDC enumerate
    ├── check watchdog scratch[0] for crash ID (0xDEAD0001)
    ├── xTaskCreate(vStartupTask, P=4)
    └── vTaskStartScheduler()

        vStartupTask() [P=4, runs before any other task]
            ├── cyw43_arch_init()           [LED driver]
            ├── system_state_init()         [DLA]
            ├── comm_init()                 [CSP / libcsp]
            ├── fault_manager_init()        [FDIR]
            ├── eps_monitor_init()          [Energy guard]
            ├── i2c_bus_init()              [I2C @ 400 kHz]
            ├── mpu6050_init()              [IMU, graceful fail]
            ├── temperature_init()          [Temp sensor]
            ├── xTaskCreate × 7            [all application tasks]
            └── vTaskPrioritySet(self, P=1) [become ALIVE loop]
```

---

## 11. Open Items for v1.0.0

| ID | Issue | Priority |
|----|-------|----------|
| ARCH-01 | Enable SMP (`configNUMBER_OF_CORES = 2`), assign core affinity | High |
| ARCH-02 | ~~Remove EPS → FMM direct path~~ — ✅ **Implemented (v0.7.1)**: `eps_monitor.c` reports `FAULT_LEVEL_CRITICAL` for both `ENERGY_CRITICAL` and `ENERGY_EMERGENCY`; `fmm_request_transition()` removed | ~~High~~ → ✅ |
| ARCH-03 | ~~Instrument all task HWMs~~ — ✅ **Implemented (v0.7.1)**: all 7 task HWMs printed every 5 s in ALIVE loop; `xPortGetMinimumEverFreeHeapSize()` added to Heartbeat and ALIVE | ~~Medium~~ → ✅ |
| ARCH-04 | Flash-backed logger backend (persistent across reboot) | Medium |
| ARCH-05 | Validate watchdog timeout window on real hardware (currently uses SDK default) | High |
| ARCH-06 | Connect real MPU6050 + HMC5883L; validate EKF convergence on hardware | High |
| ARCH-07 | Ground-station command validation (checksum, replay protection) | Medium |
