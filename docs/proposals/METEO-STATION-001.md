# METEO-STATION-001 — Meteorological Station Proposal

**Document ID**: PROP-001
**Version**: 1.0
**Date**: 2026-07-22
**Status**: Proposed (Future Implementation)
**Priority**: Medium
**Effort Estimate**: 40–60 h

---

## 1. Executive Summary

This document proposes a **meteorological station** for the CubeSat OBC platform,
integrating a **Bosch BME280** pressure/temperature/humidity sensor alongside the
existing **Sensirion SHT31** temperature/humidity sensor. The station provides
redundant meteorological measurements and atmospheric pressure data for altitude
estimation.

The design follows the existing payload architecture (FreeRTOS tasks, CSP command
interface, DLA data layer) and is compatible with both `FM_NOMINAL` and
`FM_PAYLOAD` flight modes.

---

## 2. Motivation

### 2.1 Scientific Value

- **Atmospheric pressure profiling** during LEO orbit provides thermospheric
  density estimates useful for drag modeling
- **Redundant temperature/humidity** measurements from BME280 + SHT31 enable
  cross-validation and fault detection
- **Altitude estimation** via barometric formula provides independent
  verification of orbital parameters

### 2.2 Platform Benefits

- Demonstrates CubeSat as a **reusable scientific platform** for future payloads
- Exercises I2C bus sharing, flash storage, and command interfaces
- Low power consumption (~3.6 µA at 1 Hz) fits within existing power budget

---

## 3. Hardware Specification

### 3.1 BME280 Sensor

| Parameter | Value |
|-----------|-------|
| **Manufacturer** | Bosch Sensortec |
| **Part Number** | BME280 |
| **Interface** | I2C (default 0x76) or SPI |
| **Supply Voltage** | 1.71 V – 3.6 V |
| **Current (1 Hz, forced)** | 3.6 µA |
| **Pressure Range** | 300 – 1100 hPa |
| **Pressure Accuracy** | ±1 hPa (0–65°C) |
| **Temperature Range** | -40°C – +85°C |
| **Temperature Accuracy** | ±0.5°C (25°C) |
| **Humidity Range** | 0 – 100% RH |
| **Humidity Accuracy** | ±3% RH (20–80%) |
| **Package** | LGA 8-pin (2.5 × 2.5 mm) |

### 3.2 SHT31 Sensor (Existing)

| Parameter | Value |
|-----------|-------|
| **Manufacturer** | Sensirion |
| **Part Number** | SHT31-DIS-B |
| **Interface** | I2C (default 0x44) |
| **Supply Voltage** | 2.4 V – 5.5 V |
| **Current (1 Hz)** | 800 µA |
| **Temperature Accuracy** | ±0.3°C (25°C) |
| **Humidity Accuracy** | ±2% RH (20–80%) |

### 3.3 Redundancy Strategy

| Measurement | BME280 | SHT31 | Cross-Validation |
|-------------|--------|-------|------------------|
| Temperature | ✅ | ✅ | Delta < 2°C → both valid |
| Humidity | ✅ | ✅ | Delta < 5% RH → both valid |
| Pressure | ✅ | — | Primary source, BME280 only |

---

## 4. Electrical Interface

### 4.1 I2C Bus Allocation

Both sensors share **I2C0** (GPIO4 SDA, GPIO5 SCL) with existing devices:

| Device | Address | Status |
|--------|---------|--------|
| MPU6050 (IMU) | 0x68 | ✅ Active |
| HMC5883L / LIS3MDL | 0x1E / 0x1C | ✅ Active |
| BH1750 (Light) | 0x23 | ✅ Active |
| **BME280** | **0x76** | **🆕 Proposed** |
| **SHT31** | **0x44** | **🆕 Proposed** |

### 4.2 Wiring

| Signal | OBC Pin | Sensor Pin | Notes |
|--------|---------|------------|-------|
| I2C_SDA | GPIO4 | SDA (both) | Shared bus, 4.7 kΩ pull-up |
| I2C_SCL | GPIO5 | SCL (both) | Shared bus, 4.7 kΩ pull-up |
| BME280_VCC | 3V3 (PAYLOAD) | VDD | Powered via payload rail |
| BME280_GND | GND | GND | Common ground |
| SHT31_VCC | 3V3 (PAYLOAD) | VDD | Powered via payload rail |
| SHT31_GND | GND | GND | Common ground |

### 4.3 Power Budget

| Sensor | Nominal Current | Peak Current | Duty Cycle | Average |
|--------|----------------|--------------|------------|---------|
| BME280 (1 Hz forced) | 3.6 µA | 715 µA (measuring) | 3% | ~25 µA |
| SHT31 (1 Hz) | 800 µA | 1.5 mA (measuring) | 5% | ~40 µA |
| **Total** | | | | **~65 µA** |

