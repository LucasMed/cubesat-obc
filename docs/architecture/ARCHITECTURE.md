# CubeSat OBC Architecture

## System Overview

The CubeSat On-Board Computer (OBC) is a modular, real-time flight software system for attitude determination and control (ADCS) on a 1U CubeSat platform.

**Target Hardware**: Raspberry Pi Pico 2W (RP2350 dual-core Cortex-M33 MCU, 520 KB SRAM, WiFi via CYW43)
**OS**: FreeRTOS with `ARM_CM33_NTZ` port to securely handle ARMv8-M memory & FPU dynamic exception frames.

---

## System Architecture Diagram

```
┌─────────────────────────────────────────────────────────────────┐
│                      CubeSat OBC (RP2350)                       │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  ┌──────────────┐      ┌─────────────┐      ┌──────────────┐    │
│  │   SENSORS    │      │  CONTROL    │      │  ACTUATORS   │    │
│  ├──────────────┤      ├─────────────┤      ├──────────────┤    │
│  │ • IMU(I2C)   │─────▶│ • PID Ctrl  │─────▶│ • RW Motors  │    │
│  │ • Temp (I2C) │      │ • Attitude  │      │ • Magnetorq  │    │
│  │ • Vbatt(ADC) │      │   Dynamics  │      │   (PWM/SPI)  │    │
│  └──────────────┘      └─────────────┘      └──────────────┘    │
│         ▲                      │                      ▲         │
│         │                      ▼                      │         │
│         │            ┌──────────────────┐            │          │
│         └────────────│  SYSTEM STATE    │────────────┘          │
│                      │ (Shared Memory)  │                       │
│                      └──────────────────┘                       │
│                              ▲                                  │
│         ┌────────────────────┼────────────────────┐             │
│         │                    │                    │             │
│         ▼                    ▼                    ▼             │
│  ┌────────────┐    ┌──────────────┐    ┌──────────────┐         │
│  │ FreeRTOS   │    │   TELEMETRY  │    │ HEALTH MON.  │         │
│  │  TASKS     │    │  (WiFi/UART) │    │  (Watchdog)  │         │
│  └────────────┘    └──────────────┘    └──────────────┘         │
│   • SensorRead              │                    │              │
│   • AttitudeCtrl       ┌────────┐           ┌────────┐          │
│   • Telemetry   ───────│CYW43   │           │Watchdog│          │
│   • HealthMon          │(WiFi)  │           │ (boot) │          │
│                        └────────┘           └────────┘          │
└─────────────────────────────────────────────────────────────────┘
```

---

## Physical Connection Table

| Module                  | Signal        | Pico 2W GPIO | Pico 2W Physical Pin | Notes / Bus         |
|-------------------------|--------------|----------------|----------------------|---------------------|
| **MPU-6050 (IMU)**      | SDA          | GPIO2          | 4                    | I2C0 SDA            |
|                         | SCL          | GPIO3          | 5                    | I2C0 SCL            |
|                         | VCC          | 3V3            | 36 or 39             | Power               |
|                         | GND          | GND            | 3, 8, 13, ...        | Ground              |
| **HMC5883L (Magnet.)**  | SDA          | GPIO2          | 4                    | I2C0 SDA            |
|                         | SCL          | GPIO3          | 5                    | I2C0 SCL            |
|                         | VCC          | 3V3            | 36 or 39             | Power               |
|                         | GND          | GND            | 3, 8, 13, ...        | Ground              |
| **GPS (NEO-6M/7M)**     | TX           | GPIO1          | 2                    | UART0 RX (Pico)     |
|                         | RX           | GPIO0          | 1                    | UART0 TX (Pico)     |
|                         | VCC          | 3V3            | 36 or 39             | Power               |
|                         | GND          | GND            | 3, 8, 13, ...        | Ground              |
| **HC-12/Si4463 (Radio)**| TX           | GPIO5          | 7                    | UART1 RX (Pico)     |
|                         | RX           | GPIO4          | 6                    | UART1 TX (Pico)     |
|                         | VCC          | 3V3            | 36 or 39             | Power               |
|                         | GND          | GND            | 3, 8, 13, ...        | Ground              |
| **Battery Monitor**     | +Vbat        | GPIO26         | 31                   | ADC0                |
| **Temp. Board**         | -            | GPIO27         | 32                   | ADC1 (onboard)      |
| **External Watchdog**   | Kick         | GPIO20         | 26                   | Digital output      |
| **Reaction Wheel 1**    | PWM          | GPIO8          | 11                   | PWM4A               |
| **Reaction Wheel 2**    | PWM          | GPIO9          | 12                   | PWM4B               |
| **Reaction Wheel 3**    | PWM          | GPIO3          | 5                    | PWM1B               |
| **Magnetorquer X**      | -            | GPIO17         | 22                   | Digital output      |
| **Magnetorquer Y**      | -            | GPIO23         | 34                   | Digital output      |
| **Magnetorquer Z**      | -            | GPIO24         | 35                   | Digital output      |

