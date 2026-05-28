# ICD-OBC-001 — Interface Control Document
## CubeSat OBC Hardware/Software Interface

**Document ID**: ICD-OBC-001  
**Version**: 1.2  
**Date**: 2026-03-10  
**Status**: Released — Phase 7 amendment (GPS re-scoped)  
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
| I2C0 | MPU-6050 IMU, HMC5883L Magnetometer, **RM3100 Scientific Magnetometer (Phase 7)** |
| UART0 | GPS NEO-7M GY-NEO6Mv2 — Phase 7 (re-scoped, FR-18/19) |
| UART1 | TT&C Radio E22-400M30S / HC-12 |
| PWM | Reaction Wheels (RW1–3), Magnetorquers (MTQ X/Y/Z) |
| ADC | Battery voltage, temperature, **RAD-001 radiation sensor (Phase 7)** |
| SPI1 | **CAM-001 camera (IMX219 SPI bridge) (Phase 7)** |
| GPIO | External watchdog TPS3431, status LED, **PAYLOAD_ENABLE, CAM_TRIGGER, RAD_RESET (Phase 7)** |
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
| ADC channels | 3 (GPIO26–28) + 1 internal temp |

> **⚠️ Pico 2W GPIO constraint**: Solo tiene GPIO0-22 y GPIO26-28 (no GPIO23-25, 29 como RP2350).

---

## 3. Bus Topology (Pico 2W)

```
                    ┌──────────────────┐
                    │       OBC        │
                    │     RP2350       │
                    └────────┬─────────┘
                             │
                             │ [EKF ekf_predict()]
                             │ [EKF ekf_update()]
                             │ [EKF ekf_update_mag()]
                             │
                             │ x = [q₀, q₁, q₂, q₃,
                             │      bx,  by,   bz]
       ┌─────────────────────┼──────────────────────┐
       │                     │                      │
     I2C0                  UART0                 UART1
   GPIO4/5               GPIO0/1               GPIO8/9
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
  GPIO6/7/10           GPIO26 (ADC0)           GPIO20
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

> **Flight note**: For increased EMI margin and longer cable runs, the flight configuration
> may reduce I2C0 speed to 100 kHz (`I2C0_SPEED_HZ = 100000`). All sensors (MPU-6050,
> QMC5883L/SHT31/BH1750/DS3231) are compatible with both 100 kHz and 400 kHz.

**Connected devices (I2C0 @ GPIO4/5):**

| Device | I2C Address | Driver | Status |
|--------|-------------|--------|--------|
| MPU-6050/6500 IMU | **`0x69`** (AD0=VCC) | `src/drivers/imu/mpu6050.c` | ✅ v0.25.0 |
| QMC5883L Magnetometer | **`0x2C`** (AD0=VCC) | `src/drivers/mag/hmc5883l.c` | ✅ (clone detected) |
| SHT31 (Temp/Humidity) | **`0x44`** | `src/drivers/sht31.c` | ✅ v0.28.0 |
| BH1750 (Lux) | **`0x23`** | `src/drivers/bh1750.c` | ✅ v0.26.0 |
| DS3231 (RTC) | **`0x68`** | `src/drivers/ds3231.c` | ✅ v0.26.0 |
| INA219 (Power Monitor) | **`0x40`** | `src/drivers/ina219.c` | ✅ v0.28.0 |
| INA219 (Solar Panel) | **`0x41`** (A0=GND, A1=VS) | `src/drivers/ina219.c` | ✅ v0.28.0 |
| RM3100 Scientific MAG | **`0x20`** (SA0=SA1=0) | `src/drivers/payload/rm3100.c` | ⏳ Phase 7 |

> **No pin conflict**: I2C0 uses GPIO4/5; UART1 (TT&C) uses GPIO8/9. Both buses can
> be active simultaneously during flight operations.

---

## 5. IMU Interface (MPU-6050/6500)

| Parameter | Value |
|-----------|-------|
| Sensor | MPU-6050 or MPU-6500 |
| Bus | I2C0 |
| Address | **`0x69`** (AD0=VCC on MPU-6050; MPU-6500 default) |
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
| **Configuration Status** | **EM LOCKED** — HMC5883L (GY-271), I2C `0x1E`. CDR candidate: LIS3MDL I2C `0x1C` (SA0=GND). Part selection per ACT-11 (SRR-OBC-001 2026-03-10). |
| Sensor | HMC5883L (GY-271 module) — Engineering Model |
| Bus | I2C0 |
| Address | `0x1E` |
| GPIO | GPIO4 (SDA), GPIO5 (SCL) |
| Driver | `src/drivers/mag/hmc5883l.c` [EM]; `src/drivers/mag/lis3mdl.c` [CDR — not yet created] |
| Output rate | 75 Hz (configured in driver) |
| ⚠️ Clone risk | QMC5883L (I2C `0x0D`, different register map) found in some GY-271 modules. Verify IC markings. |

**Output data:**

| Signal | Units | Consumer |
|--------|-------|---------|
| `mag_x/y/z` | µT | `SensorReadTask` → `ekf_update_mag()` → 3D attitude fusion |
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
    mag_interface.h      ← common API (EKF and momentum_dump depend only on this)
    mag_hmc5883l.c       ← lab implementation
    mag_lis3mdl.c        ← flight implementation (CDR)
```