**Impact**: Negligible — <0.02% of 250 mA payload rail capacity.

---

## 5. Software Architecture

### 5.1 FreeRTOS Task Design

```
┌─────────────────────────────────────────────────────┐
│                    meteo_task                        │
│  Priority: 2  │  Stack: 1024 words  │  Period: 1 Hz │
├─────────────────────────────────────────────────────┤
│  1. Read BME280 (pressure, temperature, humidity)    │
│  2. Read SHT31 (temperature, humidity)              │
│  3. Cross-validate redundant measurements            │
│  4. Compute barometric altitude                      │
│  5. Write to DLA                                     │
│  6. Append to flash ring buffer                      │
│  7. Notify telemetry if configured                   │
└─────────────────────────────────────────────────────┘
```

### 5.2 Task Responsibilities

| Phase | Description | Timing |
|-------|-------------|--------|
| **Init** | Configure I2C, probe BME280/SHT31, load config from flash | Once at startup |
| **Read** | I2C transaction for each sensor | ~5 ms total |
| **Process** | Cross-validate, compute altitude | ~1 ms |
| **Store** | Append `meteo_record_t` to flash ring buffer | ~2 ms |
| **Update DLA** | Write latest values to `system_state_t` | ~0.1 ms |

### 5.3 Flight Mode Activation

| Flight Mode | Behavior |
|-------------|----------|
| `FM_BOOT` | Disabled — sensors not initialized |
| `FM_DETUMBLE` | Disabled — low priority during detumbling |
| `FM_NOMINAL` | **Active** — periodic sampling at configured rate |
| `FM_PAYLOAD` | **Active** — periodic sampling (dedicated science mode) |
| `FM_SAFE` | Disabled — power conservation |
| `FM_DIAGNOSTIC` | Active — for ground testing |

---

## 6. Data Structures

### 6.1 Meteo Record (Flash Storage)

```c
typedef struct __attribute__((packed)) {
    uint32_t timestamp_s;       // Seconds since epoch (GPS UTC or uptime)
    float    pressure_hpa;      // BME280 pressure [hPa]
    float    temperature_c;     // BME280 temperature [°C]
    float    humidity_pct;      // BME280 humidity [%]
    float    sht31_temp_c;      // SHT31 temperature [°C]
    float    sht31_hum_pct;     // SHT31 humidity [%]
    float    altitude_m;        // Computed barometric altitude [m]
    uint8_t  quality_flags;     // Bit 0: BME280 valid, Bit 1: SHT31 valid,
                                // Bit 2: cross-validation OK, Bit 3: saturated
    uint8_t  crc8;              // CRC-8 of bytes [0..26]
} meteo_record_t;               // Total: 28 bytes
```

### 6.2 DLA Extension

Add to `system_state_t`:

```c
// Meteorological data
float    pressure_hpa;       // Latest BME280 pressure
float    altitude_m;         // Barometric altitude
float    meteo_temp_c;       // Latest BME280 temperature
float    meteo_hum_pct;      // Latest BME280 humidity
float    sht31_temp_c;       // Latest SHT31 temperature
float    sht31_hum_pct;      // Latest SHT31 humidity
bool     pressure_valid;     // true if BME280 read OK this cycle
bool     meteo_available;    // true if meteo_task is running
```

### 6.3 Configuration Structure

```c
typedef struct {
    float    sea_level_hpa;      // Reference pressure for altitude calc
    uint16_t sampling_rate_hz;   // 0.1 – 10 Hz (default: 1)
    uint8_t  cross_val_delta_t;  // Max temp delta for cross-validation [°C]
    uint8_t  cross_val_delta_h;  // Max humidity delta for cross-validation [%]
    bool     enable_flash_store; // Enable/disable flash ring buffer
} meteo_config_t;
```

---

## 7. Flash Storage — Ring Buffer

### 7.1 Layout

| Region | Start | End | Size |
|--------|-------|-----|------|
| **Meteo Ring Buffer** | `0x7F2000` | `0x7FFFFF` | **~36 KB** |

### 7.2 Capacity

```
Record size:  28 bytes
Region size:  36,864 bytes (0x7F2000 – 0x7FFFFF = 9 × 4 KB sectors)
Capacity:     36,864 / 28 = 1,316 records

At 1 Hz:      1,316 records ≈ 22 minutes of data
At 0.1 Hz:    1,316 records ≈ 3.6 hours of data
```

### 7.3 Ring Buffer Algorithm

```
1. Read current write_index from sector header
2. If write_index >= max_records → erase sector, reset index
3. Program record at write_index * record_size
4. Increment write_index
5. Update sector header (write_index + CRC)
```

---

## 8. Command Interface

