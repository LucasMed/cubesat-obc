# ICD-OBC-001 — Interface Control Document
## CubeSat OBC Hardware/Software Interface

**Document ID**: ICD-OBC-001  
**Version**: 1.0  
**Date**: 2026-03-05  
**Status**: Released  
**Branch merged**: `feature/hardware-bom`  
**Depends on**: SAD v1.0, BOM v1.0, `config/pico_pins.h`

---

## 1. Scope

This document defines the electrical and logical interfaces between the OBC hardware
platform (Raspberry Pi Pico 2W / RP2350) and all connected peripherals used by the
flight software.

Interfaces covered:

| Bus / Interface | Peripherals |
|----------------|------------|
| I2C0 | MPU-6050 IMU, HMC5883L Magnetometer |
| UART0 | GPS NEO-7M |
| UART1 | TT&C Radio E22-400M30S / HC-12 |
| PWM | Reaction Wheels (RW1–3), Magnetorquers (MTQ X/Y/Z) |
| ADC | Battery voltage, temperature |
| GPIO | External watchdog TPS3431, status LED |
| USB CDC | Debug console |

> **Authoritative pin source**: `config/pico_pins.h`

---

## 2. OBC Hardware Platform

**Main controller**: Raspberry Pi Pico 2W

| Parameter | Value |
|-----------|-------|
| MCU | RP2350 |
| Core | Cortex-M33 (dual-core, single-core used in FSW) |
| Clock | 150 MHz |
| Flash | 2 MB |
| SRAM | 520 KB |
| I2C | 2× (I2C0, I2C1) |
| UART | 2× (UART0, UART1) |
| PWM slices | 8 (16 channels) |
| ADC channels | 4 (GPIO26–29) + 1 internal temp |
| USB | USB 1.1 device (CDC) |

---

## 3. Bus Topology

```
                    ┌──────────────────┐
                    │       OBC        │
                    │     RP2350       │
                    └────────┬─────────┘
                             │
       ┌─────────────────────┼──────────────────────┐
       │                     │                      │
     I2C0                  UART0                 UART1
   GPIO4/5               GPIO0/1               GPIO4/5 *
       │                     │                      │
┌──────┴──────┐         ┌────┴────┐        ┌────────┴────────┐
│  MPU-6050   │         │ NEO-7M  │        │ E22-400M30S     │
│  IMU        │         │ GPS     │        │ LoRa TT&C       │
└──────┬──────┘         └─────────┘        └─────────────────┘
       │
┌──────┴──────┐
│  HMC5883L   │ ← lab
│  Magnetomtr │
└─────────────┘

       PWM                  ADC                  GPIO
  GPIO6/7/8            GPIO26 (ADC0)           GPIO20
       │                     │                    │
┌──────┴──────┐        ┌─────┴────┐        ┌─────┴──────┐
│ RW Motor    │        │ Vbatt    │        │ TPS3431    │
│ ×3 TB6612   │        │ divider  │        │ Watchdog   │
└─────────────┘        └──────────┘        └────────────┘

  GPIO14/15/16
       │
┌──────┴──────┐
│ Magnetorqr  │
│ ×3 DRV8833  │
└─────────────┘

* UART1 and I2C0 share GPIO4/GPIO5. The firmware activates
  I2C0 for sensors and UART1 for TT&C as separate build
  configurations — do not use simultaneously.
```

---

## 4. I2C0 Bus Interface

