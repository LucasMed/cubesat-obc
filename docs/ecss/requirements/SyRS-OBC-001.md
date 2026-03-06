# System Requirements Specification (SyRS) v1.0
## CubeSat OBC Flight Software — Post v0.7.0 Alignment

**Document ID**: SyRS-OBC-001  
**Version**: 1.0  
**Date**: 2026-03-04  
**Status**: ✅ Reflects implemented + verified capabilities as of v0.7.0  
**Predecessor**: SyRS v0.x (conceptual-level, now superseded)

---

## 1. Purpose

This document specifies the system-level requirements for the CubeSat OBC flight
software. It supersedes the earlier conceptual SyRS and aligns requirements with
the **actual implemented architecture** post v0.7.0, ensuring traceability from
requirements to code and tests.

Requirements marked `[IMPL]` are implemented and verified.  
Requirements marked `[PLANNED]` are not yet implemented (target: v1.0.0).

---

## 2. System Overview

The OBC software executes on a Raspberry Pi Pico 2W (RP2350, Cortex-M33) under
FreeRTOS. It provides attitude determination and control, fault detection and
isolation, telemetry downlink, telecommand uplink, and energy management for a
1U CubeSat.

---

## 3. Platform Requirements

### SYS-P-001 — Processor  
`[IMPL]` The OBC shall execute on a Cortex-M33 processor (RP2350) running at
minimum 100 MHz.

### SYS-P-002 — RTOS  
`[IMPL]` The OBC shall use FreeRTOS with the `ARM_CM33_NTZ` port.  
*Rationale: Correct EXC_RETURN handling on ARMv8-M; avoids PendSV stack corruption
observed with the ARM_CM0 port.*

### SYS-P-003 — SMP Configuration  
`[PLANNED]` The OBC shall support FreeRTOS SMP with `configNUMBER_OF_CORES = 2`.  
*Current state: disabled (`= 1`) pending boot stability validation on HW.*

### SYS-P-004 — Memory  
`[IMPL]` The OBC shall maintain a minimum of 32 KB free heap during nominal
operations.  
*Measured: 60,416 bytes free on hardware.*

---

## 4. Control & Estimation Requirements

### SYS-F-100 — Attitude Estimation

#### SYS-F-101 — EKF State Vector  
`[IMPL]` The system shall implement an Extended Kalman Filter with 6-state vector:  
`x = [roll, pitch, yaw, bias_x, bias_y, bias_z]`  
where `bias_x/y/z` are gyroscope bias estimates in rad/s.

#### SYS-F-102 — Dynamics Integration  
`[IMPL]` The EKF shall propagate attitude dynamics using a second-order
Runge-Kutta (RK2) midpoint integrator.  
*Rationale: Reduces attitude integration error compared to Euler at 10 Hz.*

#### SYS-F-103 — Gyroscope Bias Estimation  
`[IMPL]` The EKF shall estimate and subtract gyroscope bias from angular rate
measurements before integration.

#### SYS-F-104 — Accelerometer Update  
`[IMPL]` The EKF shall incorporate accelerometer measurements via `ekf_update_accel()`
for roll and pitch observability.

#### SYS-F-105 — Magnetometer Yaw Update  
`[IMPL]` The EKF shall incorporate tilt-compensated magnetometer measurements via
`ekf_update_mag()` with observation matrix `H = [0, 0, 1, 0, 0, 0]` to make yaw
observable.  
*Verified: convergence < 5° error in 10 s simulation.*

#### SYS-F-106 — Estimation Rate  
`[IMPL]` The attitude estimation pipeline shall execute at minimum 10 Hz.

---

### SYS-F-110 — Attitude Control

#### SYS-F-111 — LQR Controller  
`[IMPL]` The system shall support a full-state Linear Quadratic Regulator (LQR)
controller: `u = -K·x`, with default gains tuned for `ωn = 10 rad/s, ζ = 1`.

#### SYS-F-112 — PID Fallback  
`[IMPL]` The system shall support a PID controller as fallback when EKF
estimation is unavailable.

#### SYS-F-113 — Dynamic LQR/PID Dispatch  
`[IMPL]` The attitude control task shall dispatch to LQR or PID based on sensor
availability and flight mode, without restart.