### 8.1 CSP Commands (Port 30)

| CMD_ID | Command | Payload In | Payload Out | Description |
|--------|---------|------------|-------------|-------------|
| 30 | `CMD_METEO_READ` | none | 28 bytes | Latest meteo record |
| 31 | `CMD_METEO_STATUS` | none | 8 bytes | Sensor status + config |
| 32 | `CMD_METEO_CONFIG` | config (12 bytes) | result | Update config |
| 33 | `CMD_METEO_DUMP` | count (1–100) | N × 28 bytes | Dump ring buffer records |
| 34 | `CMD_METEO_CLEAR` | none | result | Clear ring buffer |
| 35 | `CMD_METEO_CALIB` | offset (4 bytes) | result | Set pressure offset |
| 36 | `CMD_METEO_RAW` | none | 16 bytes | Raw sensor registers |

### 8.2 UART Text Commands

| Command | Description | Example Response |
|---------|-------------|-------------------|
| `METEO` | Latest readings | `METEO: P=1013.2 T=22.1 H=45.2 ALT=150.3 SHT=22.0/44.8` |
| `METEO STATUS` | Sensor status | `METEO: BME280=OK SHT31=OK BUF=128/1316 RATE=1Hz` |
| `METEO CONFIG` | Show config | `METEO: SLH=1013.25 RATE=1 DVT=2.0 DVH=5.0` |
| `METEO DUMP [n]` | Dump last N records | (formatted output) |

---

## 9. Telemetry Integration

### 9.1 Dedicated Meteo Packet (CSP Port 31)

The meteorological data uses its own CSP telemetry packet to avoid modifying
the existing 64-byte health packet:

```c
typedef struct __attribute__((packed)) {
    uint8_t  pkt_type;           // 0x10 = METEO_HK
    uint32_t timestamp_s;
    float    pressure_hpa;
    float    altitude_m;
    float    bme280_temp_c;
    float    bme280_hum_pct;
    float    sht31_temp_c;
    float    sht31_hum_pct;
    uint8_t  quality_flags;
    uint8_t  crc8;
} meteo_telemetry_t;            // 33 bytes (fits in 64-byte CSP frame)
```

---

## 10. Barometric Altitude Formula

```c
float compute_altitude_m(float pressure_hpa, float sea_level_hpa) {
    // International Standard Atmosphere formula
    return 44330.0f * (1.0f - powf(pressure_hpa / sea_level_hpa, 0.190284f));
}
```

**Note**: Accurate to ±100 m in troposphere. In LEO (above 100 km), pressure
drops below sensor range — the altitude field becomes invalid and `pressure_valid`
is set to `false`.

---

## 11. Fault Detection

| Fault ID | Description | Trigger | Response |
|----------|-------------|---------|----------|
| `FAULT_METEO_BME280_FAIL` | BME280 I2C timeout | 3 consecutive read failures | Log event, continue with SHT31 only |
| `FAULT_METEO_SHT31_FAIL` | SHT31 I2C timeout | 3 consecutive read failures | Log event, continue with BME280 only |
| `FAULT_METEO_CROSS_VAL` | Cross-validation failure | |ΔT| > threshold OR |ΔH| > threshold | Log warning, flag data as uncertain |
| `FAULT_METEO_FLASH_FULL` | Ring buffer full | Write index at max | Continue without flash storage |
| `FAULT_METEO_PRESSURE_SAT` | Pressure out of range | P < 300 hPa OR P > 1100 hPa | Flag `pressure_valid = false` |

---

## 12. Implementation Phases

### Phase 1: Driver Layer (8 h)

| Task | Description | Effort |
|------|-------------|--------|
| T-M1.1 | `bme280_driver.c` — I2C init, read, compensation | 4 h |
| T-M1.2 | `sht31_driver.c` — I2C init, read, CRC | 3 h |
| T-M1.3 | Unit tests for both drivers (mock I2C) | 1 h |

### Phase 2: Task Integration (12 h)

| Task | Description | Effort |
|------|-------------|--------|
| T-M2.1 | `meteo_task.c` — FreeRTOS task, 1 Hz loop | 3 h |
| T-M2.2 | DLA extension — `system_state_t` fields | 2 h |
| T-M2.3 | Flash ring buffer implementation | 4 h |
| T-M2.4 | Cross-validation logic | 1 h |
| T-M2.5 | Configuration storage/retrieval | 2 h |

### Phase 3: Command Interface (8 h)

| Task | Description | Effort |
|------|-------------|--------|
| T-M3.1 | CSP commands (port 30) | 4 h |
| T-M3.2 | UART text commands | 2 h |
| T-M3.3 | Command unit tests | 2 h |

### Phase 4: Telemetry & Integration (8 h)

