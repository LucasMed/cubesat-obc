# System Design Document

**Document ID**: DES-001  
**Version**: 2.0  
**Last Updated**: 2026-03-12  
**Status**: Active

---

## 1. System Overview

The CubeSat OBC implements an Attitude Determination and Control System (ADCS) for a 1U CubeSat platform using a Raspberry Pi Pico 2W (RP2350) as the flight computer.

### 1.1 Design Goals
- Real-time attitude determination and control with 3-DOF (Roll, Pitch, Yaw)
- Full yaw observability via tilt-compensated magnetometer EKF update
- Modular architecture following ECSS-Q-ST-80C standards
- FreeRTOS-based multitasking with deterministic scheduling
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
│  │ • MPU6050  (I2C)  │───▶│ • 6-state EKF        │──▶│ • RW      │  │
│  │ • HMC5883L (I2C)  │───▶│   (accel + mag yaw)  │   │ • Mag-torq│  │
│  │ • TMP102   (I2C)  │    │ • LQR  u = -Kx       │   └───────────┘  │
│  │ • Vbatt    (ADC)  │    │ • PID fallback        │                  │
│  └───────────────────┘    │ • RK2 dynamics        │                  │
│                           │ • Momentum dump (B×L) │                  │
│                           └──────────┬───────────┘                  │
│                                      │                              │
│  ┌──────────────────────────────────▼────────────────────────────┐  │
│  │              DATA LAYER ABSTRACTION (DLA)                     │  │
│  │  SystemStateStore  ·  mutex-protected  ·  getter/setter API   │  │
│  └────────────────────────────────────────────────────────────────┘  │
│           │                   │                   │                  │
│  ┌────────▼────────┐  ┌───────▼────────┐  ┌──────▼──────────────┐  │
│  │  TELEMETRY      │  │ HEALTH MONITOR │  │  FAULT / EPS / FMM  │  │
│  │  (WiFi/UART)    │  │ · Watchdog kick│  │  Flight Mode Manager │  │
│  └─────────────────┘  └────────────────┘  └─────────────────────┘  │
└──────────────────────────────────────────────────────────────────────┘
```

---

## 2. Software Architecture

### 2.1 Module Decomposition

| Module | Path | Responsibility |
|--------|------|---------------|
| Core | `src/core/` | Data Layer Abstraction, FMM, Fault Manager, EPS Monitor, Logger |
| Drivers | `src/drivers/` | HAL for I2C, UART, ADC; MPU6050, TMP102, HMC5883L |
| Control | `src/control/` | 6-state EKF, LQR controller, PID, RK2 dynamics |
| Actuators | `src/actuators/` | Reaction wheel & magnetorquer models |
| Dynamics | `src/dynamics/` | RK2 attitude dynamics integrator |
| Services | `src/services/` | Watchdog HAL, momentum dump algorithm |
| Tasks | `src/tasks/` | FreeRTOS task implementations (4 tasks) |

### 2.2 Task Architecture

| Task | Rate | Priority | Stack | Timing Budget |
|------|------|----------|-------|---------------|
| SensorRead | 10 Hz | HIGH (3) | 512 words | <50 ms |
| AttitudeControl | 20 Hz | HIGH (3) | 512 words | <25 ms |
| Telemetry | 1 Hz | MEDIUM (2) | 256 words | <500 ms |
| HealthMonitor | 0.2 Hz | LOW (1) | 256 words | <1000 ms |

### 2.3 Data Flow

```
MPU6050 + HMC5883L (10 Hz)
        │
        ▼
  ekf_predict()          ← gyro rates
  ekf_update()           ← accel (roll, pitch)
  ekf_update_mag()       ← tilt-compensated yaw (H=[0,0,1,0,0,0])
        │
        ▼
  Data Layer (DLA)       ← EKF attitude + bias + P diagonal
        │
  ┌─────┴──────────────────────────────┐
  │                                    │
  ▼                                    ▼
LQR u=-Kx (FM_NOMINAL + EKF valid)   Telemetry (1 Hz) → Ground
PID fallback (FM_DIAGNOSTIC)          Health Monitor (0.2 Hz) + Watchdog kick
  │
  ▼
Momentum dump?  (FM_DETUMBLE + |h|>threshold)
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
| I2C0 SDA | GPIO4 | I2C | MPU6050, TMP102, HMC5883L |
| I2C0 SCL | GPIO5 | I2C | MPU6050, TMP102, HMC5883L |
| UART0 TX | GPIO0 | UART | Debug console |
| UART0 RX | GPIO1 | UART | Debug console |
| LED | CYW43 | GPIO | Via WiFi chip |

