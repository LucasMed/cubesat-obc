# System Design Document

**Document ID**: DES-001  
**Version**: 1.0  
**Last Updated**: 2026-02-20  
**Status**: Active

---

## 1. System Overview

The CubeSat OBC implements an Attitude Determination and Control System (ADCS) for a 1U CubeSat platform using a Raspberry Pi Pico 2W (RP2350) as the flight computer.

### 1.1 Design Goals
- Real-time attitude control with 3-DOF (Roll, Pitch, Yaw)
- Modular architecture following ECSS-Q-ST-80C standards
- FreeRTOS-based multitasking with deterministic scheduling
- Hardware abstraction for sensor and actuator portability

### 1.2 System Block Diagram

```
┌─────────────────────────────────────────────────────────────────┐
│                      CubeSat OBC (RP2350)                       │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  ┌──────────────┐      ┌─────────────┐      ┌──────────────┐   │
│  │   SENSORS    │      │  CONTROL    │      │  ACTUATORS   │   │
│  ├──────────────┤      ├─────────────┤      ├──────────────┤   │
│  │ • IMU(I2C)   │─────▶│ • PID Ctrl  │─────▶│ • RW Motors  │   │
│  │ • Temp (I2C) │      │ • Attitude  │      │ • Magnetorq  │   │
│  │ • Vbatt(ADC) │      │   Dynamics  │      │   (PWM/SPI)  │   │
│  └──────────────┘      └─────────────┘      └──────────────┘   │
│         ▲                      │                      ▲         │
│         └──────────────┬───────┴──────────────────────┘         │
│                  ┌─────┴──────────┐                             │
│                  │  SYSTEM STATE  │                              │
│                  │ (Shared Memory)│                              │
│                  └────────────────┘                              │
│                                                                 │
│  ┌────────────┐  ┌──────────────┐  ┌──────────────┐            │
│  │ FreeRTOS   │  │   TELEMETRY  │  │ HEALTH MON.  │            │
│  │  TASKS     │  │  (WiFi/UART) │  │  (Watchdog)  │            │
│  └────────────┘  └──────────────┘  └──────────────┘            │
└─────────────────────────────────────────────────────────────────┘
```

---

## 2. Software Architecture

### 2.1 Module Decomposition

| Module | Path | Responsibility |
|--------|------|---------------|
| Core | `src/core/` | System state management, shared data |
| Drivers | `src/drivers/` | HAL for I2C, UART, ADC sensors |
| Control | `src/control/` | PID controllers, attitude control laws |
| Actuators | `src/actuators/` | Reaction wheel & magnetorquer models |
| Dynamics | `src/dynamics/` | Attitude dynamics simulation |
| Tasks | `src/tasks/` | FreeRTOS task implementations |

### 2.2 Task Architecture

| Task | Rate | Priority | Stack | Timing Budget |
|------|------|----------|-------|---------------|
| SensorRead | 10 Hz | HIGH (3) | 512 words | <50 ms |
| AttitudeControl | 20 Hz | HIGH (3) | 512 words | <25 ms |
| Telemetry | 1 Hz | MEDIUM (2) | 256 words | <500 ms |
| HealthMonitor | 0.2 Hz | LOW (1) | 256 words | <1000 ms |

### 2.3 Data Flow

```
Sensors (10 Hz) → system_state → Control (20 Hz) → Actuators
                       ↓
              Telemetry (1 Hz) → Ground Station
                       ↓
              Health Monitor (0.2 Hz) → Diagnostics
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
| I2C0 SDA | GPIO4 | I2C | MPU6050, TMP102 |
| I2C0 SCL | GPIO5 | I2C | MPU6050, TMP102 |
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

### 4.1 Decoupled PID per Axis
- **Decision**: 3 independent PID loops instead of quaternion-based controller
- **Rationale**: Simpler, lower compute cost (~1 ms/cycle), sufficient for CubeSat
- **Trade-off**: Less robust near gimbal lock; mitigated by small angular rates

### 4.2 Hybrid Actuators (RW + Magnetorquer)
- **Decision**: Both reaction wheels and magnetorquers
- **Rationale**: Fault tolerance — RW for fast control, magnetorquer for de-saturation

### 4.3 Euler Integration
- **Decision**: Forward Euler for dynamics simulation
- **Rationale**: O(dt²) accuracy acceptable at 20 Hz; upgrade to RK4 in Phase 4 if needed

### 4.4 Centralized System State
- **Decision**: Single `system_state_t` struct with mutex protection
- **Rationale**: Small footprint (~1 KB), simpler than message-passing

### 4.5 FreeRTOS over Bare-Metal
- **Decision**: RTOS from day one
- **Rationale**: Real-time scheduling, industry standard, scalable

---

## 5. Interface Summary

See [INTERFACE_SPECIFICATION.md](../INTERFACE_SPECIFICATION.md) for detailed API contracts.

### Key APIs:
- `pid_init()`, `pid_update()` — Per-axis PID control
- `attitude_control_update()` — Attitude error to torque
- `mpu6050_init()`, `mpu6050_read()` — IMU sensor (TBD real implementation)
- `rw_set_torque()`, `mag_set_dipole()` — Actuator commands
- `get_system_state()`, `set_attitude()` — State management

---

## 6. Future Design Considerations

| Feature | Phase | Status |
|---------|-------|--------|
| Kalman filter for attitude estimation | Phase 4 | TBD |
| LQR/MPC advanced control | Phase 4 | TBD |
| Quaternion representation | Phase 4 | TBD |
| Redundant sensor voting | Phase 5 | TBD |
| Autonomous safe-mode | Phase 5 | TBD |
