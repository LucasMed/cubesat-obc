# PAYLOAD-SPEC-001 — Scientific Payload Specification

| Field           | Value                                        |
|-----------------|----------------------------------------------|
| Document ID     | PAYLOAD-SPEC-001                             |
| Version         | 0.1                                          |
| Date            | 2026-03-10                                   |
| Author          | CubeSat OBC Team                             |
| Status          | Draft — Phase 7 Baseline                     |
| Classification  | Internal                                     |
| Reviewed by     | —                                            |
| Approved by     | —                                            |

**Change History**

| Version | Date       | Author           | Description                                        |
|---------|------------|------------------|----------------------------------------------------|
| 0.1     | 2026-03-10 | CubeSat OBC Team | Initial draft — Phase 7 payload baseline           |

---

## Table of Contents

1. [Introduction](#1-introduction)
2. [Applicable Documents](#2-applicable-documents)
3. [Acronyms and Definitions](#3-acronyms-and-definitions)
4. [Mission Payload Overview](#4-mission-payload-overview)
5. [Camera Instrument (CAM-001)](#5-camera-instrument-cam-001)
6. [Scientific Magnetometer (MAG-001)](#6-scientific-magnetometer-mag-001)
7. [Radiation Detector (RAD-001)](#7-radiation-detector-rad-001)
8. [Electrical and Data Interfaces](#8-electrical-and-data-interfaces)
9. [Power Budget](#9-power-budget)
10. [Data Budget](#10-data-budget)
11. [Payload Operational Mode (FM_PAYLOAD)](#11-payload-operational-mode-fm_payload)
12. [Software Architecture](#12-software-architecture)
13. [Verification](#13-verification)
14. [Open Items](#14-open-items)
15. [References](#15-references)

---

## 1. Introduction

### 1.1 Purpose

This document defines the scientific payload suite for the CubeSat OBC mission.
It specifies the instruments selected, their electrical and data interfaces, the
power and data budgets, and the software architecture required to integrate the
payload into the existing flight software stack.

### 1.2 Scope

The payload suite **PLS-001** consists of three instruments:

```
PLS-001  Payload Suite
├── CAM-001  Earth Observation Camera   (Sony IMX219)
├── MAG-001  Scientific Magnetometer    (PNI RM3100)
└── RAD-001  Radiation / Particle Detector (PIN diode + ADC)
```

All three instruments are commercially available at low cost, widely used in
academic CubeSat missions, and interfaced via protocols already present on the
RP2350 platform (I2C, SPI, ADC). Integration is designed to be compatible with
the existing OBC bus without PCB redesign to the core OBC processor board.

### 1.3 Scientific Objectives

| Instrument | Scientific Objective                              |
|------------|---------------------------------------------------|
| CAM-001    | Earth observation: surface imaging in LEO         |
| MAG-001    | Field characterization: mapping terrestrial geomagnetic field |
| RAD-001    | Space environment: monitoring particle radiation dose in LEO |

Together these three instruments support studies in:
- Geomagnetism and magnetic field mapping
- Space weather and orbital radiation environment
- Earth observation for academic publications

### 1.4 Design Constraints

- Total payload power ≤ 500 mW average during FM_PAYLOAD operation
- Total data volume ≤ 1 MB per orbit (fits in a single downlink pass)
- All interfaces use existing RP2350 peripheral buses
- No modification to the core OBC PCB or power supply topology
- Payload operates only in `FM_PAYLOAD` mode; disabled in all other modes
- Payload power controlled via the existing 5V PAYLOAD rail (POWER-BDG-001 §11)

---

## 2. Applicable Documents

| ID              | Title                                        | Version |
|-----------------|----------------------------------------------|---------|
| MRD-OBC-001     | Mission Requirements Document                | 1.1     |
| SRS-OBC-001     | Software Requirements Specification          | 2.2     |
| ICD-OBC-001     | Interface Control Document                   | 1.1     |
| FMM-DES-001     | Flight Mode Manager Design Document          | 0.4     |
| POWER-BDG-001   | Power Budget                                 | 0.3     |
| OBC-DES-001     | OBC Hardware & CDH Design Document           | 0.1     |
| SAD-OBC-001     | System Architecture Document                 | 1.0     |

---

## 3. Acronyms and Definitions

| Term   | Definition |
|--------|-----------|
| CAM    | Camera instrument |
| MAG    | Magnetometer instrument (scientific payload, distinct from ADCS magnetometer) |
| RAD    | Radiation detector instrument |
| PLS    | Payload Suite |
| LEO    | Low Earth Orbit |
| SPI    | Serial Peripheral Interface |
| ADC    | Analogue-to-Digital Converter |
| LVDS   | Low-Voltage Differential Signalling |
| CSI    | Camera Serial Interface |
| FOV    | Field-of-View |
| DoD    | Depth of Discharge |
| FM     | Flight Mode |
| FMM    | Flight Mode Manager |
| FDIR   | Fault Detection, Isolation and Recovery |
| HK     | Housekeeping |
| TBD    | To Be Determined |

---

## 4. Mission Payload Overview

### 4.1 Payload Suite Architecture

The payload suite PLS-001 is a self-contained scientific instrument package
mounted on the CubeSat's nadir-pointing face. All instruments share the 5V
payload power rail and communicate with the OBC via standard digital interfaces.

```
OBC (RP2350) — Pico 2W (8-pin Arducam Mini, SPI+I2C only)
│
├─ SPI0 ──────────────────┬──────────────────┐
│  ├─ GPIO14 (CS)         │                  │
│  ├─ GPIO17 (MISO)    CAM-001 (OV2640)  MAG-001 (RM3100)*
│  ├─ GPIO18 (SCK)         │                  │
│  └─ GPIO19 (MOSI)       │                  │
│
├─ I2C1 ───── CAM-001 registers (SCCB)
│  ├─ GPIO2 (SDA)
│  └─ GPIO3 (SCL)
│
├─ GPIO21 (PAYLOAD_ENABLE) ──── 5 V rail switch
```
*RM3100 default interface: I2C0 (GPIO4/5, address 0x20). SPI0 CS @ GPIO6 optional.

### 4.2 Instrument Summary

| Instrument | Model                   | Interface | Power (avg) | Data rate |
|------------|-------------------------|-----------|-------------|-----------|
| CAM-001    | OV2640 Arducam Mini 2MP | SPI0      | 200 mW      | ~200 KB/capture (on command) |
| MAG-001    | PNI RM3100              | I2C0      | 30 mW       | ~1.2 kB/s (10 Hz log) |
| RAD-001    | PIN diode               | ADC1      | 80 mW       | ~0.1 kB/s (1 Hz log)  |
| **Total**  |                         |           | **310 mW**  | **< 1 MB/orbit (nominal)** |

### 4.3 Operational Concept

```
Ground uplink: ENABLE_PAYLOAD command
      │
      ▼
OBC: fmm_request_transition(FM_PAYLOAD)
      │
      ▼
payload_task: enable 5V rail, initialize instruments
      │
      ├─ periodic: log MAG-001 @ 10 Hz → flash
      ├─ periodic: log RAD-001 @ 1 Hz  → flash
      └─ on command: trigger CAM-001 capture → store JPEG → flash
      │
      ▼
Ground contact: downlink stored payload data via TT&C (CSP)
```

---

## 5. Camera Instrument (CAM-001)

### 5.1 Instrument Identification

| Parameter            | Value                                        |
|----------------------|----------------------------------------------|
| Instrument ID        | CAM-001                                      |
| Sensor               | OV2640 Arducam Mini (2 MP)                   |
| Representative module| Arducam Mini 2MP SPI camera (M12/CS mount)   |
| Scientific purpose   | Earth surface observation (LEO) — lateral mount |
| Cost estimate        | 15 – 25 USD                                  |

### 5.2 Technical Characteristics

| Parameter          | Value                     | Notes |
|--------------------|---------------------------|-------|
| Resolution         | 2 MP (1600 × 1200)        | UXGA full sensor |
| Pixel size         | 2.2 µm × 2.2 µm           | |
| Optical format     | 1/4"                      | |
| Shutter            | Rolling electronic shutter | |
| Frame rate (UXGA)  | 15 fps                    | SVGA (800×600): 30+ fps |
| Spectral range     | 400 – 700 nm (visible)    | CFA Bayer array |
| Dynamic range      | ~60 dB                    | |
| Operating voltage  | 3.3 V (on-module regulator from 5V rail) | |
| Interface          | **SPI (native)**          | No CSI bridge required |
| Quiescent current  | < 3 mA                    | Sleep mode |
| Active current     | ~40 – 60 mA @ 3.3 V      | |
| Active power       | **~ 200 mW**              | |

> **Interface note**: The OV2640 Arducam Mini provides native SPI output at
> up to 10 MHz, eliminating the need for a CSI-2 bridge. The module integrates
> a FIFO buffer (AL422B) that captures full frames at sensor speed and provides
> a standard SPI read interface to the MCU. This simplifies the wiring,
> reduces power consumption, and lowers cost compared to the IMX219 CSI bridge
> approach evaluated in earlier designs.

### 5.3 Output Data Characteristics

| Format      | Typical size per capture | Notes |
|-------------|--------------------------|-------|
| JPEG (Q=70) | 100 – 250 KB             | 2 MP UXGA resolution |
| JPEG (Q=50) | 60 – 150 KB              | Adequate for ground verification |
| RAW (UXGA)  | ~1.9 MB                  | Not recommended — storage and downlink limited |

**Recommended configuration for Phase 7**: JPEG Q=70, UXGA (1600×1200) → expected
~150 KB per image. Configurable via `config.h`.

### 5.4 Mechanical Mounting — Lateral Earth Observation

The OV2640 camera is mounted on **side panel 0** (radial +X direction) for lateral
Earth observation. Unlike a typical nadir-pointing top-plate mount, the lateral
configuration:

- Views the Earth limb when the satellite is nadir-pointing (+Z toward Earth)
- Does not require a top-plate aperture or dedicated camera shelf
- Places the camera behind a **18×18 mm window cutout** with transparent acrylic cover
- Positions the camera PCB at approximately **z = 50 mm** (mid-height), aligned with the side panel window

**Mounting Assembly**:
```
Side panel (outer)          Window (acrylic, flush)
┌─────────────────────────────────┐
│  ╔════════════════════════════╗ │
│  ║  Camera window 18×18 mm   ║ │
│  ╚════════════════════════════╝ │
│  ┌──────────────────────────┐  │
│  │ OV2640 PCB (32×32 mm)    │  │  ← Inside satellite
│  │ M2 screws at 28 mm pitch │  │
│  └──────────────────────────┘  │
└─────────────────────────────────┘
```

The camera mount bracket (aluminum or 3D-printed PLA) attaches to the standoffs
via M3 screws and holds the camera PCB flat against the panel's inner surface.
The lens (~8 mm protrusion) extends through the window cutout.

### 5.5 Operational Profile

| Activity        | Trigger      | Duration | Notes |
|-----------------|-------------|----------|-------|
| Initialization  | FM_PAYLOAD entry | < 300 ms | SPI init, register config |
| Image capture   | Ground command CAM_TRIGGER | ~1 s (readout + JPEG compress) | |
| Data transfer   | SPI0 MISO stream | ~1 s @ 10 MHz SPI0 | frame transfer to OBC RAM buffer |
| JPEG compress   | On-module (FIFO bridge) | ~0.3 s | AL422B buffer |
| Store to flash  | Post-capture | ~100 ms | write to extended flash / SD card |
| Standby         | Between captures | — | camera in low-power sleep mode |

### 5.6 GPIO and Electrical Interface

| Signal        | GPIO   | Direction | Description |
|---------------|--------|-----------|-------------|
| SPI0 SCK      | GPIO18 | OUT       | Clock (≤ 10 MHz) |
| SPI0 MOSI     | GPIO19 | OUT       | Data to camera |
| SPI0 MISO     | GPIO17 | IN        | Data from camera |
| SPI0 CSn (CAM)| GPIO14 | OUT       | Active-low chip select |
| CAM_SDA       | GPIO2  | I/O       | I2C1 data (SCCB register config) |
| CAM_SCL       | GPIO3  | OUT       | I2C1 clock (SCCB register config) |
| PAYLOAD_ENABLE| GPIO21 | OUT       | 5V rail enable |

> **Note**: 8-pin Arducam Mini module — no RESET, TRIG, or FIFO_RDY pins.
> - SW reset via SCCB register 0x12 = 0x80
> - Capture trigger via SPI ARDUCHIP_FIFO register
> - Completion polled via SPI ARDUCHIP_STATUS register (0x07, bit 0)
>   while RW_MOTOR1 uses a separate PWM output slice. The RP2350 GPIO mux allows
>   reading a digital input on a pin that also has a PWM output — they are
>   independent functions.
> - **No conflict with MTQ**: Magnetorquers now use GPIO14/15/16 (see pico_pins.h),
>   leaving GPIO21/22 free for PAYLOAD_ENABLE and CAM_TRIGGER respectively.

---

## 6. Scientific Magnetometer (MAG-001)

### 6.1 Instrument Identification

| Parameter            | Value                           |
|----------------------|---------------------------------|
| Instrument ID        | MAG-001                         |
| Sensor               | PNI Sensor RM3100               |
| Purpose              | Precision geomagnetic field measurement |
| Scientific objective | Terrestrial magnetic field mapping in LEO |
| Cost estimate        | 150 – 200 USD                   |

> **Note on naming**: MAG-001 is the *scientific payload magnetometer* and is
> distinct from the ADCS magnetometer (HMC5883L/LIS3MDL at I2C address 0x1E/0x1C).
> Both sensors coexist on the I2C0 bus at different addresses.

### 6.2 Technical Characteristics

| Parameter          | Value               | Notes |
|--------------------|---------------------|-------|
| Measurement range  | ±800 µT             | |
| Resolution         | **15 nT**           | At 200 Hz single-cycle measurement |
| Noise density      | 13 nT/√Hz RMS       | |
| Output data rate   | Up to 200 Hz        | CMM (continuous measurement mode) |
| Operating voltage  | 3.3 V               | |
| Active current     | ~9 mA @ 3.3 V, 100 Hz | |
| Active power       | **~ 30 mW**         | |
| Interface          | **I2C or SPI**      | Selectable via hardware |
| I2C address        | 0x20 (SA0=SA1=0)    | Default; no address conflict on I2C0 |
| SPI max clock      | 1 MHz               | |
| Axes               | 3 (X, Y, Z)         | |
| Operating temp     | –40 °C to +85 °C    | |

### 6.3 Advantages for CubeSat Use

- Resolution of 15 nT far exceeds the ADCS magnetometer (HMC5883L: 200 nT resolution)
- Widely used in nanosatellites (e.g. GomSpace, Endurosat platforms)
- Low power — can be left running continuously during FM_PAYLOAD
- I2C0 integration: shares existing bus (GPIO4/5); no new PCB traces required

### 6.4 Output Data Format

| Field    | Type     | Units | Rate   | Notes |
|----------|----------|-------|--------|-------|
| `mag_x`  | int32_t  | counts → nT (scale ×13 nT/count) | 10 Hz | Scientific resolution |
| `mag_y`  | int32_t  | nT    | 10 Hz  | |
| `mag_z`  | int32_t  | nT    | 10 Hz  | |
| `status` | uint8_t  | —     | 10 Hz  | DRDY flag |

### 6.5 Electrical Interface (Phase 7 Baseline: I2C0)

| Signal  | GPIO       | Description                           |
|---------|------------|---------------------------------------|
| SDA     | GPIO4      | Shared I2C0 bus (existing pull-ups)   |
| SCL     | GPIO5      | Shared I2C0 bus                       |
| I2C addr| 0x20       | SA0=SA1=0 — no conflict with 0x68/0x1E|

> **I2C0 bus load**: Adding RM3100 (0x20) to the existing I2C0 bus (MPU-6050 at
> 0x68, HMC5883L at 0x1E) is electrically compatible. The 4.7 kΩ pull-ups (BOM
> §10 #9) provide sufficient drive at 400 kHz for three devices with total bus
> capacitance < 200 pF.

---

## 7. Radiation Detector (RAD-001)

### 7.1 Instrument Identification

| Parameter            | Value                           |
|----------------------|---------------------------------|
| Instrument ID        | RAD-001                         |
| Sensor               | Silicon PIN photodiode (X100-7 or equivalent) |
| Frontend circuit     | Transimpedance amplifier (TIA) + low-pass filter → ADC |
| Purpose              | Orbital particle radiation dose monitoring |
| Cost estimate        | 80 – 150 USD (diode + discrete frontend) |

### 7.2 Measurement Principle

The PIN diode generates electron-hole pairs when struck by energetic particles
(protons, electrons, heavy ions). The resulting photocurrent is converted to a
voltage by a transimpedance amplifier and sampled by the RP2350 ADC.

**Detectable particle types**:
- MeV-range protons (trapped proton belts, SAA)
- High-energy electrons (outer Van Allen belt passages)
- Heavy ions (galactic cosmic rays)
- Total Ionizing Dose (TID) accumulation

> **Limitation**: A single unshielded PIN diode does not discriminate between
> particle species. For dose-rate estimation only. Particle identification
> requires multiple sensors or collimator/shielding geometry (future scope).

### 7.3 Technical Characteristics

| Parameter         | Value                    | Notes |
|-------------------|--------------------------|-------|
| Detector          | Silicon PIN diode, active area 100 mm² | X100-7 or S1223-01 |
| Bias voltage      | 0 V (photovoltaic mode, no bias required for particle detection) | TIA reset drain |
| Responsivity      | ~0.5 A/W (optical); particle: MIP ~80 fC/µm | |
| TIA gain          | ~10 MΩ (adjustable via feedback resistor) | |
| Output voltage    | 0 – 3.3 V (ADC compatible) | |
| ADC channel       | ADC1 (GPIO27)            | 12-bit, 500 kSPS |
| Sample rate       | 1 Hz (logging), 100 Hz (burst mode) | |
| Operating voltage | 3.3 V (TIA from 5V rail via LDO) | |
| Active power      | **~ 80 mW** (diode + TIA) | |

### 7.4 Output Data Format

| Field          | Type     | Units     | Rate | Notes |
|----------------|----------|-----------|------|-------|
| `rad_counts`   | uint16_t | ADC counts (0–4095) | 1 Hz | Raw ADC reading |
| `rad_dose_uGy` | float    | µGy/s (approximate) | 1 Hz | Calibration-dependent |
| `accumulated_dose` | float | µGy | session total | Reset on FM_PAYLOAD exit |

### 7.5 Electrical Interface

| Signal        | GPIO   | Direction | Description |
|---------------|--------|-----------|-------------|
| ADC1          | GPIO27 | IN        | Analogue output from TIA (0–3.3 V) |
| PAYLOAD_ENABLE| GPIO21 | OUT       | Shared 5V rail enable |

---

## 8. Electrical and Data Interfaces

### 8.1 Power Interface

| Parameter         | Value       | Notes |
|-------------------|-------------|-------|
| Rail voltage      | 5 V         | Existing 5V Boost rail (POWER-BDG-001 §11) |
| Maximum current   | 1 A         | Sized for peak payload draw with 20% margin |
| Maximum power     | **5 W**     | Rail current limit (PLD-R-001) |
| Enable control    | GPIO21 (PAYLOAD_ENABLE) | Active-high; de-asserted in all non-FM_PAYLOAD modes |

**Per-instrument power distribution (5V rail sub-regulator on payload board):**

| Instrument | Operating Voltage | Current (avg) | Power (avg) |
|------------|------------------|---------------|-------------|
| CAM-001    | 3.3 V (LDO from 5V) | ~90 mA     | 300 mW |
| MAG-001    | 3.3 V (LDO from 5V) | ~9 mA      | 30 mW  |
| RAD-001    | 3.3 V (LDO from 5V) | ~24 mA     | 80 mW  |
| **Total**  |                 | **~123 mA** | **410 mW** |

### 8.2 Data Interface Summary

| Instrument | Interface  | Bus      | GPIO              | OBC Peripheral | Max Speed |
|------------|------------|----------|-------------------|----------------|-----------|
| CAM-001    | SPI        | SPI0     | GPIO17/18/19/23   | `spi0`         | 10 MHz    |
| MAG-001    | I2C        | I2C0     | GPIO4/5 (0x20)    | `i2c0`         | 400 kHz   |
| RAD-001    | Analogue   | ADC1     | GPIO27            | ADC            | 500 kSPS  |

### 8.3 Control Lines

| Signal          | GPIO   | Direction | Active | Description |
|-----------------|--------|-----------|--------|-------------|
| PAYLOAD_ENABLE  | GPIO21 | OUT       | High   | Enables 5V payload rail; all instruments powered |
| CAM_TRIGGER     | GPIO22 | OUT       | High pulse | Triggers image capture on SPI bridge |
| RAD_RESET       | GPIO2  | OUT       | High   | Resets TIA accumulator / dose counter |

> GPIO2 (previously UART0 TX, descoped per SRR ACT-06) is available for RAD_RESET.

### 8.4 Interface Timing

| Transaction       | Duration   | Notes |
|-------------------|-----------|-------|
| SPI0 frame (200 KB JPEG at 10 MHz) | ~1.6 s | 200 KB × 8 bits / 10 Mbps |
| I2C0 RM3100 single read (3 axes, 12 bytes) | ~300 µs | @400 kHz |
| ADC single conversion | 2 µs | RP2350 ADC @ 500 kSPS |

---

## 9. Power Budget

### 9.1 FM_PAYLOAD Load Additions

Adding the payload suite to the existing FM_NOMINAL baseline:

| Load          | Avg Power (mW) |
|---------------|----------------|
| FM_NOMINAL (existing) | 421 |
| CAM-001 (standby between captures) | 50 |
| MAG-001 (10 Hz continuous) | 30 |
| RAD-001 (1 Hz continuous) | 80 |
| **FM_PAYLOAD (no active capture)** | **581 mW** |
| CAM-001 (during active capture, +250 mW) | +250 |
| **FM_PAYLOAD (during capture)** | **831 mW** |

### 9.2 Energy Balance (FM_PAYLOAD scenario)

Using the solar array model from POWER-BDG-001 §8 (14.0 Wh/orbit generated, EOL):

| Scenario                    | Load (mW) | Per-orbit energy (Wh) | Net/orbit (Wh) | Verdict |
|-----------------------------|-----------|----------------------|----------------|---------|
| FM_PAYLOAD (no capture)     | 581       | 0.94                 | +13.06         | ✅ PASS |
| FM_PAYLOAD + 1 capture/orbit| 831 peak (2.4 s) + 581 avg | ≈ 0.94 | +13.06 | ✅ PASS |

> At 581 mW average, one orbit (96.7 min) consumes 0.94 Wh — still less than 7%
> of solar generation. The payload adds ≈ 38% to FM_NOMINAL power draw; the
> array margin remains strongly positive.

### 9.3 Eclipse Survival (FM_PAYLOAD)

| Scenario         | Load (mW) | Eclipse (37 min) | Energy req. (Wh) | Usable EOL (Wh) | Margin |
|------------------|-----------|------------------|-------------------|-----------------|--------|
| FM_PAYLOAD (avg) | 581       | 37 min           | 0.358             | 4.74            | +1224% |

All eclipse scenarios pass. FM_PAYLOAD must be disabled before `ENERGY_CRITICAL`
is reached (battery shedding sequence: payload rail off first — POWER-BDG-001 §11).

### 9.4 Peak Power Compliance

| Scenario                         | Peak Power | MRD ≤ 4 W limit | Status |
|----------------------------------|-----------|-----------------|--------|
| FM_PAYLOAD + TX burst + capture  | 831 + 3000 = 3831 mW | 4000 mW | ✅ PASS |

> Radio TX and camera capture are never simultaneous (payload task disables TX
> during capture via `comm_suspend()` call — see §12.3).

---

## 10. Data Budget

### 10.1 Per-Instrument Data Volume

| Instrument | Rate / Event  | Per orbit (90 min) | Notes |
|------------|--------------|-------------------|-------|
| CAM-001    | 300 KB/capture (1/orbit) | **300 KB** | JPEG Q=70; configurable |
| MAG-001    | 12 B/sample × 10 Hz × 5400 s | **648 KB** → **100 KB** with 10× decimation | Full 10 Hz log excessive; recommended decimation to 1 Hz → 64.8 KB |
| RAD-001    | 4 B/sample × 1 Hz × 5400 s | **21.6 KB** | Very low |
| **Total (nominal, with MAG decimation)** | | **≈ 386 KB/orbit** | Well within one pass downlink capacity |

### 10.2 Downlink Capacity vs. Payload Data

Using LINK-BDG-001 §6 (LoRa SF9 BW125 ~1758 bps effective, 12-min pass):

```
Available per pass = 1758 bps × 720 s = ~1260 kbits ≈ 157 KB
```

| Data volume / orbit | Passes to downlink |
|---------------------|-------------------|
| 386 KB nominal      | ~2.5 passes       |
| 864 KB (full MAG 10 Hz + 1 image) | ~5.5 passes |

> **Observation**: At full 10 Hz MAG logging, payload data exceeds a single-pass
> downlink capacity. The software shall support selectable decimation factors
> (1, 2, 5, 10) configurable by ground command.

### 10.3 Storage Requirements

Required onboard storage for 7-day buffering (before downlink):

```
386 KB/orbit × 14.9 orbits/day × 7 days ≈ 40 MB
```

The existing 2 MB flash on the RP2350 is **insufficient**. Phase 7 shall add an
external SPI flash or microSD card. A **W25Q128 (16 MB)** is sufficient for
~2 days of buffering; an **SDCard (≥ 1 GB)** meets the requirement PLD-R-003
(≥ 1 GB storage) with margin for growth. See ICD-OBC-001 §15 and OI-1.

---

## 11. Payload Operational Mode (FM_PAYLOAD)

### 11.1 Mode Definition

`FM_PAYLOAD` is a new flight mode added to the FMM (FMM-DES-001 §5):

```c
typedef enum {
    FM_BOOT       = 0,
    FM_SAFE       = 1,
    FM_DETUMBLE   = 2,
    FM_NOMINAL    = 3,
    FM_DIAGNOSTIC = 4,
    FM_PAYLOAD    = 5,   /* Scientific payload operations — PLS-001 active */
    FM_COUNT
} flight_mode_t;
```

### 11.2 Entry / Exit Conditions

| Condition      | Direction | Action |
|----------------|-----------|--------|
| Ground command from FM_NOMINAL | Entry | `fmm_request_transition(FM_PAYLOAD)` |
| Ground command to FM_SAFE / FM_NOMINAL | Exit | `fmm_request_transition()` — payload task disables 5V rail on mode exit |
| FAULT_LEVEL_CRITICAL | Exit | `fmm_force_safe()` — payload rail immediately shut off |
| `ENERGY_CRITICAL` EPS state | Exit | Fault → FM_SAFE; payload rail shed first (shedding order #1) |

### 11.3 Subsystem Behavior in FM_PAYLOAD

| Subsystem            | FM_PAYLOAD Behaviour                                           |
|----------------------|----------------------------------------------------------------|
| ADCS (attitude ctrl) | **Nadir-pointing** (LQR targeting nadir, same as FM_NOMINAL)   |
| Sensor read task     | Active (IMU + ADCS MAG at normal rates)                        |
| Telemetry task       | Full ADCS TLM + **payload HK packet** (MAG-001 field, RAD-001 dose, CAM status) |
| EPS / Health Monitor | Active                                                         |
| Payload task         | **Active** — MAG-001 @ 10 Hz, RAD-001 @ 1 Hz, CAM on command |
| Reaction wheels      | Active (Phase 7 scope — nadir pointing)                        |
| Magnetorquers        | Active (trim only)                                             |
| 5V PAYLOAD rail      | Enabled via GPIO21                                             |

### 11.4 Transition Matrix

`FM_PAYLOAD` is reachable only from `FM_NOMINAL` and returns to `FM_NOMINAL`
or `FM_SAFE` (any fault):

```
From \ To   │ … NOMINAL  PAYLOAD
────────────┼─────────────────────
NOMINAL     │     —        ✓
PAYLOAD     │     ✓        —
```

`FM_PAYLOAD → FM_SAFE` is always allowed (bypass rule, same as all modes).

---

## 12. Software Architecture

### 12.1 New Modules

```
src/
  drivers/
    payload/
      camera_driver.c         ← SPI0 frame readout + trigger
      rm3100.c                ← I2C RM3100 driver (read, init, CMM config)
      radiation_driver.c      ← ADC1 single-channel read + TIA reset
      spi_payload.c           ← SPI0 bus init (shared: camera + RM3100)
  tasks/
    payload_task.c            ← FreeRTOS payload task (10 Hz MAG, 1 Hz RAD, on-cmd CAM)
  services/
    payload/
      payload_manager.c       ← enable/disable, status, storage coordinator

include/
  camera_driver.h
  rm3100.h
  radiation_driver.h
  payload_task.h
  payload_manager.h

tests/unit/
  test_rm3100.c               ← T-PLD-MAG-01..05
  test_radiation_driver.c     ← T-PLD-RAD-01..03
  test_payload_task.c         ← T-PLD-TSK-01..06
```

### 12.2 FreeRTOS Task Profile

| Task            | Priority | Stack (bytes) | Period   | Notes |
|-----------------|----------|---------------|----------|-------|
| `PayloadTask`   | 3        | 2 048         | 100 ms   | MAG every 100 ms (10 Hz); RAD every 1000 ms; CAM on notification |
| Existing tasks  | 4 (ADCS), 2 (sensor, HLT, CMD, TLM) | — | — | Priorities unchanged |

### 12.3 Task Behavior

```c
void payload_task(void *params) {
    while (1) {
        if (fmm_get_mode() != FM_PAYLOAD) {
            payload_manager_disable();
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }

        payload_manager_enable();           // idempotent — enables 5V rail on first call

        // 10 Hz MAG sample
        if (mag_sample_due()) {
            rm3100_read(&mag_data);
            flash_store_mag(&mag_data);
        }

        // 1 Hz RAD sample
        if (rad_sample_due()) {
            radiation_read(&rad_data);
            flash_store_rad(&rad_data);
        }

        // On-command camera capture (notification from command_task)
        if (xTaskNotifyWait(0, 0, &notification, 0) == pdTRUE) {
            if (notification & NOTIFY_CAM_CAPTURE) {
                camera_trigger_capture();
                camera_readout_spi(&image_buf, &image_len);
                flash_store_image(&image_buf, image_len);
            }
        }

        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(100));
    }
}
```

### 12.4 Modified Modules

| File | Change |
|------|--------|
| `include/flight_mode.h` | Add `FM_PAYLOAD = 5` before `FM_COUNT` |
| `src/services/fmm/flight_mode_manager.c` | Add PAYLOAD row/column to `g_allowed[][]`; add `"PAYLOAD"` to `fmm_mode_name()` |
| `src/obc_main.c` | Create `PayloadTask` in task init sequence |
| `include/data_layer.h` | Add `payload_status_t` field to `obc_snapshot_t` |
| `src/core/data_layer.c` | Implement payload status accessors |
| `include/config.h` | Add `PAYLOAD_SPI_HZ`, `CAM_TRIGGER_PIN`, `PAYLOAD_ENABLE_PIN`, `RAD_ADC_CHANNEL` |
| `config/pico_pins.h` | Add payload GPIO assignments; reassign RW3 to GPIO3 |
| `include/log_event_ids.h` | Add `LOG_EVT_PAYLOAD_ENABLE`, `LOG_EVT_PAYLOAD_DISABLE`, `LOG_EVT_CAM_CAPTURE` |
| `include/fault_ids.h` | Add `FAULT_PAYLOAD_CAM_FAIL`, `FAULT_PAYLOAD_MAG_FAIL`, `FAULT_PAYLOAD_STORAGE_FULL` |
| `scripts/pico_ci.sh` | No changes — payload task built with existing CMake target |
| `src/CMakeLists.txt` | Add `drivers/payload/`, `services/payload/`, `tasks/payload_task.c` |

---

## 13. Verification

### 13.1 Unit Tests

| Test ID         | Description                                    | Pass Criterion |
|-----------------|------------------------------------------------|----------------|
| T-PLD-MAG-01    | `rm3100_init()` configures CMM register correctly | Correct register value written via I2C mock |
| T-PLD-MAG-02    | `rm3100_read()` returns correctly scaled nT values | Within 1 nT of expected (mock data) |
| T-PLD-MAG-03    | `rm3100_read()` returns false on I2C NACK | Return value false, fault raised |
| T-PLD-MAG-04    | RM3100 does not conflict with ADCS magnetometer on I2C0 | Address 0x20 ≠ 0x1E/0x68 |
| T-PLD-RAD-01    | `radiation_read()` returns correct ADC-to-dose conversion | Within 1% of expected (mock ADC) |
| T-PLD-RAD-02    | ADC saturation handled gracefully | Returns `FAULT_PAYLOAD_ADC_OVERRANGE` |
| T-PLD-TSK-01    | `PayloadTask` enters data-collection loop only in `FM_PAYLOAD` | No samples logged in other modes |
| T-PLD-TSK-02    | MAG logged at 10 Hz (100 ms period, ±5ms jitter) | Timestamp delta checks |
| T-PLD-TSK-03    | RAD logged at 1 Hz | Timestamp delta checks |
| T-PLD-TSK-04    | CAM capture triggered by task notification | Mock notification triggers capture flow |
| T-PLD-TSK-05    | `FM_PAYLOAD` exit disables 5V rail (GPIO21 de-asserted) | GPIO state check |
| T-PLD-TSK-06    | FAULT_PAYLOAD_STORAGE_FULL raised when flash buffer full | Fault raised, capture disabled |

### 13.2 Integration Tests

| Test ID         | Description                                    | Pass Criterion |
|-----------------|------------------------------------------------|----------------|
| T-PLD-INT-01    | FM_NOMINAL → FM_PAYLOAD → FM_NOMINAL transition | FMM accepts; PAYLOAD_ENABLE asserted/de-asserted |
| T-PLD-INT-02    | FAULT_LEVEL_CRITICAL forces FM_SAFE from FM_PAYLOAD | Payload rail off, FM_SAFE reached |
| T-PLD-INT-03    | Telemetry includes payload HK in FM_PAYLOAD | CSP packet contains non-zero MAG/RAD fields |
| T-PLD-INT-04    | Ground command triggers image capture and storage | Flash image count increments |

### 13.3 Compliance Matrix

| Requirement   | Verification Method          | Status |
|---------------|------------------------------|--------|
| PLD-R-001     | Power rail measurement on HW | ⏳ Phase 7 HW test |
| PLD-R-002     | SPI0 loopback test (sw)      | ⏳ Phase 7 SW test |
| PLD-R-003     | Storage capacity check (sw)  | ⏳ Phase 7 SW test |
| PLD-R-004     | FMM transition test (T-PLD-INT-01) | ⏳ Phase 7 |
| PLD-R-005     | Thermal model (BOM-OBC-001)  | ⏳ Phase 7 |

---

## 14. Open Items

| OI  | Description | Priority | Status |
|-----|-------------|----------|--------|
| OI-1 | External storage selection: W25Q128 (16 MB SPI flash) vs. microSD — finalize in Phase 7 BOM | High | Open |
| OI-2 | Camera SPI bridge selection: confirm Arducam IMX219 mini SPI module availability and SPI protocol specification | High | Open |
| OI-3 | Confirm CAM_FIFO_RDY (GPIO10 input) is compatible with RW_MOTOR1_PWM (GPIO10 output) — RP2350 GPIO mux supports this, but HW test required | Medium | Updated |
| OI-4 | RM3100 procurement lead time: PNI Sensor typical 4–6 week lead; order early in Phase 7 | Medium | Open |
| OI-5 | Payload data downlink protocol: define CSP service ID for payload file transfer (large file chunking) | Medium | Open |
| OI-6 | Thermal analysis: 410 mW of payload in small board area; thermal interface to nadir panel TBD | Medium | Open |
| OI-7 | Payload pointing: nadir-pointing requirement for imaging — ADCS Phase 2 precision pointing must be operational | High | Depends on Phase 2 |

---

## 15. References

| Ref | Document |
|-----|---------|
| [1] | MRD-OBC-001 v1.1 — Mission requirements and ConOps |
| [2] | SRS-OBC-001 v2.2 — Software requirements (PLD-R-001..005) |
| [3] | ICD-OBC-001 v1.1 — Hardware interface definitions |
| [4] | POWER-BDG-001 v0.3 — Power budget including FM_PAYLOAD scenario |
| [5] | FMM-DES-001 v0.4 — FM_PAYLOAD mode definition |
| [6] | Sony IMX219 datasheet |
| [7] | PNI RM3100 datasheet and application note AN-103 |
| [8] | Hamamatsu X100-7 PIN diode datasheet |
| [9] | ECSS-E-HB-10-02A — Verification guidelines |