> **Note:** You can connect all modules to the same 3V3 and GND pin, as long as the total current does not exceed the Pico 2W's power supply capability. For sensors and small modules, this is safe.

---

## Subsystem Modules

### 1. **Core System State** (`src/core/`)
- **Purpose**: Centralized vehicle state (attitude, rates, sensor readings)
- **Key Data**: Euler angles, angular rates, IMU data, battery voltage
- **Access**: Thread-safe reads/writes by RTOS tasks
- **Files**: `system_state.c`, `system_state.h`

### 2. **Sensor Drivers** (`src/drivers/`)
- **IMU Driver** (MPU6050): 6-DOF accelerometer + gyroscope via I2C
- **Temperature Sensor**: TMP102 or onboard sensor readout
- **Power Monitor**: Battery voltage via ADC
- **Magnetometer Driver** — **EM baseline: HMC5883L (GY-271)**, I2C0 addr `0x1E`, ODR 75 Hz;
  ⚠️ QMC5883L clone risk in GY-271 modules — verify IC markings before procurement;
  used by EKF for yaw estimation via tilt-compensated update (`ekf_update_mag()`).
  **CDR/FM candidate: LIS3MDL** (STMicroelectronics, I2C0 addr `0x1C` SA0=GND, ODR 80 Hz) —
  actively produced, new driver `lis3mdl.c` required (no backward compatibility with HMC5883L register map).
  Part selection locked per ACT-11 (SRR-OBC-001).
- **Design Rationale**: Hardware abstraction layer (HAL) pattern—easy to swap sensors
- **Files**: `drivers/imu/mpu6050.c`, `drivers/mag/hmc5883l.c` [EM]; `drivers/mag/lis3mdl.c` [CDR scope, not yet created]

### 3. **Control System** (`src/control/`)
- **PID Controller**: Decoupled per axis (roll, pitch, yaw)
  - Proportional, Integral, Derivative gains tunable per axis
  - Anti-windup on integral term
  - Output saturation to protect actuators
- **Attitude Controller**: Higher-level control law
  - Converts attitude error to rate commands
  - Feeds to PID for rate stabilization
  - Supports null-space momentum management
- **Files**: `pid_controller.c`, `attitude_control.c`

### 4. **Actuators** (`src/actuators/`)
- **Reaction Wheels** (3-axis): Angular momentum storage for ADCS
  - Model: torque-to-momentum conversion
  - Momentum dump via magnetorquers when saturated
- **Magnetorquers** (3-axis): Magnetic dipole interaction with Earth's field
  - **Primary ADCS actuator (Phase 1)**: B-dot detumbling and safe-mode attitude hold achievable
    without reaction wheels
  - Reaction wheels required only for precision pointing (Phase 2 ADCS)
  - Used for momentum desaturation (B×L dump) when RWs are saturated
- **Design Rationale**: Magnetorquers-first strategy — MTQ-only enables full detumbling and SAFE MODE;
  RWs add precision pointing in a later phase. Fault tolerance: loss of all RWs still permits MTQ-only safe mode.
- **Files**: `reaction_wheel.c`, `magnetorquer.c`, `services/adcs/momentum_dump.c`

### 5. **Attitude Dynamics** (`src/dynamics/`)
- **Model**: Rigid body rotation dynamics
  - Euler integration (RK2 optional upgrade)
  - Inertia tensor parameterized for CubeSat geometry
  - Actuator torque + disturbance torque inputs
- **Purpose**: Simulation on ground; will be replaced by real sensors in flight
- **Files**: `attitude_dynamics.c`

### 6. **FreeRTOS Task Layer** (`src/tasks/`)
- **SensorRead Task** (10 Hz): Poll IMU, temperature, voltage
- **AttitudeControl Task** (20 Hz): Compute control commands
- **Telemetry Task** (1 Hz): Transmit state to ground station
- **HealthMonitor Task** (0.2 Hz): Bus voltage, thermal monitoring, internal MCU watchdog kick +
  external TPS3431 watchdog feed (`watchdog_hal_feed()` on GPIO20, 3 s timeout)