| Parameter | Value |
|-----------|-------|
| Peripheral | I2C0 (`i2c0`) |
| Speed | 400 kHz (fast-mode) |
| SDA pin | **GPIO4** (`I2C0_SDA_PIN`) |
| SCL pin | **GPIO5** (`I2C0_SCL_PIN`) |
| Pull-ups | 4.7 kΩ to 3.3 V (external, required — see BOM §10 #9) |

**Connected devices:**

| Device | I2C Address | Driver |
|--------|------------|--------|
| MPU-6050 IMU | `0x68` (AD0=GND) | `src/drivers/imu/mpu6050.c` |
| HMC5883L Magnetometer | `0x1E` | `src/drivers/mag/hmc5883l.c` |

> **Shared pin conflict**: GPIO4/GPIO5 are also mapped to UART1 TX/RX. The firmware
> selects one function at compile time. I2C0 is active during normal flight (sensor
> reading); UART1 is active for TT&C communications. Do not enable both simultaneously.

---

## 5. IMU Interface (MPU-6050)

| Parameter | Value |
|-----------|-------|
| Sensor | MPU-6050 |
| Bus | I2C0 |
| Address | `0x68` |
| GPIO | GPIO4 (SDA), GPIO5 (SCL) |
| Driver | `src/drivers/imu/mpu6050.c` |
| Output rate | 100 Hz (gyro + accel) |

**Output data:**

| Signal | Units | Consumer |
|--------|-------|---------|
| `gyro_x/y/z` | rad/s | `SensorReadTask` → EKF |
| `accel_x/y/z` | m/s² | `SensorReadTask` → EKF |

**API:**
```c
bool mpu6050_init(void);
bool mpu6050_read(imu_data_t *data);   // returns gyro [rad/s] + accel [m/s²]
```

---

## 6. Magnetometer Interface (HMC5883L)

| Parameter | Value |
|-----------|-------|
| Sensor | HMC5883L (GY-271 module) |
| Bus | I2C0 |
| Address | `0x1E` |
| GPIO | GPIO4 (SDA), GPIO5 (SCL) |
| Driver | `src/drivers/mag/hmc5883l.c` |
| Output rate | 75 Hz (configured in driver) |

**Output data:**

| Signal | Units | Consumer |
|--------|-------|---------|
| `mag_x/y/z` | µT | `SensorReadTask` → `ekf_update_mag()` → yaw estimate |
| `mag_x/y/z` | µT | `momentum_dump()` → B×L desaturation |

**API:**
```c
bool hmc5883l_init(void);
bool hmc5883l_read(mag_data_t *data);  // returns [µT] per axis
```

> **⚠️ Procurement note — QMC5883L clone risk**: Many GY-271 modules contain a
> **QMC5883L** (I2C addr `0x0D`, different register map) instead of the genuine
> HMC5883L (`0x1E`). Verify IC markings before use. If QMC5883L is detected,
> update I2C address and register map in the driver.

---

### 6.1 Design Note — Magnetometer Migration Path

```
Current lab configuration          Future flight configuration
────────────────────────           ───────────────────────────
HMC5883L (I2C 0x1E)      ──CDR──▶ LIS3MDL (I2C 0x1C, SA0=GND)
hmc5883l.c                         lis3mdl.c (to be developed)
```

The current laboratory hardware uses the **HMC5883L** due to wide availability in
low-cost breakout modules and full driver integration (`src/drivers/mag/hmc5883l.c`).

For the **flight model**, the **LIS3MDL** (STMicroelectronics) is the recommended
baseline magnetometer:

| Feature | HMC5883L (lab) | LIS3MDL (flight) |
|---------|---------------|-----------------|
| Status | Discontinued | Active production |
| I2C address | `0x1E` | `0x1C` (SA0=GND) |
| Full-scale range | ±8 Gauss | ±16 Gauss |
| Output rate | 75 Hz max | 80 Hz (continuous) |
| Driver | `hmc5883l.c` ✅ | `lis3mdl.c` (CDR scope) |
| BOM section | §3 / §3.1 | §3.1 (recommendation) |

**Recommended driver architecture for clean migration:**
```
src/drivers/mag/
    mag_interface.h      ← common API: mag_init(), mag_read()
    mag_hmc5883l.c       ← lab implementation
    mag_lis3mdl.c        ← flight implementation (CDR)
```
Switching sensor only requires selecting the implementation at compile time;
the EKF (`ekf_update_mag()`) and `momentum_dump()` remain unchanged.

---

## 7. GPS Interface (NEO-7M)

| Parameter | Value |
|-----------|-------|
| Module | GY-NEO6Mv2 with NEO-7M |
| Bus | UART0 (`uart0`) |
| TX pin | **GPIO0** (`UART0_TX_PIN`) |
| RX pin | **GPIO1** (`UART0_RX_PIN`) |
| Baud rate | 9600 bps |
| Protocol | NMEA 0183 — sentences `$GPGGA`, `$GPRMC` |
| Logic voltage | 3.3 V |
| Driver | `src/drivers/gps/neo7m.c` (planned) |

**Output data:**

| NMEA sentence | Data | Consumer |
|--------------|------|---------|
| `$GPGGA` | Position (lat/lon/alt), fix quality, UTC | Navigation task, Telemetry |
| `$GPRMC` | Position, speed, course, UTC date | Navigation task |

> **Debug note**: UART0 was previously used for ASCII debug output. Debug is now
> fully routed to **USB CDC** (`pico_enable_stdio_usb = 1` in `src/CMakeLists.txt`).
> UART0 is exclusively reserved for GPS.

**Pending firmware work:**
- [ ] NMEA parser driver `src/drivers/gps/neo7m.c`
- [ ] FreeRTOS GPS task (parse + write to DLA)

---

## 8. TT&C Radio Interface (E22-400M30S / HC-12)

| Parameter | Value |
|-----------|-------|
| Flight module | EBYTE E22-400M30S (SX1268, 433 MHz LoRa, 30 dBm) |
| Lab module | HC-12 (Si4463, 433 MHz FSK, 20 dBm) |
| Bus | UART1 (`uart1`) |
| TX pin | **GPIO4** (`UART1_TX_PIN`) |
| RX pin | **GPIO5** (`UART1_RX_PIN`) |
| Baud rate | 115200 bps |
| Protocol | KISS framing → CSP v2 (OBC addr=10, GS addr=1) |
| Driver | `src/drivers/uart/pico_usart.c` |
| Stack | `third_party/libcsp/` |

**Software stack:**
```
TelemetryTask / CommandTask
        │
    libcsp (CSP v2)
        │
    KISS framing
        │
    pico_usart.c (UART1)
        │
    E22-400M30S  ←→  Ground Station
```

**RF chain (flight):**
```
RP2350 UART1
   │
E22-400M30S (SX1268, 30 dBm)
   │
SAW filter 433 MHz (TDK B39431) ← EMI isolation
   │
λ/4 dipole antenna (16.4 cm at 434 MHz)
```

**E22 mode pins:**

| Signal | GPIO | Value | Description |
|--------|------|-------|-------------|
| M0 | TBD (free GPIO) | 0 | Transparent UART mode |
| M1 | TBD (free GPIO) | 0 | Transparent UART mode |
| AUX | TBD (optional) | — | Busy/ready flag |

> M0/M1/AUX GPIOs to be assigned in `pico_pins.h` during hardware integration.

**Link budget (SSO, critical case):**

| Case | Slant range | Margin |
|------|-------------|--------|
| Zenith pass (600 km) | 600 km | +20.2 dB ✅ |
| Horizon pass (5° el.) | 2300 km | **+8.5 dB ✅** |

---

## 9. ADCS Actuator Interface

### 9.1 Reaction Wheels (TB6612FNG)

| Parameter | Value |
|-----------|-------|
| Driver IC | TB6612FNG H-bridge (lab) |
| Interface | PWM + direction GPIO |
| Driver | `src/actuators/reaction_wheel.c` |

| Axis | PWM GPIO | PWM Slice/Ch | Notes |
|------|----------|-------------|-------|
| RW1 | **GPIO6** (`RW_MOTOR1_PIN`) | PWM3A | `AttitudeControlTask` |
| RW2 | **GPIO7** (`RW_MOTOR2_PIN`) | PWM3B | `AttitudeControlTask` |
| RW3 | **GPIO8** (`RW_MOTOR3_PIN`) | PWM4A | `AttitudeControlTask` |

> Direction GPIOs for TB6612 (AIN1/AIN2) to be assigned in `pico_pins.h`
> during Phase 7 actuator HAL integration.

### 9.2 Magnetorquers (DRV8833) — Phase 1 ADCS Primary Actuator

| Parameter | Value |
|-----------|-------|
| Driver IC | DRV8833 H-bridge (lab) |
| Interface | Bidirectional PWM (2 pins per axis) |
| Driver | `src/actuators/magnetorquer.c` |
| Control law | B-dot detumbling (Phase 1) + B×L dump |

| Axis | PWM GPIO | PWM Slice/Ch | Notes |
|------|----------|-------------|-------|
| MTQ X | **GPIO14** (`MAG_X_PIN`) | PWM7A | `AttitudeControlTask` |
| MTQ Y | **GPIO15** (`MAG_Y_PIN`) | PWM7B | `AttitudeControlTask` |
| MTQ Z | **GPIO16** (`MAG_Z_PIN`) | PWM0A | `AttitudeControlTask` |

> **ADCS strategy**: Magnetorquers are the **primary Phase 1 actuator** — B-dot
> detumbling and SAFE MODE attitude hold require no reaction wheels. Reaction
> wheels are activated in Phase 2 for precision pointing.

---

## 10. Power Monitoring Interface (ADC)

| Parameter | Value |
|-----------|-------|
| ADC peripheral | ADC0 |
| GPIO | **GPIO26** (`ADC_VBATT_PIN`) |
| Input | Resistor divider: R1=330 kΩ, R2=100 kΩ |
| Transfer function | `V_ADC = V_batt × 0.233` |
| Consumer | `eps_monitor.c` → EPS state machine |

**Voltage states (Schmidt-trigger hysteresis):**

| State | V_batt threshold |
|-------|----------------|
| NOMINAL | > 3.7 V |
| LOW | < 3.5 V |
| CRITICAL | < 3.2 V |
| EMERGENCY | < 3.0 V → FM_SAFE triggered |

---

## 11. External Watchdog Interface (TPS3431)

| Parameter | Value |
|-----------|-------|
| Device | TPS3431 (or MCP1316, MAX706) |
| Signal | WDI (watchdog input) |
| GPIO | **GPIO20** (`WATCHDOG_PIN`) |
| Kick interval | ~1 s |
| Timeout | **3 s** |
| Kick source | `HealthMonitorTask` → `watchdog_hal_feed()` |
| On timeout | Full hardware system reset |

**Recovery chain:**
```
HealthMonitorTask
      │  watchdog_hal_feed() every ~1 s
      ▼
Internal MCU watchdog (RP2350 hardware)
      │
      ▼
External TPS3431 (GPIO20, 3 s timeout)
      │  if kick missed
      ▼
Full system reset → boot → FM_BOOT → FM_SAFE
```

**Driver**: `src/drivers/watchdog/watchdog_hal.c`  
**Tests**: T-WDT-01..05, T-SAFE-01a..c

---

## 12. Debug Interface (USB CDC)

| Parameter | Value |
|-----------|-------|
| Interface | USB 1.1 Device (CDC ACM) |
| GPIO | USB D+/D− (dedicated USB pins on Pico 2W) |
| Baud rate | N/A (USB native) |
| Host port | `/dev/ttyACM0` (Linux) |
| Usage | `printf`, `LOG_*` macros, development diagnostics |
| Enabled by | `pico_enable_stdio_usb(target 1)` in `src/CMakeLists.txt` |

> UART0 (GPIO0/1) is **no longer used for debug** — exclusively reserved for GPS.

---

## 13. Software Ownership Matrix

| Interface | GPIO | Driver file | Task owner | Status |
|-----------|------|------------|-----------|--------|
| I2C0 (IMU) | GPIO4/5 | `src/drivers/imu/mpu6050.c` | `SensorReadTask` | ✅ Integrated |
| I2C0 (Mag) | GPIO4/5 | `src/drivers/mag/hmc5883l.c` | `SensorReadTask` | ✅ Integrated |
| UART0 (GPS) | GPIO0/1 | `src/drivers/gps/neo7m.c` | Navigation (planned) | 🔄 Planned |
| UART1 (TT&C) | GPIO4/5 | `src/drivers/uart/pico_usart.c` | `CommandTask`, `TelemetryTask` | ✅ Integrated |
| PWM (RW) | GPIO6/7/8 | `src/actuators/reaction_wheel.c` | `AttitudeControlTask` | 🔄 HAL pending |
| PWM (MTQ) | GPIO14/15/16 | `src/actuators/magnetorquer.c` | `AttitudeControlTask` | 🔄 HAL pending |
| ADC0 (Vbatt) | GPIO26 | `src/services/eps/eps_monitor.c` | `HealthMonitorTask` | ✅ Integrated |
| GPIO (WDI) | GPIO20 | `src/drivers/watchdog/watchdog_hal.c` | `HealthMonitorTask` | ✅ HAL integrated |
| USB CDC | — | stdio USB (Pico SDK) | All tasks (printf) | ✅ Integrated |

---

## 14. Fault Handling

Interface-level failures propagate through the FDIR chain:

```
Driver error (I2C NACK, UART timeout, ADC out-of-range)
      │
      ▼
fault_report(fault_id, FAULT_LEVEL_*)
      │
      ▼
FaultManager (32-slot table, anti-cascade)
      │  FAULT_LEVEL_CRITICAL
      ▼
FlightModeManager → fmm_force_safe()
      │
      ▼
FM_SAFE MODE (magnetorquers only, telemetry heartbeat only)
```

| Failure | Fault ID | Level | FDIR Response |
|---------|----------|-------|--------------|
| I2C bus hang (IMU/Mag) | `FAULT_IMU_UNRESPONSIVE` | WARNING | Retry 3×; CRITICAL if persists |
| EPS EMERGENCY | `FAULT_EPS_EMERGENCY` | CRITICAL | → FM_SAFE |
| Watchdog miss | — | — | Hardware reset → FM_SAFE |
| UART1 TX stall | `FAULT_COMMS_TX_FAIL` | WARNING | Log; skip telemetry cycle |

---

## 15. Interface Verification Methods

| Interface | Method | Acceptance criterion |
|-----------|--------|---------------------|
| I2C0 (MPU-6050) | Unit test + hardware read | Sensor ACK; gyro/accel in expected range at rest |
| I2C0 (HMC5883L) | Unit test + hardware read | Sensor ACK; mag vector magnitude ≈ Earth field (30–60 µT) |
| UART0 (GPS) | Loopback + live NMEA parse | `$GPGGA` / `$GPRMC` sentences parsed without error |
| UART1 (TT&C) | Loopback + GS round-trip | CSP packet transmitted and echoed by ground station |
| PWM (RW/MTQ) | Oscilloscope | Correct duty cycle and frequency for commanded torque |
| ADC0 (Vbatt) | Known voltage reference | Measured V_batt within ±2% of reference |
| GPIO20 (watchdog) | Forced timeout test | System resets within 3 s of kick cessation |
| USB CDC | `minicom` / `screen` | Log output received on host at correct rate |

**Test references:** T-WDT-01..05, T-MAG-01..04, T-EKF-01..06, T-TLM-01..06, T-HM-01..03

---

## 16. Changelog

| Version | Date | Description |
|---------|------|-------------|
| 1.0 | 2026-03-05 | Initial release — interfaces defined for BOM v1.0 hardware set |