```c
/* mag_interface.h — sensor-agnostic magnetometer API */
typedef struct {
    float x;   /* [µT] */
    float y;   /* [µT] */
    float z;   /* [µT] */
} mag_data_t;

bool mag_init(void);
bool mag_read(mag_data_t *data);
```

The EKF (`ekf_update_mag()`) and `momentum_dump()` use `mag_data_t` exclusively.
Switching sensor requires only selecting the implementation at compile time — no
changes to any control or estimation code.

---

## 7. GPS Interface (NEO-7M)

> **✅ COMPLETED — Phase 7**  
> AIR-OBC-001 ACT-06 resolved via **Option A** (2026-03-10).  
> Requirements **FR-18** (NMEA parse ≥ 1 Hz), **FR-19** (UTC sync ± 500 ms), and **IR-10** (UART0 interface)  
> implemented in driver `src/drivers/gps/neo7m.c` and `GpsTask`.  
> **Hardware verified: patch antenna receiving satellite signals** (2026-05-05)

| Parameter | Value |
|-----------|-------|
| Module | GY-NEO6Mv2 with NEO-7M |
| Bus | UART0 (`uart0`) |
| TX pin | **GPIO0** (`UART0_TX_PIN`) |
| RX pin | **GPIO1** (`UART0_RX_PIN`) |
| Baud rate | 9600 bps (reconfigurable to 38400 via UBX `CFG-PRT`) |
| Protocol | NMEA 0183 — `$GPGGA` (position + altitude + UTC), `$GPRMC` (position + speed + date) |
| Driver | `src/drivers/gps/neo7m.c` — **Phase 7 Complete** |
| Status | **Active** |

> **Future note**: Increasing to 38400 bps reduces NMEA message latency and enables
> higher fix update rates. Requires reconfiguring the NEO-7M via UBX protocol command
> (`CFG-PRT`) before switching. Lab default is 9600 bps.

**Output data:**

| NMEA sentence | Data | Consumer |
|--------------|------|---------|
| `$GPGGA` | Position (lat/lon/alt), fix quality, UTC, satellites, HDOP | Navigation task, Telemetry |
| `$GPRMC` | Position, speed, course, UTC date | Navigation task |

> **Debug note**: UART0 was previously used for ASCII debug output. Debug is now
> fully routed to **USB CDC** (`pico_enable_stdio_usb = 1` in `src/CMakeLists.txt`).
> UART0 is exclusively reserved for GPS.

**Implemented features:**
- ✅ NMEA parser driver `src/drivers/gps/neo7m.c`
- ✅ FreeRTOS GPS task `src/tasks/gps_task.c` (1 Hz, parse + write to DLA)
- ✅ Data Layer: `GpsFix_t` struct, `data_layer_set_gps_fix()`, `data_layer_get_gps_fix()`
- ✅ UTC clock sync: `rtc_set_datetime()` on valid `$GPRMC` fix (within ± 500 ms)
- ✅ HDOP parsing added to GpsFix_t
- ✅ GPS stats (GpsStats_t) with counters
- ✅ 11 CSP commands implemented (CMD_GPS_STATUS, CMD_STATUS, CMD_FAULT_LIST, etc.)
- ✅ UART text commands (GPS, STATUS, FAULTS, RESETGPS)

---

## 8. TT&C Radio Interface (E22-400M30S / HC-12)