- **Design**: Priority levels (HIGH/MEDIUM/LOW) prevent starvation
- **Files**: 4 task implementations + task headers

### 7. **Communication** (`third_party/libcsp/`, `src/core/comm_init.c` - Phase 3)
- **Protocol**: CubeSat Space Protocol (CSP) v2
- **Physical Layer**: UART1 acts as the primary telemetry/command link via KISS framing.
- **Telemetry Task** (Port 10): Emits 1 Hz packed binary `csp_telemetry_packet_t` over CSP (connection-less).
- **Command Task** (Port 20): Listens for uplink commands (Echo, Reboot, Set Mode).
- **UART0**: Remapped to GPS NEO-7M @ 9600 baud, NMEA 0183 (`$GPGGA`/`$GPRMC`); debug output
  migrated to USB CDC (`pico_enable_stdio_usb = 1`).

---

## Data Flow Diagram

```
TIME → 

SensorRead (10 Hz):
  IMU ──(I2C)──▶ mpu6050_read() ──▶ system_state.imu_data ┐
  Temp ─(I2C)─▶ temp_read()     ──▶ system_state.temp     │
  Vbatt(ADC)───▶ adc_read()     ──▶ system_state.vbatt    │
                                                           │
AttitudeControl (20 Hz):                         ╔═════════╩═════════╗
  system_state.imu_data                          ║ SYSTEM STATE      ║
  system_state.control_gains ──▶ pid_update() ── ║ (shared memory)   ║
  ──▶ attitude_control()                         ║                   ║
  ──▶ system_state.control_torque                ║ RW Rate Cmds (Hz) ║
           │                                     ║ Magnetorq Cmds    ║
           ▼                                     ║ Attitude (Euler)  ║
  Actuator Models:                               ║ Angular Rates     ║
  rw_apply_torque()       (momentum change)      ║ Sensor Readings   ║
  magnetorquer_dipole()                          ╚═══════════════════╝
           │
           ▼
  Dynamics Update (Euler):
  attitude_dynamics_step()
           │
           ▼
  system_state.attitude (updated)
  system_state.angular_rates (updated)


Telemetry (1 Hz):
  system_state.{attitude, rates, sensor_data}
           │
           ▼
  format_telemetry_packet()
           │
           ▼
  WiFi TX (CYW43) ──▶ Ground Station
  UART TX (fallback)
```

---

## Design Decisions & Rationale

### 1. **Decoupled PID per Axis**
- **Decision**: Use 3 independent PID loops (roll, pitch, yaw) instead of quaternion-based controller
- **Rationale**: 
  - Simpler implementation, easier to tune for small-sat applications
  - Avoids gimbal lock issues (Euler angles sufficient for CubeSat)
  - Low computational cost (~1 ms per 10 Hz cycle on RP2350)
  - Can upgrade to LQR/MPC in Phase 4 if needed
- **Trade-off**: Less robust near singularities; mitigated by small angular rates in nadir-pointing mode