### 3.3 Memory Budget

| Region | Available | Used | Margin |
|--------|-----------|------|--------|
| Flash | 2 MB | ~536 KB (blink) | >70% |
| SRAM | 520 KB | ~60 KB (est.) | >88% |
| FreeRTOS Heap | 32 KB | TBD | TBC |

---

## 4. Design Decisions

### 4.1 Data Layer Abstraction (DLA)
- **Decision**: Centralised `SystemStateStore` with mutex-protected getter/setter API instead of direct struct access
- **Rationale**: Decouples producers (sensor task) from consumers (control task, telemetry); enables unit testing without FreeRTOS stubs
- **Trade-off**: Minor overhead per field access; negligible at 10–20 Hz

### 4.2 6-State EKF over Complementary Filter
- **Decision**: 6-state EKF `x = [roll, pitch, yaw, bx, by, bz]` with gyro-bias estimation
- **Rationale**: Handles gyro bias drift over multi-hour orbits; provides attitude covariance for LQR weighting; analytic 2×2 S⁻¹ avoids matrix decomposition at runtime
- **Trade-off**: ~20× higher CPU cost than complementary filter; still <0.1 ms/cycle at 150 MHz

### 4.3 Scalar Yaw Update via Magnetometer (H = [0,0,1,0,0,0])
- **Decision**: Augment EKF with a separate scalar yaw measurement from tilt-compensated HMC5883L data
- **Rationale**: Yaw is unobservable from accelerometer alone; magnetometer closes the loop without enlarging the observation matrix (EKF_M stays 2)
- **Trade-off**: Degenerate near magnetic poles or with a purely vertical field (|Bh| < 1 µT guard added)

### 4.4 LQR Full-State Controller with PID Fallback
- **Decision**: LQR `u = -Kx` in FM_NOMINAL when `imu_ekf_valid`; PID per-axis fallback in FM_DIAGNOSTIC or during EKF convergence
- **Rationale**: LQR gives optimal transient response for the linearised plant; PID is robust and parameter-free to re-tune
- **Trade-off**: LQR gains are fixed at design time (ωn=10 rad/s, ζ=1); gain scheduling deferred to future sprint

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
- `ekf_get_attitude(ekf_t*, rpy[3])` / `ekf_get_bias(ekf_t*, bias[3])`

**LQR / PID (attitude control)**
- `lqr_init(lqr_t*, K[3][6])` / `lqr_update(lqr_t*, state[6], torque[3])`
- `pid_init()`, `pid_update()` — per-axis fallback
- `attitude_control_update()` — error-to-torque dispatcher

**Drivers**
- `mpu6050_init()`, `mpu6050_read_raw()` — IMU (gyro + accel)
- `hmc5883l_init()`, `hmc5883l_read()` — 3-axis magnetometer [µT]
- `temperature_read()` — TMP102 temperature

**Actuators**
- `rw_set_torque()`, `mag_set_dipole()` — command outputs
- `momentum_dump_update(md_t*, h[3], B[3])` — B×L detumble algorithm

**Watchdog**
- `watchdog_hal_init()`, `watchdog_hal_kick()`, `watchdog_hal_enable(ms)` — HAL stubs

**Data Layer**
- `data_layer_init()`, `data_layer_read()`, `data_layer_write_*()`
- `data_layer_write_ekf()`, `data_layer_write_mag()`, `data_layer_write_momentum_dump()`

---

## 6. Future Design Considerations

| Feature | Phase | Status |
|---------|-------|--------|
| RK2 dynamics integrator | Phase 4 | ✅ Done (PR-11) |
| 6-state EKF (gyro bias) | Phase 4 | ✅ Done (PR-12) |
| LQR full-state controller | Phase 4 | ✅ Done (PR-13) |
| EKF sensor fusion in sensor task | Phase 4 | ✅ Done (PR-14) |
| LQR/PID mode dispatch | Phase 4 | ✅ Done (PR-15) |
| Watchdog HAL + kick | Phase 5 | ✅ Done (PR-16) |
| Momentum dump (B×L) | Phase 5 | ✅ Done (PR-17) |
| HMC5883L magnetometer driver | Phase 5 | ✅ Done (PR-18) |
| EKF yaw update via magnetometer | Phase 5 | ✅ Done (PR-19) |
| On-hardware integration tests | Phase 6 | ⏳ Pending HW |
| Flash-backed persistent log | Phase 6 | ⏳ Planned |
| Gain scheduling / adaptive LQR | Phase 6 | ⏳ Planned |
| Quaternion attitude representation | Phase 6 | ⏳ Planned |
| Redundant sensor voting | Phase 6 | ⏳ Planned |