| Parameter | Value |
|-----------|-------|
| Flight module | EBYTE E22-400M30S (SX1268, 433 MHz LoRa, 30 dBm) |
| Lab module | HC-12 (Si4463, 433 MHz FSK, 20 dBm) |
| Bus | UART1 (`uart1`) |
| TX pin | **GPIO8** (`UART1_TX_PIN`) |
| RX pin | **GPIO9** (`UART1_RX_PIN`) |
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

### 9.1 Reaction Wheels (TB6612FNG) — Pico 2W Compatible

| Parameter | Value |
|-----------|-------|
| Driver IC | TB6612FNG H-bridge |
| Interface | PWM + direction GPIO |
| Driver | `src/actuators/reaction_wheel.c` |

| Axis | PWM GPIO | PWM Slice/Ch | Pico 2W Pin | Notes |
|------|---------|-------------|-------------|-------|
| RW1 | GPIO10 | PWM5A | 14 | |
| RW2 | GPIO11 | PWM5B | 15 | |
| RW3 | GPIO12 | PWM6A | 16 | |

### 9.2 Magnetorquers (DRV8833) — Pico 2W Compatible

| Parameter | Value |
|-----------|-------|
| Driver IC | DRV8833 H-bridge |
| Interface | Bidirectional PWM |
| Driver | `src/actuators/magnetorquer.c` |

| Axis | PWM GPIO | PWM Slice/Ch | Pico 2W Pin | Notes |
|------|---------|-------------|-------------|-------|-------|
| MTQ X | GPIO14 | PWM7A | 31 | Updated per pico_pins.h v1.1 |
| MTQ Y | GPIO15 | PWM7B | 32 | |
| MTQ Z | GPIO16 | PWM0A | 33 | |

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

## 11. Sun Sensor Interface (ADC1/ADC2)

> ✅ **IMPLEMENTED — feat/sun-sensor-driver** (2026-04-24)

| Parameter | Value |
|-----------|-------|
| Sensor | Dual-axis photodiode (BPW21 or equivalent) |
| ADC peripheral | ADC1, ADC2 |
| GPIO X | **GPIO27** (`SUN_SENSOR_X_PIN`) |
| GPIO Y | **GPIO28** (`SUN_SENSOR_Y_PIN`) |
| Pull-down | 10 kΩ to GND (required for voltage divider) |
| Driver | `src/drivers/sun_sensor.c` |
| Output rate | Configurable (100 Hz in current implementation) |

**Electrical interface:**

```
Photodiode (reverse-biased or photovoltaic)
     │
     ├────> GPIO27 (ADC1) ───[10kΩ]──► GND   → Sun sensor X
     │
     └────> GPIO28 (ADC2) ───[10kΩ]──► GND   → Sun sensor Y
```

> **Wiring note**: The photodiode connects in series with a 10kΩ pull-down resistor
> to form a voltage divider. The ADC reads the voltage drop across the photodiode,
> which varies with light intensity.

**Output data:**

| Signal | Units | Range | Consumer |
|--------|-------|-------|----------|
| `adc_x` | raw | 0-4095 | `SensorReadTask` → attitude estimation |
| `adc_y` | raw | 0-4095 | `SensorReadTask` → attitude estimation |
| `intensity_x` | normalized | 0.0-1.0 | Telemetry |
| `intensity_y` | normalized | 0.0-1.0 | Telemetry |
| `sun_detected_x` | boolean | — | ADCS mode logic |
| `sun_detected_y` | boolean | — | ADCS mode logic |

**Intensity calculation:**
```
intensity = adc_value / 4095.0  // Normalized to 0.0-1.0
```

**Typical values:**

| Condition | ADC Value | Intensity |
|-----------|-----------|-----------|
| Direct sun | ~3700-3800 | ~0.90-0.91 |
| Ambient light | ~1700-2100 | ~0.43-0.53 |
| Covered/dark | ~10-50 | ~0.00-0.01 |

**API:**
```c
bool sun_sensor_init(void);
bool sun_sensor_read(sun_sensor_data_t *data);
bool sun_sensor_is_sun_visible(uint16_t threshold);
```

**Driver files:**
- Header: `include/sun_sensor.h`
- Implementation: `src/drivers/sun_sensor.c`
- Stub (tests): `src/drivers/sun_sensor_stub.c`