### 2. **Hybrid Actuator (RW + Magnetorquer)**
- **Decision**: Both reaction wheels and magnetorquers instead of single actuator type
- **Rationale**:
  - RW: Fast, direct control, but accumulates momentum
  - Magnetorquer: Slow response, but provides de-saturation without fuel
  - Fault tolerance: Loss of one RW still allows magnetorquer stabilization
  - CubeSat missions often include magnetic field (Earth's field available)
- **Trade-off**: Increased code complexity; managed via clear HAL

### 3. **Euler Integration (not RK4)**
- **Decision**: Simple Euler forward integration for attitude dynamics
- **Rationale**:
  - RP2350 runs attitude update in simulation only (Phase 2 adds real sensor fusion)
  - Euler is O(dt²); acceptable for 20 Hz update rate during ground testing
  - Can add RK2/RK4 in Phase 4 if validation requires higher-order integration
- **Trade-off**: Slight loss of accuracy; sufficient for control law validation

### 4. **Centralized System State (not distributed)**
- **Decision**: Single shared `system_state_t` struct accessed by all tasks
- **Rationale**:
  - Simplifies data consistency (vs. message-passing overhead)
  - Small memory footprint (~1 KB for state)
  - Thread-safe access via recursive mutexes (essential for SMP)

### 5. **Software Floating-Point (Soft-FP)**
- **Decision**: Force `-mfloat-abi=soft` in build configuration.
- **Rationale**: 
  - The FreeRTOS port (RP2350) does not handle FPU context switching.
  - Using hardware FPU on RP2350 leads to stack corruption during task switches.
  - Soft-FP ensures stability without significant performance impact for the 10-20 Hz loop.
  - Clear ownership model in tasks (read/write boundaries documented)
- **Trade-off**: Careful mutex/critical section use needed; RTOS handles this

### 5. **FreeRTOS over Bare-Metal Loop**
- **Decision**: Use RTOS task abstraction from day one
- **Rationale**:
  - Real-time scheduling guarantees (task priorities)
  - Scalability for Phase 3 (comms, monitoring)
  - Industry standard for flight software
  - Easier to add watchdog, safety monitors
- **Trade-off**: ~10 KB SRAM for RTOS kernel; acceptable on RP2350

### 6. **Host-Buildable Scaffold**
- **Decision**: Compile and test on Linux (GCC) before Pico SDK integration
- **Rationale**:
  - Faster iteration during development
  - Enables CI/CD before hardware available
  - Logic validation independent of cross-toolchain issues
  - FreeRTOS stubs allow task structure testing
- **Trade-off**: Stubs are incomplete; real integration required for flight

---

## Interface Specifications Summary

See [INTERFACE_SPECIFICATION.md](INTERFACE_SPECIFICATION.md) for detailed API contracts.

### Key Interfaces:
1. **PID Controller**: `pid_init()`, `pid_update()` — per-axis control law
2. **Attitude Control**: `attitude_control_update()` — error to rate mapping
3. **Sensor Drivers**: `mpu6050_read()`, `temp_read()` — I2C abstractions
4. **Actuators**: `rw_set_torque()`, `mag_set_dipole()` — command inputs
5. **System State**: `get_system_state()`, `set_attitude()` — state management

---

## Traceability Matrix

See [TRACEABILITY_MATRIX.md](TRACEABILITY_MATRIX.md) for requirements-to-tests mapping.

### Coverage:
- **Functional Requirements** (attitude control, sensor fusion): 8 requirements
- **Non-Functional Requirements** (real-time, power budget): 5 requirements
- **Unit Tests**: 3 (100% passing)
- **Integration Tests**: Planned Phase 3

---

## Technology Stack

| Layer | Technology | Rationale |
|-------|-----------|-----------|
| MCU | RP2350 (Pico 2W) | Low cost, dual-core, integrated WiFi, ARM Cortex-M0+ |
| RTOS | FreeRTOS | Open-source, flight-proven, wide industry adoption |
| Build | CMake | Cross-platform, modular, industry standard |
| Language | C (C11) | Deterministic, low overhead, MISRA compliance |
| Testing | CTest + Unit Tests | Native C testing, CI integration |
| Sensors | I2C (IMU, Temp) | Standard protocol, widely available components |
| Actuators | PWM/SPI (RW motors) | Standard digital control, DMA-capable on RP2350 |

---

## Performance Targets

| Metric | Target | Status |
|--------|--------|--------|
| Attitude Determination Accuracy | ±5° (before EKF) | ✅ EKF operational (±3° target) |
| Control Loop Rate | 20 Hz | Confirmed |
| Telemetry Rate | 1 Hz | Confirmed |
| Real-Time Task Jitter | <1 ms | ✅ Confirmed (±22 µs) |
| Power Budget | <2 W (average) | ⏳ TBD Phase 3 |
| Memory Footprint | <100 KB flash, <60 KB SRAM | ~80 KB used (on track) |

---

## Phase 4 Achievements (Advanced Control — Complete)

1. **EKF Attitude Estimator** ✅: 6-state EKF with gyro-bias estimation; analytic
   2×2 S⁻¹ inversion; integrated into 10 Hz sensor_read_task loop.
2. **LQR Controller** ✅: 3×6 full-state gain matrix (ωn=10 rad/s, ζ=1);
   dispatched in FM_NOMINAL when EKF is converged.
3. **RK2 Dynamics** ✅: Midpoint integration replacing forward-Euler in
   attitude_dynamics_step.

## Future Enhancements (Phase 5+)

1. **Magnetometer Fusion**: Add magnetometer observation to EKF for yaw observability.
2. **Momentum Dumping**: Automated de-saturation strategy for reaction wheels.
3. **Redundancy**: Dual-sensor voting, graceful degradation.
4. **Safety**: Autonomous safe-mode, watchdog + supervised shutdown.

---

**Last Updated**: 2026-03-08
**Author**: OBC Development Team
**Status**: Phase 3 (Communication & Telemetry) Complete
