# System Requirements Specification (SyRS) v1.1
## CubeSat OBC Flight Software — Phase 7 Implementation Update

**Document ID**: SyRS-OBC-001  
**Version**: 1.1  
**Date**: 2026-03-20  
**Status**: ✅ Active — Phase 7 implementation baseline  
**Predecessor**: SyRS v1.0 (2026-03-04, superseded)

---

## 1. Purpose

This document specifies the system-level requirements for the CubeSat OBC flight
software. It supersedes the earlier conceptual SyRS and aligns requirements with
the **actual implemented architecture** post v0.7.0, ensuring traceability from
requirements to code and tests.

Requirements marked `[IMPL]` are implemented and verified.  
Requirements marked `[PARTIAL]` are partially implemented (stub or in-progress).  
Requirements marked `[PLANNED]` are not yet implemented (target: v1.0.0).

**Phase 7 Update (2026-03-20)**: Added implementation status for:
- Telemetry (STUB COMPLETE): libcsp v2.2 + UART1 KISS framing done, HW pending
- Magnetometer (PARTIAL): HMC5883L stub driver + EKF integration done, I2C pending
- GPS (PARTIAL): NEO-7M driver with NMEA parser, GPRMC parsing done
- Reaction Wheels (STUB): PWM HAL stub only, HW integration pending Phase 8
- Magnetorquers (STUB): PWM HAL stub only, HW integration pending Phase 8
- Flash Storage (STUB): backend writes to /tmp/obc_log.bin, full RP2350 pending
- Payload (PARTIAL): Power rail stub, camera deferred, SD SPI stub

---

## 1.1 Mission Objective to SyRS Requirement Cross-Reference

*Added per SRR-OBC-001 ACT-08 — provides top-down traceability from MRD-OBC-001
mission objectives to system-level requirements in this document.*

| MO   | Mission Objective (MRD-OBC-001 §2.3)                              | Implementing SyRS Requirements            |
|------|-------------------------------------------------------------------|-------------------------------------------|
| MO-1 | 3-axis attitude determination via EKF                             | SYS-F-101, SYS-F-102, SYS-F-103, SYS-F-104, SYS-F-105, SYS-F-106 |
| MO-2 | 3-axis attitude control — B-dot detumbling (MTQ) + LQR pointing (RW) | SYS-F-111, SYS-F-112, SYS-F-113, SYS-F-114, SYS-F-115, **SYS-F-120, SYS-F-121, SYS-F-122, SYS-F-123** |
| MO-3 | Autonomous FDIR: FM_SAFE transition within 10 s of CRITICAL fault | SYS-F-201, SYS-F-204, SYS-F-205, SYS-F-211, SYS-F-212, SYS-F-213 |
| MO-4 | Bidirectional TT&C over 433 MHz LoRa (E22-400M30S)               | SYS-F-401, SYS-F-402, SYS-F-403, SYS-F-404, SYS-F-405, SYS-F-406, SYS-F-407, **SYS-F-450** |
| MO-5 | Energy-aware subsystem management (EPS Schmitt-trigger, rail shedding) | SYS-F-201, SYS-F-202, SYS-F-203, SYS-F-204, SYS-F-205 |
| MO-6 | Persistent event logging across power cycles (Class-A events survive reset) | SYS-F-301, SYS-F-302, SYS-F-303, SYS-F-304 |
| MO-7 | ECSS-Q-ST-80C compliance (MISRA C, traceability, ≥90% test coverage) | SYS-NF-001, SYS-NF-002, SYS-NF-003 |

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

### SYS-F-107 — Magnetometer Hardware

*Added per Phase 7. Hardware: HMC5883L (GY-271) on I2C0 at 0x1E, 75 Hz. CDR/FM may substitute LIS3MDL at 0x1C.*

#### SYS-F-107 — Magnetometer Data Acquisition  
`[PARTIAL]` The system shall read 3-axis magnetic field vector via I2C at ≥10 Hz and publish to EKF and momentum-dump service.  
*HMC5883L stub driver implemented. EKF integration complete (`ekf_update_mag()`). Real I2C implementation pending PR-18. I2C0 GPIO4/5 configured.*

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

### SYS-F-116 — Magnetorquer Control

*Hardware: 3-axis magnetorquer coils for momentum dump (de-saturation). **Status: STUB — PWM HAL stub only, HW integration pending Phase 8.***

#### SYS-F-116a — Magnetorquer Torque Command  
`[STUB]` The system shall apply magnetic dipole commands to magnetorquer coils for momentum dump.  
*PWM HAL stub implemented. Actual coil driver and I2C PWM generator pending Phase 8.*

#### SYS-F-116b — B-dot Control  
`[IMPL]` The system shall execute B-dot detumbling control during `FM_DETUMBLE`.  
*Algorithm implemented in `momentum_dump.c`. Uses magnetometer data to compute dipole moment proportional to dB/dt.*

### SYS-F-120 — Reaction Wheel Performance

