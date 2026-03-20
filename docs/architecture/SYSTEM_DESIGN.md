# System Design Document

**Document ID**: DES-001  
**Version**: 2.1  
**Last Updated**: 2026-03-20  
**Status**: Active

> **Doc-Update 2026-03-20**: Corrected discrepancies found during code review:
> - FreeRTOS: Single-core mode (`configNUMBER_OF_CORES=1`)
> - AttitudeControl: **10 Hz** (not 20 Hz)
> - EKF: **7-state quaternion** `[q0,q1,q2,q3,bx,by,bz]` (not 6-state Euler)
> - LQR: **Gain scheduling** for FM_NOMINAL/FM_DETUMBLE modes
> - Tasks: **8 tasks** (not 4)
> - GPS: UART1 GPIO4/5 (not UART0 GPIO0/1)

---

## 1. System Overview

The CubeSat OBC implements an Attitude Determination and Control System (ADCS) for a 1U CubeSat platform using a Raspberry Pi Pico 2W (RP2350) as the flight computer.

### 1.1 Design Goals
- Real-time attitude determination and control with quaternion representation
- Full yaw observability via tilt-compensated magnetometer EKF update
- Modular architecture following ECSS-Q-ST-80C standards
- FreeRTOS-based multitasking with deterministic scheduling (single-core SMP config)
- Hardware abstraction for sensor and actuator portability (`__attribute__((weak))` HAL stubs)
- Host-testable on Linux without hardware (`PICO_ENABLED=OFF`)

### 1.2 System Block Diagram

```
┌──────────────────────────────────────────────────────────────────────┐
│                        CubeSat OBC (RP2350)                          │
├──────────────────────────────────────────────────────────────────────┤
│                                                                      │
│  ┌───────────────────┐    ┌──────────────────────┐   ┌───────────┐  │
│  │      SENSORS      │    │       CONTROL        │   │ ACTUATORS │  │
│  ├───────────────────┤    ├──────────────────────┤   ├───────────┤  │
│  │ • MPU6050  (I2C)  │───▶│ • 7-state EKF (q)    │──▶│ • RW      │  │
│  │ • HMC5883L (I2C)  │───▶│   (accel + mag yaw)  │   │ • Mag-torq│  │
│  │ • TMP102   (I2C)  │    │ • LQR u=-Kx (sched.) │   └───────────┘  │
│  │ • Vbatt    (ADC)  │    │ • PID fallback        │                  │
│  │ • RM3100   (SPI)  │    │ • RK2 dynamics        │                  │
│  └───────────────────┘    │ • Momentum dump (B×L) │                  │
│                           └──────────┬───────────┘                  │
│                                      │                              │
│  ┌──────────────────────────────────▼────────────────────────────┐  │
│  │              DATA LAYER ABSTRACTION (DLA)                     │  │
│  │  SystemStateStore  ·  mutex-protected  ·  getter/setter API   │  │
│  └────────────────────────────────────────────────────────────────┘  │
│           │                   │                   │                  │
│  ┌────────▼────────┐  ┌───────▼────────┐  ┌──────▼──────────────┐  │
│  │  TELEMETRY      │  │ HEALTH MONITOR │  │  FAULT / EPS / FMM  │  │
│  │  (CSP/WiFi/UART)│  │ · Watchdog kick│  │  6 Flight Modes     │  │
│  └─────────────────┘  └────────────────┘  └─────────────────────┘  │
└──────────────────────────────────────────────────────────────────────┘
```

---

## 2. Software Architecture

### 2.1 Module Decomposition

| Module | Path | Responsibility |
|--------|------|---------------|
| Core | `src/core/` | Data Layer Abstraction, FMM (6 modes), Fault Manager (32-slot), EPS Monitor, Logger |
| Drivers | `src/drivers/` | HAL for I2C, UART, ADC, SPI; MPU6050, TMP102, HMC5883L, NEO-7M GPS, RM3100, Camera |
| Control | `src/control/` | 7-state EKF (quaternion), LQR controller with gain scheduling, PID, RK2 dynamics |
| Actuators | `src/actuators/` | Reaction wheel & magnetorquer models |
| Dynamics | `src/dynamics/` | RK2 attitude dynamics integrator |
| Services | `src/services/` | Watchdog HAL, momentum dump (B×L) algorithm, EPS monitor |
| Tasks | `src/tasks/` | FreeRTOS task implementations (8 tasks: Sensor, Ctrl, Telemetry, Command, Health, GPS, Payload, LED/Heartbeat) |

### 2.2 Task Architecture

| Task | Rate | Priority (IDLE+n) | Stack | Notes |
|------|------|-------------------|-------|-------|
| SensorRead | 10 Hz | IDLE+4 (4) | 2048 words | EKF predict/update loop |
| AttitudeControl | 10 Hz | IDLE+3 (3) | 2048 words | LQR/PID + RK2 dynamics |
| Telemetry | 1 Hz | IDLE+2 (2) | 2048 words | CSP telemetry TX |
| Command | Event-driven | IDLE+2 (2) | 2048 words | CSP port 20 uplink |
| HealthMonitor | 0.2 Hz | IDLE+1 (1) | 2048 words | Watchdog + FDIR |
| GpsTask | 1 Hz | IDLE+2 (2) | 2048 words | NMEA → DLA |
| PayloadTask | 10 Hz | 2 | 1024 words | RM3100, camera, SD |
| LED/Heartbeat | 5/0.5 Hz | IDLE+1-2 | 2048 | Pico-only diagnostics |