#### SYS-F-114 — Momentum Dump  
`[IMPL]` The system shall execute a B-dot momentum dump algorithm when flight
mode is `FM_DETUMBLE`.  
The duty cycle shall be proportional to the time-derivative of the magnetic field
vector (`dB/dt`).

#### SYS-F-115 — Safe-Mode Actuator Inhibit  
`[IMPL]` In `FM_SAFE` and `FM_BOOT`, the attitude control task shall produce no
actuator output.

---

## 5. FDIR & Energy Management Requirements

### SYS-F-200 — Energy Guard

#### SYS-F-201 — EPS Monitoring  
`[IMPL]` The EPS Monitor shall continuously measure battery voltage and classify
energy state as: `ENERGY_NOMINAL`, `ENERGY_LOW`, `ENERGY_CRITICAL`.

#### SYS-F-202 — Schmitt-Trigger Hysteresis  
`[IMPL]` The EPS Monitor shall apply Schmitt-trigger hysteresis to energy state
transitions to prevent flapping on boundary voltages.

#### SYS-F-203 — Telemetry Energy Flag  
`[IMPL]` Telemetry packets shall encode the current energy state in flags
`bits[3:2]`.

#### SYS-F-204 — SAFE MODE on Critical Energy  
`[IMPL]` The system shall transition to `FM_SAFE` when `ENERGY_CRITICAL` is
detected permanently.  
*Implementation: EPS Monitor → `fault_report(FAULT_LEVEL_CRITICAL)` → Fault Manager
→ `fmm_force_safe()`.*

#### SYS-F-205 — FDIR Authority Chain (single path)  
`[IMPL]` SAFE MODE transitions triggered by EPS shall flow exclusively through
the Fault Manager (EPS → Fault Manager → FMM). Direct calls from EPS to FMM
(`fmm_request_transition`) are removed.  
*Both `ENERGY_CRITICAL` and `ENERGY_EMERGENCY` now report `FAULT_LEVEL_CRITICAL`
→ `fault_manager` → `fmm_force_safe()`. No direct FMM calls from EPS Monitor.*

---

### SYS-F-210 — Watchdog

#### SYS-F-211 — Hardware Watchdog Feed  
`[IMPL]` The Health Monitor task shall call `watchdog_hal_feed()` on every
execution cycle (period: 5 s).

#### SYS-F-212 — Watchdog Reset on Hang  
`[IMPL]` The hardware watchdog shall force a system reset if the Health Monitor
fails to feed within the configured timeout window.

#### SYS-F-213 — Post-Reset Fault Report  
`[IMPL]` On boot, if a watchdog-triggered reset is detected (`watchdog_hw->scratch[0]
== 0xDEAD0001`), the system shall raise `FAULT_WDT_KICK_MISSED` at
`FAULT_LEVEL_CRITICAL`, transitioning to `FM_SAFE`.

#### SYS-F-214 — Watchdog Timeout Validation  
`[PLANNED]` The watchdog timeout window shall be validated on hardware to confirm
> 5 s (Health Monitor period) and < 30 s (reboot budget).

---

## 6. Logging Requirements

### SYS-F-300 — Persistent Ring Logger

#### SYS-F-301 — Ring Buffer Capacity  
`[IMPL]` The logger shall maintain a circular ring buffer of minimum 320 entries.

#### SYS-F-302 — Class-A Protection  
`[IMPL]` Events classified as `FAULT_LEVEL_CRITICAL` (Class-A) shall be protected
from eviction by lower-priority entries in the ring buffer.

#### SYS-F-303 — Logger API  
`[IMPL]` The logger shall provide `logger_append()` and `logger_read()` APIs
accessible from all service layers.

#### SYS-F-304 — Flash Backend  
`[PLANNED]` The logger shall persist Class-A events to flash memory to survive
power cycling.

---

## 7. Communication Requirements

### SYS-F-400 — CSP Communication

#### SYS-F-401 — Protocol Stack  
`[IMPL]` The OBC shall run the `libcsp` v1.x stack on FreeRTOS SMP.

#### SYS-F-402 — Transport  
`[IMPL]` CSP shall be transported over UART1 using KISS framing (`pico_usart`
driver).

#### SYS-F-403 — Telemetry Rate  
`[IMPL]` The OBC shall transmit telemetry packets at minimum 1 Hz in
`FM_NOMINAL`.  
*Implemented at 2 Hz.*