*Added per SRR-OBC-001 ACT-13. Values derived from `config.h` constants and closed-loop simulation model (`closed_loop_sim.h`). Hardware: custom 1U RW assembly. **Status: STUB — PWM HAL stub only, HW integration pending Phase 8.***

#### SYS-F-121 — RW Maximum Angular Speed  
`[IMPL]` Each reaction wheel axis shall support a maximum angular velocity of **4000 RPM** (≈ 419 rad/s).  
*Source: `RW_MAX_OMEGA_RPM = 4000.0f` in `config.h`. Fault `FAULT_ACT_RW_SPEED_LIMIT` is raised on exceedance.*

#### SYS-F-122 — RW Maximum Generated Torque  
`[IMPL]` Each reaction wheel axis shall generate a maximum continuous torque of **1 mN·m** per axis.  
*Source: `CLS_TAU_SAT = 1.0e-3 N·m` in `closed_loop_sim.h`. Sized for 1U CubeSat inertia `I = diag(0.01, 0.01, 0.005) kg·m²`.*

#### SYS-F-123 — RW Maximum Momentum Storage  
`[PLANNED]` Each reaction wheel axis shall store a maximum angular momentum of **0.42 N·m·s** before saturation ( = `RW_INERTIA × RW_MAX_OMEGA_RPM_RAD_S` = 0.001 × 418.9).  

#### SYS-F-124 — Momentum Dump Activation Threshold  
`[IMPL]` The system shall initiate a B×L momentum dump when the total reaction-wheel angular momentum vector magnitude exceeds **0.5 mN·m·s**.  
*Source: `CLS_MOM_THRESH = 5.0e-4 N·m·s` in `closed_loop_sim.h`; runtime threshold passed to `momentum_dump_needed()` via `attitude_control_task.c`.*

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
`[IMPL]` The logger shall persist Class-A events to flash memory to survive power cycling.  
*Implemented in `src/core/flash_backend.c` (ACT-16, 2026-03-10): round-robin 4-sector log region at top of 2 MB flash (`0x1FC000–0x1FFFFF`), 16-byte header (magic `OBCLOGV1` + CRC-32 + length), interrupt-disabled erase+program via Pico SDK `hardware_flash`. Recovery path: `flash_backend_recover()`. Host build retains stub in `flash_backend_stub.c`. **Status: STUB — writes to /tmp/obc_log.bin on host, full RP2350 flash implementation pending.***

---

## 7. Communication Requirements

### SYS-F-400 — CSP Communication

#### SYS-F-401 — Protocol Stack  
`[IMPL]` The OBC shall run the `libcsp` v2.2 stack (CSP protocol version 2) on FreeRTOS (single-core configuration; SMP planned per SYS-P-003).  
*Version confirmed: `third_party/libcsp/CMakeLists.txt` — `project(CSP VERSION 2.2)`, default protocol version `csp_conf.version = 2` in `src/csp_init.c`. Resolved per SRR-OBC-001 ACT-02. **Status: STUB COMPLETE — libcsp + KISS framing done, HW transmission pending (E22-400M30S).***

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

#### SYS-F-450 — RF Link Margin  
`[PLANNED]` The RF link subsystem (E22-400M30S, 433 MHz band) shall achieve a minimum **6 dB** link margin in both uplink and downlink paths under the worst-case orbital geometry: altitude 600 km, elevation angle 5°, with OBC transmit power ≤ 30 dBm (+30 dBm E22 maximum) and ground station receive antenna gain ≥ 3 dBi (quarter-wave whip equivalent).  
*Verification method: analysis via LINK-BDG-001 link budget (planned CDR deliverable). COMMS-DES-001 §6 provides physical layer parameters. Added per SRR-OBC-001 ACT-12.*

---

## 7.1 GPS Requirements

*Added per Phase 7 payload baseline (AIR-OBC-001 ACT-06 Option A). Hardware: NEO-7M GPS module (GY-NEO6Mv2).*

### SYS-F-460 — GPS Module

#### SYS-F-461 — NMEA Sentence Parsing  
`[PARTIAL]` The OBC shall parse NMEA 0183 sentences (`$GPGGA`, `$GPRMC`) from the NEO-7M GPS module via UART0 at ≥ 1 Hz.  
*NEO-7M driver with NMEA parser implemented. GPRMC parsing and position/velocity extraction done. UART0 GPIO0/1 configured at 9600 baud.*

#### SYS-F-462 — Position/Velocity Output  
`[PARTIAL]` The system shall publish parsed position (latitude, longitude, altitude) and velocity to the Data Layer.  
*Data layer integration done for $GPGGA (position) and $GPRMC (velocity).*

#### SYS-F-463 — UTC Time Synchronization  
`[IMPLEMENTED]` The system shall synchronize the internal software clock to GPS UTC time (from `$GPRMC`) within ± 500 ms on each valid fix acquisition.  
*`nmea_parse_gprmc_and_sync_rtc()` calls `ds3231_set_time()` on valid $GPRMC after `gps_rtc_delta_check()` (±3 s guard). 7/7 delta guard tests passing (host).*