> **Discrepancy from original doc**: AttitudeControl runs at **10 Hz** (not 20 Hz).

### 2.3 Data Flow

```
MPU6050 (10 Hz)
        │
        ▼
  ekf_predict()          ← gyro rates (rad/s)
  ekf_update()           ← accel (roll, pitch)
        │
  HMC5883L (if avail.) ──▶ ekf_update_mag()  ← tilt-compensated yaw
        │
        ▼
  Data Layer (DLA)       ← EKF quaternion q[4] + bias[3] + P[7][7]
        │
  ┌─────┴──────────────────────────────┐
  │                                    │
  ▼                                    ▼
LQR u=-Kx (FM_NOMINAL + EKF valid)   Telemetry (1 Hz) → CSP → Ground
PID fallback (FM_DIAGNOSTIC/!EKF)     Health Monitor (0.2 Hz) + Watchdog
  │
  ▼
Momentum dump?  (FM_DETUMBLE → B×L law)
  │
  ▼
Actuators: RW torque / Magnetorquer dipole
```

---

## 3. Hardware Design

### 3.1 Target Platform
- **MCU**: RP2350 (Pico 2W) — ARM Cortex-M33, dual-core, 520 KB SRAM
- **WiFi**: CYW43439 (onboard Pico 2W)
- **Clock**: 150 MHz (default)

### 3.2 Pin Assignment
See `config/pico_pins.h` for complete mapping.

| Function | GPIO | Protocol | Notes |
|----------|------|----------|-------|
| I2C0 SDA | GPIO4 | I2C | MPU6050, TMP102, HMC5883L (see config.h) |
| I2C0 SCL | GPIO5 | I2C | MPU6050, TMP102, HMC5883L |
| UART1 TX | GPIO4 | UART | GPS NEO-7M RX |
| UART1 RX | GPIO5 | UART | GPS NEO-7M TX |
| Debug UART | USB CDC | USB | stdio_usb (pico_enable_stdio_usb=1) |
| LED | CYW43 | GPIO | Via WiFi chip |

### 3.3 Memory Budget

| Region | Available | Used | Margin |
|--------|-----------|------|--------|
| Flash | 2 MB | ~80 KB (est.) | >95% |
| SRAM | 520 KB | ~60 KB (est.) | >88% |
| FreeRTOS Heap | 128 KB | configTOTAL_HEAP_SIZE | 128 KB configured |

---

## 4. Design Decisions

### 4.1 Data Layer Abstraction (DLA)
- **Decision**: Centralised `SystemStateStore` with mutex-protected getter/setter API instead of direct struct access
- **Rationale**: Decouples producers (sensor task) from consumers (control task, telemetry); enables unit testing without FreeRTOS stubs
- **Trade-off**: Minor overhead per field access; negligible at 10–20 Hz

### 4.2 7-State Quaternion EKF over Complementary Filter
- **Decision**: 7-state EKF `x = [q0, q1, q2, q3, bx, by, bz]` with quaternion attitude + gyro-bias estimation
- **Rationale**: Handles gyro bias drift over multi-hour orbits; quaternion avoids gimbal lock; provides attitude covariance for LQR weighting; analytic 3×3 S⁻¹ avoids matrix decomposition at runtime
- **Trade-off**: ~20× higher CPU cost than complementary filter; still <0.1 ms/cycle at 150 MHz

### 4.3 Scalar Yaw Update via Magnetometer (H = [0,0,1,0,0,0])
- **Decision**: Augment EKF with a separate scalar yaw measurement from tilt-compensated HMC5883L data
- **Rationale**: Yaw is unobservable from accelerometer alone; magnetometer closes the loop without enlarging the observation matrix (EKF_M stays 2)
- **Trade-off**: Degenerate near magnetic poles or with a purely vertical field (|Bh| < 1 µT guard added)

### 4.4 LQR Full-State Controller with Gain Scheduling + PID Fallback
- **Decision**: LQR `u = -Kx` with mode-scheduled gains (FM_NOMINAL: ωn=10, FM_DETUMBLE: ωn=30); PID per-axis fallback in FM_DIAGNOSTIC or when EKF not converged
- **Rationale**: LQR gives optimal transient response for the linearised plant; gain scheduling provides aggressive detumble control; PID is robust and parameter-free for diagnostic mode
- **Trade-off**: LQR gains are fixed at design time; gain scheduling implemented for 2 modes (see `lqr_schedule.c`)

### 4.5 RK2 Midpoint Dynamics Integrator
- **Decision**: Replace forward Euler with RK2 (midpoint) for attitude propagation
- **Rationale**: RK2 reduces attitude error to <0.1 mrad/step at 20 Hz vs. O(dt²) Euler; O(dt³) accuracy with one extra function evaluation
- **Trade-off**: Doubles dynamics compute; still <0.05 ms/step at 150 MHz