#### SYS-F-404 — Telemetry Content (Nominal)  
`[IMPL]` In `FM_NOMINAL`, telemetry shall include: flight mode, attitude
`[roll, pitch, yaw]`, energy flags, and sensor availability flags.

#### SYS-F-405 — Telemetry Content (Safe)  
`[IMPL]` In `FM_SAFE`, telemetry shall be reduced to housekeeping only:
temperature, mode, flags (attitude and rates zeroed).

#### SYS-F-406 — Command Reception  
`[IMPL]` The command task shall listen on CSP Port 20.

#### SYS-F-407 — Command Types  
`[IMPL]` The OBC shall handle at minimum: `CMD_ECHO` and `CMD_REBOOT` commands.

---

## 8. System-Level Non-Functional Requirements

### SYS-NF-001 — Host Testability  
`[IMPL]` All business logic shall be testable on a Linux host without Pico SDK
(`PICO_ENABLED=OFF`) via `__attribute__((weak))` HAL stubs.

### SYS-NF-002 — Test Coverage  
`[IMPL]` The CI pipeline shall maintain ≥ 85% line coverage across
`src/control/`, `src/core/`, and `src/services/`.  
*Measured: 91.9% lines, 82.5% branches.*

### SYS-NF-003 — Static Analysis  
`[IMPL]` All source files shall pass `clang-format`, `clang-tidy`, and `cppcheck`
with 0 violations in the CI pipeline.

### SYS-NF-004 — Task Timing (10 Hz Loop)  
`[IMPL]` The sensor read task shall execute at 10 Hz with absolute jitter
≤ 500 µs.  
*Measured on hardware: min=99,954 µs, max=100,037 µs (±43 µs jitter — well within spec).*

### SYS-NF-005 — Boot Time  
`[IMPL]` The OBC shall complete subsystem initialization and begin nominal task
execution within 10 s of power-on (excluding USB CDC enumeration wait).

### SYS-NF-006 — Stack Safety  
`[PLANNED]` All task stack high-water marks shall be measured on hardware and each
task shall have ≥ 20% stack headroom.  
*Current: only Heartbeat task instrumented (HWM=1930/2048 = 94% free).*

---

## 9. FDIR Architecture Summary

The following table captures the complete FDIR decision matrix as implemented:

| Trigger | Detected by | Fault reported | Level | FMM transition |
|---------|-------------|----------------|-------|----------------|
| Battery critical | `eps_monitor_tick()` | `FAULT_EPS_VBATT_CRITICAL` | CRITICAL | `FM_SAFE` |
| Battery low | `eps_monitor_tick()` | `FAULT_EPS_VBATT_LOW` | WARNING | none |
| EPS read error | `eps_monitor_tick()` | `FAULT_EPS_READ_ERROR` | ERROR | none |
| Watchdog reboot | `vHealthMonitorTask_Step()` | `FAULT_WDT_KICK_MISSED` | CRITICAL | `FM_SAFE` |
| Task stack overflow | `vApplicationStackOverflowHook` | writes task name to `scratch[1..3]`, resets | — | cold reboot |

---

## 10. Requirements Traceability Summary

| Requirement | Implementation | Test |
|-------------|---------------|------|
| SYS-F-101..106 | `src/dynamics/ekf.c` | `ekf_test`, `ekf_mag_test` |
| SYS-F-111..113 | `src/control/lqr_schedule.c`, `pid_controller.c` | `lqr_test`, `lqr_schedule_test` |
| SYS-F-114 | `src/actuators/momentum_dump.c` | `momentum_dump_test` |
| SYS-F-201..204 | `src/services/eps/eps_monitor.c` | `eps_monitor_test` |
| SYS-F-211..213 | `src/tasks/health_monitor_task.c`, `watchdog_hal*.c` | `watchdog_test`, `health_monitor_task_test` |
| SYS-F-301..303 | `src/services/logger/logger.c` | `logger_test` |
| SYS-F-401..407 | `src/core/comm_init.c`, `src/tasks/telemetry_task.c`, `command_task.c` | `comm_init_test`, `telemetry_test`, `command_test` |
| SYS-NF-001..003 | `include/host/`, `scripts/static_analysis.sh` | CI pipeline |
| SYS-NF-004 | `src/tasks/sensor_read_task.c` | hardware measurement |