| Task | Description | Effort |
|------|-------------|--------|
| T-M4.1 | Meteo telemetry packet (CSP port 31) | 3 h |
| T-M4.2 | Integration tests | 3 h |
| T-M4.3 | Hardware validation on Pico 2W | 2 h |

### Phase 5: Documentation (4 h)

| Task | Description | Effort |
|------|-------------|--------|
| T-M5.1 | Update ICD-PAYLOAD-001 with meteo subsystem | 2 h |
| T-M5.2 | Update RTM-OBC-001 with meteo requirements | 1 h |
| T-M5.3 | Update PENDING_TASKS.md | 1 h |

**Total estimated effort**: 40 h

---

## 13. New Files

```
src/
  drivers/
    sensors/
      bme280_driver.c          ← I2C BME280 driver
      sht31_driver.c           ← I2C SHT31 driver
  tasks/
    meteo_task.c               ← FreeRTOS meteorological task
  services/
    meteo/
      meteo_manager.c          ← Ring buffer, config, cross-validation

include/
  bme280_driver.h
  sht31_driver.h
  meteo_task.h
  meteo_manager.h

tests/unit/
  test_bme280.c                ← T-MEO-BME-01..04
  test_sht31.c                 ← T-MEO-SHT-01..03
  test_meteo_task.c            ← T-MEO-TSK-01..06
  test_meteo_manager.c         ← T-MEO-MGR-01..04
```

---

## 14. Modified Files

| File | Change |
|------|--------|
| `include/system_state.h` | Add meteo fields to `system_state_t` |
| `include/data_layer.h` | Add `data_layer_get/set_meteo()` |
| `src/core/data_layer.c` | Implement meteo DLA accessors |
| `include/command_task.h` | Add meteo command IDs (30–36) |
| `src/tasks/command_task.c` | Add meteo command handlers |
| `src/tasks/obc_main.c` | Create `MeteoTask` at startup |
| `include/flash_layout.h` | Add meteo ring buffer region |
| `src/tasks/CMakeLists.txt` | Add `meteo_task.c` |
| `src/drivers/CMakeLists.txt` | Add `sensors/` directory |
| `tests/unit/CMakeLists.txt` | Add meteo test targets |
| `config/pico_pins.h` | Document I2C0 address allocation |

---

## 15. Risk Assessment

| Risk | Probability | Impact | Mitigation |
|------|-------------|--------|------------|
| I2C bus contention (7 devices) | Medium | Medium | Mutex already implemented; add timeout handling |
| BME280 I2C address conflict | Low | High | Address 0x76 configurable via SDO pin |
| Flash ring buffer wear | Low | Low | 9 sectors × 4 KB = 1,316 cycles per sector; sufficient for mission life |
| Altitude formula invalid in LEO | High | Low | Detect pressure < 300 hPa, set `pressure_valid = false` |
| Cross-validation noise | Medium | Low | Configurable thresholds; default ±2°C / ±5% RH |

---

## 16. Dependencies

| Dependency | Status | Notes |
|------------|--------|-------|
| I2C0 bus mutex | ✅ Complete | `i2c0_mutex` in `i2c_driver.c` |
| DLA `system_state_t` | ✅ Complete | Easy to extend with new fields |
| CSP command framework | ✅ Complete | Port 20 (existing), port 30 (meteo) |
| Flash ring buffer pattern | ✅ Complete | Pattern from `boot_log.c` |
| FreeRTOS task creation | ✅ Complete | Pattern from `payload_task.c` |

---

## 17. Acceptance Criteria

- [ ] BME280 reads pressure within ±1 hPa of reference
- [ ] SHT31 reads temperature within ±0.3°C of reference
- [ ] Cross-validation detects disagreements > 2°C
- [ ] Barometric altitude computed within ±100 m (sea-level reference)
- [ ] Flash ring buffer stores 1,316+ records without corruption
- [ ] CSP command `CMD_METEO_READ` returns latest record
- [ ] UART `METEO` command shows formatted readings
- [ ] All unit tests pass (target: 20+ tests)
- [ ] CI pipeline green (6/6 stages)
- [ ] Meteo task runs at 1 Hz without timing violations

---

## 18. Future Enhancements

| Enhancement | Description | Prerequisite |
|-------------|-------------|--------------|
| **Dynamic sampling rate** | Adjust rate based on orbital position | GPS fix available |
| **On-orbit calibration** | Auto-adjust sea_level_hpa from known altitude | GPS altitude reference |
| **Solar irradiance** | Add BH1750 light sensor to meteo packet | BH1750 already on I2C0 |
| **Ground station GUI** | Real-time meteo display with plots | Telemetry downlink |
| **Machine learning** | Predict atmospheric density from pressure profile | Historical data collection |

---

*End of Document*