### 4.6 Momentum Dump with B×L Detumble Law
- **Decision**: Duty-cycle B-dot / B×L momentum dumping in FM_DETUMBLE mode
- **Rationale**: Standard approach for initial detumble using magnetorquing; no moving parts required
- **Trade-off**: Effectiveness depends on field strength; disabled automatically outside FM_DETUMBLE

### 4.7 Watchdog HAL with Weak-Symbol Stubs
- **Decision**: Hardware watchdog kick behind `__attribute__((weak))` HAL functions; host stub logs periodic messages
- **Rationale**: Required for SYS-REQ-4 (fault-tolerant safe-mode re-entry); weak symbol allows host unit tests to link without Pico SDK
- **Implementation**: `watchdog_hal_feed()`, `watchdog_hal_triggered()`, `watchdog_hal_enable()` in `src/services/watchdog/`
- **Trade-off**: Real watchdog period tuning requires hardware-in-the-loop testing

### 4.8 Hybrid Actuators (RW + Magnetorquer)
- **Decision**: Both reaction wheels and magnetorquers
- **Rationale**: Fault tolerance — RW for fast control, magnetorquer for desaturation
- **Status**: ✅ Both models implemented; momentum dump wired to magnetorquer

---

## 5. Interface Summary

See [INTERFACE_SPECIFICATION.md](../INTERFACE_SPECIFICATION.md) for detailed API contracts.

### Key APIs:

**EKF (attitude estimation)**
- `ekf_init(ekf_t*)` — initialise state, P, Q, R to defaults
- `ekf_predict(ekf_t*, gyro[3], dt)` — gyro-driven state propagation
- `ekf_update(ekf_t*, accel[3])` — accelerometer roll/pitch update
- `ekf_update_mag(ekf_t*, mag_uT[3], declination_rad)` — tilt-compensated yaw update
- `ekf_get_quaternion(ekf_t*, q[4])` / `ekf_get_attitude(ekf_t*, rpy[3])` / `ekf_get_bias(ekf_t*, bias[3])`

**LQR / PID (attitude control)**
- `lqr_init(lqr_t*)` / `lqr_compute(lqr_t*, att_err[3], rate_err[3], torque[3])`
- `lqr_schedule_apply(lqr_t*, flight_mode)` — apply mode-scheduled gains
- `pid_init()`, `pid_update()` — per-axis fallback
- `attitude_control_update()` — error-to-torque dispatcher

**Drivers**
- `mpu6050_init()`, `mpu6050_read_raw()` — IMU (gyro + accel)
- `hmc5883l_init()`, `hmc5883l_read()` — 3-axis magnetometer [µT]
- `temperature_read()` — TMP102 temperature
- `gps_init()`, `gps_read_fix()` — NEO-7M GPS NMEA parser
- `rm3100_read_vector()` — Payload magnetometer (RM3100 SPI)

**Actuators**
- `rw_set_torque()`, `mag_set_dipole()` — command outputs
- `momentum_dump_step(md_t*, B[3], rates[3], dipole[3])` — B×L detumble algorithm

**Watchdog**
- `watchdog_hal_init()`, `watchdog_hal_feed()`, `watchdog_hal_triggered()` — HAL stubs

**Data Layer**
- `data_layer_init()`, `data_layer_read()`, `data_layer_write_*()`
- `data_layer_write_ekf()`, `data_layer_write_mag()`, `data_layer_write_momentum_dump()`, `data_layer_set_gps_fix()`

**Flight Mode Manager**
- `fmm_request_transition(flight_mode_t)`, `fmm_force_safe()`, `fmm_get_mode()`, `fmm_mode_name()`

---

## 6. Future Design Considerations

| Feature | Phase | Status |
|---------|-------|--------|
| RK2 dynamics integrator | Phase 4 | ✅ Done (PR-11) |
| 7-state Quaternion EKF | Phase 4 | ✅ Done (PR-12) |
| LQR full-state controller | Phase 4 | ✅ Done (PR-13) |
| EKF sensor fusion in sensor task | Phase 4 | ✅ Done (PR-14) |
| LQR/PID mode dispatch + gain scheduling | Phase 4/5 | ✅ Done (PR-15, PR-23) |
| Watchdog HAL + kick | Phase 5 | ✅ Done (PR-16) |
| Momentum dump (B×L) | Phase 5 | ✅ Done (PR-17) |
| HMC5883L magnetometer driver | Phase 5 | ✅ Done (PR-18) |
| EKF yaw update via magnetometer | Phase 5 | ✅ Done (PR-19) |
| FreeRTOS SMP (dual-core) | Phase 5 | ⏳ Disabled (`configNUMBER_OF_CORES=1`) |
| On-hardware integration tests | Phase 6 | ⏳ Pending HW |
| Flash-backed persistent log | Phase 6 | ⏳ Planned |
| Redundant sensor voting | Phase 6 | ⏳ Planned |

> **Note**: Quaternion representation is already implemented in Phase 4 (EKF uses quaternion state `x=[q0,q1,q2,q3,bx,by,bz]`).