---

## 12. External Watchdog Interface (TPS3431)

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
TPS3431 (GPIO20, 3 s timeout)
      │  if kick missed (firmware hang)
      ▼
RESET line → RP2350 reboot
      │
      ▼
Boot sequence → FM_BOOT → FM_SAFE
```

**Driver**: `src/drivers/watchdog/watchdog_hal.c`  
**Tests**: T-WDT-01..05, T-SAFE-01a..c

---

## 13. Debug Interface (USB CDC)

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

## 14. Software Ownership Matrix — Pico 2W Compatible

| Interface | GPIO | Driver file | Task owner | Status |
|-----------|------|------------|-----------|--------|
| I2C0 (IMU) | GPIO4/5 | `src/drivers/imu/mpu6050.c` | `SensorReadTask` | ✅ Integrated |
| I2C0 (Mag) | GPIO4/5 | `src/drivers/mag/hmc5883l.c` | `SensorReadTask` | ✅ Integrated |
| I2C0 (SHT31/BH1750/DS3231/INA219) | GPIO4/5 | Various drivers | `SensorReadTask` | ✅ Integrated |
| UART0 (GPS) | GPIO0/1 | `src/drivers/gps/neo7m.c` | Navigation | ✅ Integrated |
| UART1 (TT&C) | GPIO8/9 | `src/drivers/uart/pico_usart.c` | `CommandTask`, `TelemetryTask` | ✅ Integrated |
| PWM (RW 1-3) | GPIO10/11/12 | `src/actuators/reaction_wheel.c` | `AttitudeControlTask` | 🔄 HAL pending |
| PWM (MTQ X/Y/Z) | GPIO14/15/16 | `src/actuators/magnetorquer.c` | `AttitudeControlTask` | 🔄 HAL pending |
| ADC0 (Vbatt) | GPIO26 | `src/services/eps/eps_monitor.c` | `HealthMonitorTask` | ✅ Integrated |
| ADC1 (Sun Sensor X) | GPIO27 | `src/drivers/sun_sensor.c` | `SensorReadTask` | ✅ Integrated |
| ADC2 (Sun Sensor Y) | GPIO28 | `src/drivers/sun_sensor.c` | `SensorReadTask` | ✅ Integrated |
| SPI0 (Flash) | GPIO7/16/18/19 | TBD | `PayloadTask` | 🔄 Phase 7 |
| GPIO20 (WDI) | GPIO20 | `src/drivers/watchdog/watchdog_hal.c` | `HealthMonitorTask` | ✅ HAL integrated |
| USB CDC | — | stdio USB (Pico SDK) | All tasks (printf) | ✅ Integrated |

---

## 15. Fault Handling

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
| SPI1 camera timeout (Phase 7) | `FAULT_PAYLOAD_CAM_FAIL` | WARNING | Retry 1×; log; disable CAM for session |
| ADC1 out-of-range (Phase 7) | `FAULT_PAYLOAD_ADC_OVERRANGE` | WARNING | Log; skip sample |
| Payload storage full (Phase 7) | `FAULT_PAYLOAD_STORAGE_FULL` | ERROR | Disable new captures; ground notification |

---

## 16. Payload Suite Interface (PLS-001) — Phase 7

Defined in PAYLOAD-SPEC-001. Summary of hardware interfaces added in Phase 7.

### 15.1 SPI1 — Camera Interface (CAM-001)

| Parameter     | Value                                  |
|---------------|----------------------------------------|
| Peripheral    | SPI1 (`spi1`)                          |
| SCK           | **GPIO10** — ⚠️ reassign RW3 to GPIO3  |
| MOSI          | **GPIO11**                             |
| MISO          | **GPIO12**                             |
| CSn (camera)  | **GPIO13**                             |
| Max clock     | 10 MHz                                 |
| Module        | Arducam IMX219 SPI bridge (or equivalent) |
| Driver        | `src/drivers/payload/camera_driver.c`  |

> **GPIO10 conflict**: Current ICD §9.1 assigns GPIO10 to RW3 PWM (PWM5A).
> Phase 7 PCB shall relocate RW3 to **GPIO3** (PWM1B — available after GPS
> descope of UART0). No software change required beyond updating `pico_pins.h`
> and the RW3 PWM slice/channel binding.

### 15.2 I2C0 Addition — Scientific Magnetometer (MAG-001)

| Parameter  | Value                              |
|------------|------------------------------------|
| Bus        | I2C0 (existing GPIO4/5)            |
| Address    | `0x20` (SA0=SA1=0)                 |
| Sensor     | PNI RM3100                         |
| Driver     | `src/drivers/payload/rm3100.c`     |
| Rate       | 10 Hz (CMM mode, Phase 7)          |

> No new PCB traces needed. RM3100 shares the existing I2C0 bus.
> I2C0 bus load with 3 devices at 400 kHz is within spec (capacitance < 200 pF).

### 15.3 ADC1 — Radiation Detector (RAD-001)

> ⚠️ **GPIO27 shared**: ADC1 (GPIO27) is shared between RAD-001 and sun sensor.
> Radiation driver uses GPIO2 for RAD_RESET, freeing GPIO27 for sun sensor primary use.

| Parameter  | Value                                      |
|------------|--------------------------------------------|
| ADC channel| ADC1 (shared with sun sensor)              |
| GPIO       | **GPIO27** (sun sensor X)                  |
| Input      | TIA output, 0–3.3 V                        |
| Driver     | `src/drivers/payload/radiation_driver.c`   |
| Rate       | 1 Hz (Phase 7)                             |

### 15.4 Payload Control GPIOs

| Signal         | GPIO   | Direction | Active | Software owner |
|----------------|--------|-----------|--------|----------------|
| PAYLOAD_ENABLE | GPIO21 | OUT       | High   | `payload_manager.c` |
| CAM_TRIGGER    | GPIO22 | OUT       | High   | `camera_driver.c`   |
| RAD_RESET      | GPIO2  | OUT       | High   | `radiation_driver.c` — GPIO2 free (UART0 descoped) |

> **GPIO note**: MTQ on GPIO14/15/16, PAYLOAD_ENABLE on GPIO21, CAM_TRIGGER on
> GPIO22 — no conflict (MTQ was moved from GPIO17/21/22 to avoid sharing).

### 15.5 Interface Verification Methods (Payload)

| Interface | Method | Acceptance criterion |
|-----------|--------|---------------------|
| SPI1 (CAM-001) | SPI loopback + image capture | JPEG size 50–500 KB, no SPI errors |
| I2C0 (RM3100) | Unit test + hardware read | ACK at 0x20; mag vector 30–60 µT |
| ADC1 (RAD-001) | Known voltage reference + field test | ADC reading within ±1% of reference |
---

## 17. Interface Verification Methods

| Interface | Method | Acceptance criterion |
|-----------|--------|---------------------|
| I2C0 (MPU-6050) | Unit test + hardware read | Sensor ACK; gyro/accel in expected range at rest |
| I2C0 (HMC5883L) | Unit test + hardware read | Sensor ACK; mag vector magnitude ≈ Earth field (30–60 µT) |
| UART0 (GPS) | Loopback + live NMEA parse | `$GPGGA` / `$GPRMC` sentences parsed without error |
| UART1 (TT&C) | Loopback + GS round-trip | CSP packet transmitted and echoed by ground station |
| PWM (RW/MTQ) | Oscilloscope | Correct duty cycle and frequency for commanded torque |
| ADC0 (Vbatt) | Known voltage reference | Measured V_batt within ±2% of reference |
| ADC1/2 (Sun Sensor) | Flatsat test with flashlight | ADC value changes with light intensity (10-50 dark, ~3700 bright) |
| GPIO20 (watchdog) | Forced timeout test | System resets within 3 s of kick cessation |
| USB CDC | `minicom` / `screen` | Log output received on host at correct rate |

**Test references:** T-WDT-01..05, T-MAG-01..04, T-EKF-01..06, T-TLM-01..06, T-HM-01..03

---

## 18. Changelog

| Version | Date | Description |
|---------|------|-------------|
| 1.0 | 2026-03-05 | Initial release — interfaces defined for BOM v1.0 hardware set |
| 1.1 | 2026-04-06 | GPS Phase 1 & 2 commands implemented (11 CSP + 8 UART text), HDOP, GpsStats_t |
| 1.3 | 2026-04-24 | Sun sensor driver added — dual-axis photodiode on GPIO27/28 (ADC1/ADC2), API in sun_sensor.h, driver sun_sensor.c |

---

**Document version: 1.3**