---

## 7.5 Payload Power Management

*Added per SRR-OBC-001 ACT-04. Payload concept: a low-power science/demonstration module (e.g., camera module or beacon transmitter) powered by a dedicated EPS-controlled GPIO rail. Hardware definition deferred to CDR. **Status: PARTIAL — power rail control stub implemented, GPIO21 HAL done, FMM integration pending Phase 8.***

#### SYS-F-500 — Payload Rail Enable/Disable  
`[PARTIAL]` The OBC shall enable and disable the payload power rail via a dedicated GPIO output pin under FMM control.  
*Payload rail shall be OFF in FM_BOOT, FM_SAFE, and FM_DETUMBLE. Rail may be enabled only in FM_NOMINAL and FM_DIAGNOSTIC. GPIO21 HAL stub implemented.*

#### SYS-F-501 — Payload Rail FMM Interlock  
`[PARTIAL]` The FMM shall inhibit payload rail activation unless energy state is `ENERGY_NOMINAL`.  
*Rationale: Prevents payload from drawing power when the battery is below safe operating voltage. FMM integration pending.*

#### SYS-F-502 — Payload Rail Fault Detection  
`[PLANNED]` The EPS Monitor shall detect payload rail overcurrent (current draw exceeding configured threshold) and report `FAULT_PAYLOAD_OVERCURRENT` at `FAULT_LEVEL_WARNING`.  
*On second consecutive overcurrent event: escalate to `FAULT_LEVEL_CRITICAL` and command rail off.*

#### SYS-F-503 — Payload Rail Telemetry  
`[PLANNED]` Telemetry packets shall include a 1-bit payload rail status flag (enabled/disabled) in the housekeeping frame.

#### SYS-F-504 — Payload Rail Command  
`[PLANNED]` The OBC shall accept an uplink command `CMD_PAYLOAD_ENABLE` / `CMD_PAYLOAD_DISABLE` on CSP Port 20 to activate/deactivate the payload rail, subject to FMM and energy state interlocks (SYS-F-501).

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

| Requirement | Implementation | Test | Coverage |
|-------------|---------------|------|----------|
| SYS-F-101..106 | `src/dynamics/ekf.c` | `ekf_test`, `ekf_mag_test` | ~94% |
| SYS-F-107 | `src/sensors/hmc5883l.c` (stub) | pending | — |
| SYS-F-111..113 | `src/control/lqr_schedule.c`, `pid_controller.c` | `lqr_test`, `lqr_schedule_test` | ~94% |
| SYS-F-114, SYS-F-116b | `src/actuators/momentum_dump.c` | `momentum_dump_test` | ~90% |
| SYS-F-116a | `src/actuators/motor_pwm_hal.c` (stub) | pending | — |
| SYS-F-120..124 | `src/actuators/rw_pwm_hal.c` (stub) | pending | — |
| SYS-F-201..204 | `src/services/eps/eps_monitor.c` | `eps_monitor_test` | ~90% |
| SYS-F-211..213 | `src/tasks/health_monitor_task.c`, `watchdog_hal*.c` | `watchdog_test`, `health_monitor_task_test` | ~88% |
| SYS-F-301..304 | `src/services/logger/logger.c`, `flash_backend.c` | `logger_test`, `flash_backend_test` | ~90% |
| SYS-F-401..407 | `src/core/comm_init.c`, `src/tasks/telemetry_task.c`, `command_task.c` | `comm_init_test`, `telemetry_test`, `command_test` | ~92% |
| SYS-F-461..463 | `src/sensors/neo7m.c` | pending | — |
| SYS-NF-001..003 | `include/host/`, `scripts/static_analysis.sh` | CI pipeline | 91.9% lines, 82.5% branches |

---

## 11. Phase 7 Implementation Status Summary

| Component | Status | Details |
|-----------|--------|---------|
| **Telemetry (FR-7)** | STUB COMPLETE | libcsp v2.2, UART1 KISS framing, 2 Hz TX; HW pending |
| **Magnetometer (FR-11)** | PARTIAL | HMC5883L stub, EKF integration done; I2C pending PR-18 |
| **GPS (FR-12/18/19)** | PARTIAL | NEO-7M driver, NMEA parser, GPRMC parsing, position/velocity done |
| **Reaction Wheels (FR-5)** | STUB | PWM HAL stub; HW integration Phase 8 |
| **Magnetorquers (FR-6)** | STUB | PWM HAL stub; HW integration Phase 8 |
| **Flash Storage** | STUB | Writes to /tmp/obc_log.bin (host); RP2350 flash pending |
| **Payload Power Rail** | PARTIAL | GPIO21 HAL stub; FMM integration Phase 8 |
| **Camera (CAM-001)** | DEFERRED | IMX219 driver deferred to Phase 8 |
| **SD Storage** | PARTIAL | SD SPI stub; backend integration pending |
| **FM_PAYLOAD Mode** | PARTIAL | Mode stub; FMM update pending |
