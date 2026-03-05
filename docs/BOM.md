# Bill of Materials (BOM) — CubeSat OBC Hardware

**Document ID**: BOM-OBC-001  
**Version**: 0.9  
**Date**: 2026-03-05  
**Branch**: `feature/hardware-bom`  
**Status**: 🔄 In progress — components added as they are evaluated

---

## 0. Mission Parameters (BOM reference)

> These orbital parameters are the basis for all link budget, EPS (eclipse), and contact window calculations.

| Parameter | Value | Notes |
|-----------|-------|-------|
| Orbit type | **SSO — Sun-Synchronous / Polar** | Fixed local solar time each day |
| Altitude | **500–700 km** (nominal target: 600 km) | Low LEO |
| Inclination | **~96°–98°** | Polar coverage |
| Orbital period | **94.5 min** (500 km) – **98.6 min** (700 km) | $T = 2\pi\sqrt{a^3/\mu}$ |
| Max eclipse | **35–37 min** (worst case, $\beta = 0°$) | $t_{ecl} = T \cdot \frac{\arccos\sqrt{1-(R_E/a)^2}}{\pi}$ |
| Min eclipse | **0 min** (continuous sunlight when $|\beta| > 66°$) | Part of the year eclipse-free |
| Contact window (GS) | **10–14 min/pass**, 2–4 passes/day | Depends on GS latitude and min elevation |
| Slant range overhead (nadir) | ~500–700 km | Zenith pass |
| **Critical slant range** (5° el., horizon) | **~2000–2300 km** | ⚠️ Critical case for link budget |
| Orbital drift | ~0.98°/day (precession) | SSO maintains fixed LTAN |

**Direct BOM implications:**
- **Link budget**: size for slant range **2300 km** (horizon), not 600 km (nadir) — see §6
- **EPS / battery**: size to cover **37 min of eclipse** per orbit — see §9
- **GPS**: highly relevant — SSO passes at the **same local solar time** every day; GPS provides precise timestamps and position to correlate sensor data with geographic coordinates — see §4
- **GS (Ground Station)**: useful link window ≤8 min per pass → CSP protocol + 1 Hz telemetry must be efficient

---

## 1. Purpose

This document lists all hardware components required to assemble the flight prototype
of the OBC (On-Board Computer) based on the Raspberry Pi Pico 2W (RP2350, Cortex-M33).
Firmware compatibility status and integration notes are included for each component.

**Status legend:**
| Symbol | Meaning |
|--------|---------|
| ✅ Integrated | Driver and requirement implemented in firmware |
| 🔄 Planned | Parts evaluated, driver pending development |
| ❓ To evaluate | Candidate, compatibility analysis pending |
| ❌ Rejected | Incompatible or superseded by another component |

---

## 2. Processor / Central OBC

| # | Component | P/N / Model | Qty | Status | Notes |
|---|-----------|-------------|-----|--------|-------|
| 1 | OBC Microcontroller | Raspberry Pi Pico 2W | 2 | ✅ Integrated | RP2350 (Cortex-M33 dual-core, 520 KB SRAM, Wi-Fi/BT); 1 flight + 1 spare |

**Firmware reference**: `config/pico_pins.h`, `src/CMakeLists.txt`

> **PDR notes**:
> - The RP2350 Cortex-M33 core exposes the **DWT cycle counter** (`DWT->CYCCNT`) — use it to measure WCET (Worst-Case Execution Time) of FreeRTOS tasks before CDR.
> - The Pico 2W is **COTS (not space-grade)**. For flight, track TID/SEE susceptibility; consider conformal coating and latch-up protection on critical power rails.
> - External hardware watchdog (§10 item #12) is recommended to ensure SAFE MODE recovery if the OBC hangs.

---

## 3. Attitude Sensors (ADCS)

| # | Component | P/N / Model | Qty | Status | Notes |
|---|-----------|-------------|-----|--------|-------|
| 2 | 6-DOF IMU | MPU-6050 (GY-521 module) | 2 | ✅ Integrated | I2C @ 400 kHz, addr 0x68; GPIO4 (SDA), GPIO5 (SCL) |
| 3 | 3-axis Magnetometer | HMC5883L (GY-271 module) | 2 | ⚠️ Integrated (risk) | I2C0 bus; driver `src/drivers/mag/hmc5883l.c` — Phase 5; **see §3.1 — discontinued IC risk** |

**Integration notes — Attitude sensors:**
- IMU and magnetometer share I2C0 bus (`GPIO4`/`GPIO5`, fast-mode 400 kHz).
- The EKF fuses accelerometer + gyroscope + magnetometer (3-state: roll/pitch/yaw).
- I2C pull-up resistors (4.7 kΩ) on SDA/SCL are **confirmed required** — see §10 item #9.
- Reference: `src/tasks/sensor_read_task.c`, `include/ekf.h`

### 3.1 HMC5883L Discontinuation Risk

> **⚠️ PDR Finding**: The HMC5883L magnetometer has been **discontinued by Honeywell**. Most GY-271 modules sold today contain a **QMC5883L clone** (QST Corporation) with a different register map and I2C address (`0x0D` vs `0x1E`). A driver built for HMC5883L will **silently fail or return garbage data** on a QMC5883L module.

**Mitigation for lab prototype:**
- Before using a GY-271 module, verify the IC markings on the chip itself.
- If the IC is QMC5883L: either adapt the existing driver (`src/drivers/mag/hmc5883l.c`) or use a QMC5883L-specific driver, updating the I2C address and register definitions.

**Flight-grade alternatives (CDR decision required):**

| Component | Model | Interface | Temp range | Notes |
|-----------|-------|-----------|-----------|-------|
| IMU (upgrade) | **ICM-42688-P** (TDK InvenSense) | SPI / I2C | −40 to +85 °C | High-precision, DMP, actively produced; drop-in upgrade for MPU-6050 |
| Magnetometer (replacement) | **LIS3MDL** (STMicroelectronics) | SPI / I2C | −40 to +85 °C | Low-power, 16-bit, actively produced; functional HMC5883L replacement |

> **Lab decision**: the GY-271 is acceptable for prototype if the actual IC is confirmed. For flight, migrate to **LIS3MDL** on the custom OBC PCB (CDR scope).

---

## 4. Navigation / Positioning

| # | Component | P/N / Model | Qty | Status | Notes |
|---|-----------|-------------|-----|--------|-------|
| 4 | GPS Module | GY-NEO6Mv2 with NEO-7M + antenna | 2 | 🔄 Planned | UART @ 9600 baud, 3.3V; requires freeing UART0 — see §4.1 |

> **SSO relevance**: GPS is **especially useful** in this mission. The SSO passes at the same local solar time every day → GPS provides precise timestamps and position to correlate readings with geographic coordinates. Also enables OBC clock synchronization on each pass.

### 4.1 GPS GY-NEO6Mv2 / NEO-7M Compatibility

**Hardware**: ✅ Compatible with condition  
**Software**: 🔄 FreeRTOS driver and task pending development

| Feature | GY-NEO6Mv2 (NEO-7M) | RP2350 / Pico 2W | Status |
|---------|---------------------|------------------|--------|
| Protocol | UART (NMEA 0183) | 2× hardware UART | ✅ |
| Logic voltage | 3.3 V | GPIO at 3.3 V | ✅ |
| Supply voltage | 3.3–5 V (onboard regulator) | 3.3 V available | ✅ |
| Antenna | Passive ceramic (included) | N/A | ✅ |
| Default baud rate | 9600 | Configurable | ✅ |

**UART conflict** (must be resolved before purchase):

| Port | Pins | Current use | Available for GPS |
|------|------|-------------|-------------------|
| UART0 | GPIO0/GPIO1 | Debug ASCII @ 115200 | ⚠️ Freeable if debug→USB CDC |
| UART1 | GPIO4/GPIO5 | CSP/KISS telemetry @ 115200 | ❌ In use |

**Selected solution: Option A — Free UART0 for GPS**
> Move all debug output to USB CDC (already enabled in `src/CMakeLists.txt`,
> `pico_enable_stdio_usb = 1`). UART0 is then free to receive NMEA sentences
> from the GPS. Estimated firmware change: minimal (1 line in `pico_pins.h` + NMEA driver).

**Firmware work required** (Phase 7 or new task):
- [ ] NMEA parser driver (`src/drivers/gps/neo7m.c`)
- [ ] FreeRTOS GPS task (`src/tasks/gps_task.c`) — 1 Hz
- [ ] Data Layer write (`data_layer_write_gps()`)
- [ ] SyRS requirement (`SYS-F-GPS-001` — orbital position)
- [ ] Unit tests (`tests/unit/test_gps.c`)

---

## 5. Power Management

| # | Component | P/N / Model | Qty | Status | Notes |
|---|-----------|-------------|-----|--------|-------|
| 5 | Temperature sensor | TMP102 or internal ADC4 | 1 | ✅ Integrated | ADC4 mode active; I2C addr 0x48 if external |

> Battery, 5V regulator, charger, and solar panel are defined in **§9 EPS**.

---

## 6. Communications (TT&C)

| # | Component | P/N / Model | Qty | Status | Notes |
|---|-----------|-------------|-----|--------|-------|
| 6 | TT&C Transceiver (flight) | EBYTE E22-400M30S (SX1268, 433 MHz LoRa) | 2 | 🔄 Planned | Transparent UART 3.3V, 30 dBm (1W); see §6.1 — **buy for flight** |
| 6b | TT&C Transceiver (lab/GS) | HC-12 Si4463 (433 MHz FSK, TTL UART) | 2 | 🔄 **Buy now** | Plug-and-play KISS/CSP; same firmware as E22; ~$3/unit — see §6.2 |
| 6c | TT&C Antenna (lab) | 433 MHz whip/rubber-duck SMA (5–8 dBi) | 4 | 🔄 **Buy now** | 2× for HC-12 OBC+GS, 2× spare; ~$1–2 each |
| 6d | TT&C Antenna (flight) | Custom λ/4 dipole at 434 MHz (~17.3 cm wire + radials) | 1 | 🔄 Planned | λ/4 with velocity factor 0.95 = 16.4 cm wire + ground plane; see §6.3. For CubeSat deployment, prefer **steel tape-measure strip** (16.4 cm × 2 elements) — spring-loaded, self-deploying, robust against vibration |
| 6e | TT&C EMI filter | SAW filter 433 MHz (e.g. TDK B39431-B3735-U410 or equivalent) | 2 | 🔄 Planned | Insert between antenna port and E22-400M30S RF input; attenuates out-of-band interference; critical for EMI immunity in CubeSat bus environment; ~$1–2 each |

> See also **§7 Ground Station** for ground control hardware.

### 6.1 UHF/VHF Transceiver Analysis

**Firmware requirements for TT&C:**
- Interface: **UART1** (`GPIO4 TX` / `GPIO5 RX`) @ **115200 baud**, 3.3 V TTL
- Protocol: **KISS framing** over the physical layer → radio must act as a **transparent UART pipe**
- Stack: CSP v2 (OBC addr 10, GS addr 1); telemetry 1 Hz @ 29 bytes/packet

> Firmware reference: [docs/PHASE3_COMM_SPEC.md](PHASE3_COMM_SPEC.md), `src/drivers/uart/pico_usart.c`

**Evaluated candidates:**

| Module | Frequency | Interface | Power | KISS-compatible | Est. price | Verdict |
|--------|-----------|-----------|-------|----------------|-----------|---------|
| **EBYTE E22-400M30S** (SX1268) | 410–493 MHz | UART TTL 3.3V | 30 dBm (1W) | ✅ Transparent mode | ~$15 | ✅ **Recommended** |
| **EBYTE E22-900M30S** (SX1262) | 850–930 MHz | UART TTL 3.3V | 30 dBm (1W) | ✅ Transparent mode | ~$15 | ✅ Alternative (different band) |
| **HC-12** (Si4463) | 433 MHz FSK | UART TTL 3.3V | 20 dBm (100 mW) | ✅ Transparent mode | ~$3 | ⚠️ Lab prototype only |
| **Dorji DRA818U** | 400–470 MHz UHF | UART (AT cmd) + analog audio | 1W | ❌ Analog audio, requires TNC | ~$8 | ❌ Not directly compatible |
| **AX5043** (IC) | Multi-band sub-GHz | **SPI** | configurable | ❌ SPI → requires new driver | ~$10 (IC) | ❌ Architecture change |
| **RFM98W** / SX1276 | 433/868/915 MHz | **SPI** | 20 dBm | ❌ SPI | ~$5 | ❌ Architecture change |

**Why is the E22-400M30S recommended?**

1. **Native transparent UART**: in normal operating mode acts as an RF serial cable → current KISS/CSP firmware stack works without modifications.
2. **TTL 3.3 V**: directly compatible with RP2350 GPIO, no level-shifter needed.
3. **30 dBm (1 W)**: adequate TX power for LEO link (~600 km) with dipole antenna.
4. **LoRa + FSK**: configurable; for LEO, FSK @ 9600–115200 baud or LoRa SF7 is recommended for better link budget.
5. **433 MHz (UHF)**: aligned with amateur satellite frequencies (IARU Region 2: 435–438 MHz).

**Wiring diagram (no level-shifter required):**
```
Pico 2W                    E22-400M30S
GPIO4 (TX) ─────────────▶ RXD
GPIO5 (RX) ◀───────────── TXD
3.3V       ─────────────▶ VCC
GND        ─────────────▶ GND
GPIO[free] ─────────────▶ M0  (mode: 00 = transparent)
GPIO[free] ─────────────▶ M1
GPIO[free] ─────────────▶ AUX (busy/ready flag, optional)
```

**Firmware work required** (minimum — direct compatibility):
- [ ] Confirm E22 baud rate configured to 115200 (configurable via AT before flight)
- [ ] Add M0/M1/AUX configuration to `pico_pins.h` with free GPIOs
- [ ] End-to-end KISS/CSP test with real hardware

**RF notes for SSO (500–700 km, 97° inclination):**

**⚠️ The critical case is NOT the zenith pass — it is the horizon pass.** At 5° minimum elevation slant range reaches ~2300 km.

| Parameter | Zenith pass (600 km) | **Horizon pass (2300 km, 5° el.)** |
|-----------|---------------------|-------------------------------------|
| Free-space path loss @ 435 MHz | 140.8 dB | **152.5 dB** (+11.7 dB more) |
| EIRP E22 (1W + 3 dBi dipole) | 33 dBm | 33 dBm |
| Received signal (GS 3 dBi dipole) | −104.8 dBm | **−116.5 dBm** |
| E22 sensitivity FSK @ 9600 bps | −125 dBm | −125 dBm |
| **E22 link margin** | **+20.2 dB** | **+8.5 dB ✅ (sufficient)** |
| **HC-12 margin** (100 mW, −117 dBm) | +10 dB | **−9.5 dB ❌ (insufficient)** |

> The E22-400M30S closes the link with +8.5 dB margin even at the 2300 km horizon — confirms it is the right choice for flight.
> The HC-12 is clearly rejected for flight with the real SSO parameters.

- Requires amateur license (IARU coordinate frequency 435–438 MHz)
- Antenna: λ/4 dipole (~16.4 cm at 434 MHz)
- Contact window ~10–14 min/pass → CSP protocol must transmit maximum telemetry in that time

### 6.2 Specific analysis: HC-12 Si4463 433 MHz

**Verdict: ✅ Excellent for development/GS — ⚠️ Insufficient margin for LEO flight**

The HC-12 is the **simplest to integrate** of all candidates evaluated: transparent UART, same band (433 MHz), and compatible with KISS/CSP with no firmware changes.

| Feature | HC-12 (Si4463) | E22-400M30S (SX1268) |
|---------|----------------|----------------------|
| Pico 2W interface | ✅ Direct UART TTL (3.3V–5V) | ✅ Direct UART TTL (3.3V) |
| KISS/CSP plug-and-play | ✅ Yes, mode FU3 = transparent pipe | ✅ Yes, transparent mode |
| Frequency | ✅ 433.4–473 MHz configurable | ✅ 410–493 MHz configurable |
| Baud rate | ✅ up to 115200 bps (AT+Bxxxx) | ✅ up to 115200 bps |
| TX power | ⚠️ 20 dBm (100 mW) | ✅ 30 dBm (1 W) |
| RX sensitivity | ⚠️ −117 dBm @ 5 kbps | ✅ −148 dBm (LoRa SF12) |
| Price | ✅ ~$3–5 | ~$15 |
| Level-shifter needed | ✅ No (accepts 3.3V and 5V) | ✅ No |
| Extra config pins (M0/M1/AUX) | ✅ No (UART + optional SET pin) | ⚠️ Yes (3 extra GPIOs) |

#### Wiring diagram — HC-12 (simpler than E22):
```
Pico 2W                  HC-12
GPIO4 (TX) ───────────▶ TXD
GPIO5 (RX) ◀─────────── RXD
3.3V       ───────────▶ VCC  (accepts 3.2–5.5V)
GND        ───────────▶ GND
                        SET  (leave open = normal mode; GND = AT cmd mode)
```
> Only 4 wires. No extra mode pins in normal operation.

#### Link budget for SSO (critical case: horizon at 2300 km, 5° el.):

| Parameter | HC-12 (100 mW) | E22-400M30S (1 W) |
|-----------|----------------|-------------------|
| EIRP TX | ~23 dBm (3 dBi dipole) | ~33 dBm |
| Path loss SSO **2300 km** @ 435 MHz | ~152.5 dB | ~152.5 dB |
| Received signal (GS 3 dBi dipole) | **−126.5 dBm** | **−116.5 dBm** |
| RX sensitivity (FSK 9600 bps) | −117 dBm | −125 dBm (FSK) |
| **Link margin** | **−9.5 dB ❌** | **+8.5 dB ✅** |

> With real SSO parameters (horizon 2300 km), the HC-12 has even less margin than the previous 600 km calculation.

> **Link budget conclusion**: the HC-12 at 100 mW does not have sufficient margin for a reliable LEO link with dipole antennas. It would only be viable with high-gain yagi antennas at the GS (≥10 dBi), which complicates the ground station.

#### Recommended roles and acquisition plan:

| Role | HC-12 | E22-400M30S |
|------|-------|-------------|
| **Lab testing (now)** | ✅ **Buy now** — plug-and-play, cheap, same firmware | ✅ Also valid |
| Short-range field testing (< 1 km) | ✅ More than enough | ✅ |
| Ground Station radio (GS) | ✅ Economical pair with yagi antenna | ✅ Standard pair |
| **LEO flight transceiver** | ❌ No link margin | ✅ **Buy for flight** |

> **Suggested plan**: buy 2× HC-12 now (~$6–10 total) to start all lab KISS/CSP/telemetry tests.
> When E22-400M30S modules arrive for flight,
> **the firmware does not change** — same UART, same configuration, same pins.

---

## 7. Ground Station

| # | Component | P/N / Model | Qty | Status | Notes |
|---|-----------|-------------|-----|--------|-------|
| GS-1 | GS Radio (bench / secondary) | LORA32U4 II 915 MHz + IPEX antenna | 1 | ⚠️ Secondary | PC→USB→ATmega32U4→SX1276; bench testing only (915 MHz); requires custom bridge firmware; see §7.1 |
| GS-2 | GS Radio **(recommended)** | EBYTE E22-400M30S (433 MHz) + USB-UART adapter | 1 | 🔄 **Planned** | Identical hardware to satellite; symmetric 433 MHz pair; plug-and-play with existing KISS/CSP firmware; **preferred final GS**; see §7.2 |
| GS-3 | PC / Laptop | Any Linux/Mac/Win PC | 1 | ✅ Available | Runs CSP ground client (Phase 3) |
| GS-4 | USB-UART adapter | CP2102 or CH340G module (3.3V TTL, USB) | 1 | 🔄 **Buy now** | Links PC USB port to E22-400M30S UART; must be 3.3V TTL (not RS-232); ~$1–2 |

### 7.1 LORA32U4 II as Ground Station radio

**Verdict for GS: ⚠️ Secondary option — usable with custom firmware at 915 MHz (bench testing only). See §7.2 for the recommended GS configuration.**

Although not suitable for the satellite OBC (see §6.1), the LORA32U4 II makes sense as a GS radio for bench testing:

| Aspect | Detail |
|--------|--------|
| **PC connection** | Native USB (ATmega32U4 has hardware USB) → appears as serial port `/dev/ttyACM0` |
| **RF chip** | Internal SX1276 → same core as many LoRa modules |
| **Required firmware** | USB↔LoRa serial bridge (e.g. `RadioLib` or `arduino-lmic` with KISS mode) |
| **Band** | 915 MHz ISM — valid for bench testing in ITU Region 2 (Americas); **not suitable for flight** |
| **TX power** | 20 dBm (100 mW) — sufficient for bench distances (< 1 km) |
| **IPEX antenna** | IPEX (U.FL) connector with cable included → connect 915 MHz dipole antenna |
| **Price** | ~$18–22 |

**Bench test flow:**
```
PC (CSP Python/C client)
  └─ USB serial ─▶ LORA32U4 II (KISS/LoRa bridge @ 915 MHz)
                        │
                        │ RF 915 MHz
                        │
                   E22-400M30S or second LORA32U4 II
                        │
                        └─ UART1 ─▶ Pico 2W OBC (KISS/CSP firmware)
```

> **Important note**: For bench testing at 915 MHz, both ends must use 915 MHz — the E22-400M30S defaults to 433 MHz. If LORA32U4 II is chosen as GS, the bench pair would be: **LORA32U4 II (GS, 915) ↔ E22-900M30S (satellite, 915)**. For real flight, migrate to 433/435 MHz and replace the GS with an E22-400M30S connected via UART to the PC.

**Pending items to use LORA32U4 II as GS:**
- [ ] Write/adapt KISS-serial bridge firmware for ATmega32U4 (Arduino + RadioLib)
- [ ] Validate that the KISS bridge is bit-for-bit compatible with the OBC's `csp_if_kiss`
- [ ] Define final flight band (433 vs 915 MHz) to ensure correct pair

### 7.2 Recommended Ground Station: E22-400M30S + USB-UART adapter

**Verdict for GS: ✅ Recommended — no custom firmware required, symmetric 433 MHz pair**

| Aspect | Detail |
|--------|--------|
| **PC connection** | CP2102 or CH340 USB-UART adapter → `/dev/ttyUSBx` on Linux |
| **RF module** | E22-400M30S — identical to satellite module; transparent UART mode |
| **Band** | 433 MHz — same as flight band; no reconfiguration needed |
| **Firmware** | None required — same KISS/CSP Python/C client used for bench tests |
| **TX power** | 30 dBm (1 W) — sufficient for full link budget test at bench and field |
| **Antenna** | 433 MHz rubber-duck SMA (GS-side) — same as item 6c |
| **Total GS cost** | ~$15 (E22) + ~$2 (USB-UART) = **~$17** |

**GS wiring:**
```
PC (USB)
  └─ CP2102 / CH340 (USB-UART)
        ├─ TX (3.3V) ─────────────▶ RXD  E22-400M30S
        ├─ RX ◀───────────────────── TXD  E22-400M30S
        ├─ 3.3V ──────────────────▶ VCC
        └─ GND ───────────────────▶ GND
                                    M0/M1 → GND (transparent mode)
```

> **Migration path**: the lab HC-12 pair validates the KISS/CSP stack end-to-end. When E22 modules arrive, swap hardware — **no firmware changes**. The E22 GS replaces the LORA32U4 II for all flight-band testing.

---

## 8. Actuators (ADCS)

| # | Component | P/N / Model | Qty | Status | Notes |
|---|-----------|-------------|-----|--------|-------|
| 7 | Reaction Wheels (flight) | (TBD — custom BLDC) | 3 | ❓ To evaluate | GPIO6/7/8 (PWM3A/3B/4A); see §8.1 |
| 7b | Reaction Wheels **(lab)** | DC motor with encoder + TB6612 driver | 3 | 🔄 **Buy now** | Emulates inertia; direct PWM on GPIO6/7/8; see §8.1 |
| 8 | Magnetorquers (flight) | Custom ferrite coil + H-bridge | 3 | ❓ To evaluate | GPIO14/15/16 (PWM7A/7B/0A); see §8.2 |
| 8b | Magnetorquers **(lab)** | DRV8833 module + ferrite coil | 3 | 🔄 **Buy now** | Bidirectional PWM control; see §8.2 |

> **⚠️ PDR Finding — ADCS Strategy: Magnetorquers First**
>
> The **magnetorquer subsystem alone** is sufficient for the first ADCS operational phase:
> - **Detumbling** (B-dot control): reduce angular rate after deployment using only MTQs.
> - **Safe mode attitude hold**: coarse pointing with MTQs + EKF; no reaction wheels required.
>
> Reaction wheels are needed only for **precision pointing** (Phase 2 ADCS). This means:
> 1. The MTQ-only lab setup (`DRV8833 + ferrite coil`) should be the **first hardware validation target**.
> 2. Reaction wheel hardware can be deferred until MTQ detumbling is verified in the loop.
> 3. In a contingency scenario (RW failure), the satellite can still maintain safe mode with MTQs.
>
> **Firmware implication**: implement and validate `b_dot_control.c` (MTQ-only) before `lqr_control.c` (RW+MTQ full ADCS).

### 8.1 Reaction Wheels — lab options

**Firmware interface:**
- PWM control on `GPIO6` (RW1), `GPIO7` (RW2), `GPIO8` (RW3) — see `config/pico_pins.h`
- Firmware model: `reaction_wheel_apply_torque(rw, torque, dt)` → updates `rw->omega`
- Parameters: `RW_MAX_OMEGA_RPM = 4000`, `RW_INERTIA = 0.001 kg·m²`
- RP2350 hardware PWM driver (`hardware_pwm`) already available on slices PWM3/PWM4

**Lab components (buy now):**

| Component | Suggested model | Price | Function |
|-----------|----------------|-------|---------|
| DC motor with encoder | GA12-N20 (6V, 100–300 RPM) or N20 micro | ~$3–5 each | Simulates the reaction wheel |
| H-bridge motor driver | TB6612FNG (breakout module) | ~$2–3 each | Converts Pico PWM → bidirectional motor current |
| Inertia disk | Acrylic or metal disk ~5 cm Ø | ~$1 | Increases shaft inertia for realistic behavior |

**Wiring diagram — Reaction Wheel × 1 axis (repeat × 3):**
```
Pico 2W                    TB6612FNG            Motor N20
GPIO6 (PWM) ─────────────▶ PWMA          ──▶ AO1/AO2 ──▶ Motor
GPIO_DIR_A  ─────────────▶ AIN1
GPIO_DIR_B  ─────────────▶ AIN2
3.3V        ─────────────▶ VCC (logic)
VMOTOR 5V   ─────────────▶ VM  (motor)
GND         ─────────────▶ GND, STBY
```

> **Note**: the current firmware model only computes `omega` internally. The next development step is adding the PWM HAL that converts `torque → duty cycle` and writes it to `hardware_pwm`. This falls under **Phase 7 / actuator HAL**.

**Why not use a servo or ESC directly?**
- Hobby servos/ESCs use 50 Hz PWM with 1–2 ms pulses → requires extra logic
- TB6612 accepts the native high-frequency PWM of the RP2350 (up to ~125 kHz) → simpler and closer to real control

### 8.2 Magnetorquers — lab options

**Firmware interface:**
- PWM control on `GPIO14` (X), `GPIO15` (Y), `GPIO16` (Z)
- Firmware: `magnetorquer_set_moment(mq, mx, my, mz)` → sets magnetic dipole [A·m²]
- B×L desaturation is implemented in `src/services/adcs/momentum_dump.c` (Phase 5)

**Lab components (buy now):**

| Component | Suggested model | Price | Function |
|-----------|----------------|-------|---------|
| Bidirectional H-bridge driver | DRV8833 (module) or L9110S | ~$1–2 each | Reverses current through coil (dipole +/−) |
| Electromagnetic coil | Homemade ferrite-core coil (AWG28, 300 turns) | ~$2–4 each | Generates magnetic dipole proportional to current |
| Ferrite core | MnZn bar 8×70 mm | ~$1 each | Increases permeability → more moment per turn |

**Wiring diagram — Magnetorquer × 1 axis (repeat × 3):**
```
Pico 2W                  DRV8833             Coil
GPIO14 (PWM) ──────────▶ AIN1 (or IN1)
GPIO_DIR     ──────────▶ AIN2 (or IN2)  ──▶ AOUT1/AOUT2 ──▶ Coil
3.3V         ──────────▶ VCC
GND          ──────────▶ GND
```

> Current direction determines dipole polarity (+/−) → firmware must be able to invert the moment sign for the B×L dump.

**What can be tested in the lab with this:**
- ✅ Full ADCS loop: EKF → LQR → `magnetorquer_set_moment()` → real current in coil
- ✅ Verify that B×L dump generates current proportional to the magnetic field measured by HMC5883L
- ✅ Measure generated field with the system's own magnetometer (true closed loop)
- ⚠️ Does not simulate real orbital torque (Earth field ~50 µT vs. lab with EMI)

**Actuator acquisition plan summary:**

| Stage | What to buy | Est. cost | When |
|-------|------------|-----------|------|
| **Lab now** | 3× Motor N20 + 3× TB6612 + 3× DRV8833 + AWG28 wire + ferrite | ~$30–40 total | Now |
| **Flight** | Custom BLDC reaction wheels + custom ferrite coils | To be quoted | Final HW phase |

---

## 9. EPS — Electrical Power System

| # | Component | P/N / Model | Qty | Status | Notes |
|---|-----------|-------------|-----|--------|-------|
| EPS-1 | LiPo Battery | **18650 1S 3.7V 3000–3500 mAh** (e.g. Samsung 30Q, Panasonic NCR18650B) | 1–2 | 🔄 **Buy now** | Main bus; see §9.1 |
| EPS-2 | 5V Boost converter | **MT3608** or XL6009 (module) | 1 | 🔄 **Buy now** | LiPo 3.7V → 5V bus; see §9.1 |
| EPS-3 | LiPo Charger | **TP4056** with protection (micro-USB module) | 1 | 🔄 **Buy now** | Charges 1S from USB or solar panel; see §9.1 |
| EPS-4 | Solar panel (lab) | Monocrystalline **6V 1W** (Vmpp ≈ 5.5V, Isc ≈ 200 mA, 135×110 mm) | 1 | 🔄 **Buy now** | Tests TP4056 charging circuit; add 1N5819 Schottky in series; see §9.2 |
| EPS-6 | Anti-backflow Schottky diode | **1N5819** (Vf ≈ 0.3V @ 200 mA, Vr = 40V) | 1 | 🔄 **Buy now** | Prevents battery discharge through panel at night; place between panel and TP4056 Vin |
| EPS-5 | Battery voltage sensor | ADC0 on GPIO26 (resistor divider) | 1 | ✅ Integrated | Driver already in firmware; see `config/pico_pins.h` |

### 9.1 Bus voltage selection — Analysis

**Recommendation: Regulated 5V bus from LiPo 1S (3.7V)**

#### Voltages in play

| Component | Operating voltage | Compatible with 5V bus |
|-----------|-----------------|----------------------|
| Pico 2W (VSYS) | **1.8–5.5V** → internal LDO → 3.3V | ✅ Connect VSYS to 5V bus |
| MPU-6050 (GY-521) | 3.3–5V (onboard regulator) | ✅ |
| HMC5883L (GY-271) | 3.3V (onboard regulator) | ✅ |
| GPS NEO-7M | 3.3–5V (onboard regulator) | ✅ |
| HC-12 | **3.2–5.5V** | ✅ |
| Motor N20 (via TB6612) | VM: **2.5–13.5V** → best at 5V | ✅ More torque at 5V |
| Ferrite coils (via DRV8833) | VM: 0–10.8V → designed at 3.3V | ✅ Also works at 5V |
| E22-400M30S (flight) | 3.3–5.5V | ✅ |

> **Conclusion**: all current and planned components accept 5V. The Pico 2W regulates internally to 3.3V for its logic, and sensor modules have their own onboard regulators.

#### Why 5V and not 3.3V?

| Criterion | 3.3V bus | **5V bus** |
|-----------|---------|-----------|
| N20 motor torque | ⚠️ Reduced (~60% of nominal) | ✅ Nominal |
| Regulation margin | ❌ LiPo 3.7V → must step down → losses | ✅ LiPo 3.7V → boost to 5V → efficient |
| Pico 2W | ✅ Works (VSYS min 1.8V) | ✅ Works better |
| Simplicity | ❌ Needs LDO buck anyway | ✅ Single boost converter |
| Component compatibility | ✅ All work | ✅ All work |

#### Why 1S LiPo (3.7V) and not 2S (7.4V)?

- 2S requires a higher power buck regulator to step down to 5V → more components
- 1S + MT3608 boost to 5V is the simplest and most efficient circuit for < 1A
- 1S cell voltage (3.0–4.2V) always within the Pico VSYS range

#### Recommended EPS architecture

```
                   TP4056 (charger)
USB/Solar 5V  ──▶  ├── CHRG/STDBY LED
                   └── BAT+ / BAT-
                          │
                   LiPo 18650 1S
                   3.0V – 4.2V
                          │
                   MT3608 boost
                   3.7V → 5V @ 2A
                          │
              ┌───────────┴─────────────────┐
              │                             │
         5V BUS                         Resistor divider
              │                         (R1=330kΩ, R2=100kΩ)
    ┌─────────┼──────────────┐               │
    │         │              │           GPIO26 (ADC0)
  VSYS      VM TB6612      VM DRV8833    → Pico reads Vbatt
  (Pico)   (RW motors)   (magnetorquers)
    │
  LDO 3.3V (internal Pico)
    │
  GPIO / I2C / UART (all sensors)
```

#### Resistor divider for Vbatt reading

The Pico ADC measures up to 3.3V. The battery can be between 3.0V–4.2V:

$$V_{ADC} = V_{batt} \times \frac{R_2}{R_1 + R_2}$$

With $R_1 = 330\,\text{k}\Omega$ and $R_2 = 100\,\text{k}\Omega$:

$$V_{ADC} = 4.2 \times \frac{100}{430} \approx 0.98\,\text{V} \quad \checkmark \text{ (within ADC range)}$$

> Resistor values to be added in §10 Miscellaneous.

#### Estimated system power consumption and autonomy

**Eclipse calculation for SSO 600 km (worst case, $\beta = 0°$):**

$$t_{eclipse} = T_{orbit} \times \frac{\arccos\sqrt{1-\left(\frac{R_E}{R_E+h}\right)^2}}{\pi} = 96.7 \times \frac{66.1°}{180°} \approx 35.5 \text{ min}$$

> ✅ **Our original ~35 min estimate was correct.** SSO parameters confirm the EPS sizing.
> Note: during periods with $|\beta| > 66°$ there is no eclipse (continuous sunlight).

| Subsystem | Component | Typical current @ 5V |
|-----------|-----------|---------------------|
| OBC | Pico 2W | ~80 mA |
| IMU | MPU-6050 | ~4 mA |
| Magnetometer | HMC5883L | ~1 mA |
| GPS | NEO-7M | ~45 mA |
| TT&C | HC-12 TX active | ~80 mA (TX) / 13 mA (RX) |
| RW motors | 3× Motor N20 @ 5V | ~150–300 mA |
| Magnetorquers | 3× ferrite coil | ~240 mA |
| **Maximum total** | **(all active)** | **~700 mA @ 5V = 3.5W** |
| **Nominal total** | **(active control, RX comms)** | **~400 mA @ 5V = 2W** |

**Autonomy with 18650 3000 mAh @ 3.7V = 11.1 Wh:**

| Scenario | Consumption | Estimated autonomy |
|----------|-------------|-------------------|
| Nominal (all active) | 2W | ~5.5 hours |
| Maximum (full actuators) | 3.5W | ~3 hours |
| OBC + comms only (idle) | 0.5W | ~22 hours |

> For LEO flight (~90 min orbit), with 1W solar and 2W nominal consumption, the battery must be sized to cover the eclipse (~35 min). See §9.2 for solar panel analysis.

### 9.2 Solar panel — Analysis and sizing

#### Lab panel (prototype)

**Lab panel objective**: validate the TP4056 charging circuit, not power the full system.

| Parameter | Value |
|-----------|-------|
| Model | Monocrystalline 6V 1W, 135×110 mm |
| Vmpp | ~5.5V |
| Impp | ~182 mA |
| Voc (open circuit) | ~7.2V |
| Isc (short circuit) | ~200 mA |
| TP4056 compatibility | ✅ TP4056 Vin max = 8V; Vmpp under load ≈ 5.5V ✅ |

**Wiring diagram:**
```
Solar panel 6V 1W
   (+)──▶ 1N5819 ──▶ Vin TP4056 ──▶ BAT+ 18650
   (−)──────────────▶ GND TP4056
```
> The 1N5819 diode (Vf ≈ 0.3V) prevents the battery from discharging through the panel in darkness.
> With diode: Vin_TP4056 = Vmpp − 0.3V ≈ 5.2V → within operating range ✅

**Energy balance in lab (outdoor, sunny day):**

| Scenario | Solar generation | System consumption | Balance |
|----------|-----------------|-------------------|---------|
| System inactive (charge only) | 1W × 5h sun = 5 Wh | ~0.1W (OBC idle) | +4.5 Wh → charges battery |
| Nominal system (all active) | 1W × 6h = 6 Wh | 2W × 6h = 12 Wh | −6 Wh → battery drains |
| **Conclusion** | Battery is the primary power source in lab. Panel only reduces drain. For continuous operation: power via USB. | | |

#### Flight LEO panel sizing (reference)

Based on SSO 600 km parameters (§0):

$$P_{panel} \geq \frac{P_{load} \times T_{orbit}}{\eta_{conv} \times T_{sun}} = \frac{2\,\text{W} \times 98.6\,\text{min}}{0.70 \times 63.1\,\text{min}} \approx 4.5\,\text{W raw}$$

Where:
- $T_{sun} = T_{orbit} - t_{eclipse} = 98.6 - 35.5 = 63.1$ min/orbit
- $\eta_{conv}$ = 0.70 (boost + charger efficiency)
- Nominal load: 2W

| Panel configuration | Typical power | Sufficient for flight? |
|--------------------|--------------|------------------------|
| 1U, 1 face (10×10 cm, GaAs 28%) | ~1.5W | ❌ Insufficient |
| 1U, 2 opposite faces | ~3W | ⚠️ Marginal |
| 1U, 4 lateral faces | ~4–5W | ✅ Sufficient (nominal) |
| **Flight recommendation** | **4 faces × 1.5W = 6W raw** | ✅ +33% margin |

> **Current status**: flight panel is `🔄 Planned`. Flight solar cells (triple-junction GaAs or space-grade monocrystalline) are long lead-time components and will be sourced in the final HW phase.

---

## 10. Miscellaneous / Passives

| # | Component | P/N / Model | Qty | Status | Notes |
|---|-----------|-------------|-----|--------|-------|
| 9 | I2C pull-up resistors | 4.7 kΩ 0402 | 4 | 🔄 Planned | For SDA/SCL of I2C0 and I2C1; **confirmed required** — MPU-6050 and HMC5883L/LIS3MDL both need explicit pull-ups (§3.1) |
| 10 | Debug connector | Micro-USB or USB-C | 1 | ✅ Integrated | USB CDC enabled in firmware |
| 11 | Vbatt resistor divider | R1 = 330 kΩ, R2 = 100 kΩ (¼ W) | 2 | 🔄 Planned | Vbatt reading on ADC0/GPIO26; V_ADC = V_batt × 0.23 |
| 12 | External watchdog | TPS3431 (or MCP1316, MAX706) | 1 | 🔄 Planned | GPIO20 (placeholder in `pico_pins.h`); triggers hardware reset if firmware hangs; critical for SAFE MODE recovery in LEO; ~$1–2 |

---

## 11. Pin Assignment Summary (RP2350 / Pico 2W)

```
GPIO0  — UART0 TX  → 🔄 Remap to GPS TX (debug → USB CDC)
GPIO1  — UART0 RX  → 🔄 Remap to GPS RX
GPIO2  — I2C1 SDA  (future expansion)
GPIO3  — I2C1 SCL  (future expansion)
GPIO4  — I2C0 SDA  / UART1 TX  ← MPU6050 + HMC5883L; UART1 = CSP TT&C
GPIO5  — I2C0 SCL  / UART1 RX  ← MPU6050 + HMC5883L; select one
GPIO6  — PWM3A  → RW Motor 1
GPIO7  — PWM3B  → RW Motor 2
GPIO8  — PWM4A  → RW Motor 3
GPIO14 — PWM7A  → Magnetorquer X
GPIO15 — PWM7B  → Magnetorquer Y
GPIO16 — PWM0A  → Magnetorquer Z
GPIO20 — External watchdog (placeholder)
GPIO25 — Onboard status LED
GPIO26 — ADC0   → Battery voltage sensor
GPIO27 — ADC1   → Temperature sensor (optional external)
ADC4   — RP2350 internal temperature
```

> **Note**: GPIO4/GPIO5 are mapped to both I2C0 and UART1. The current firmware
> activates I2C0 for sensors and UART1 for CSP. Do not use simultaneously.

---

## 12. Open items and decisions

- [x] ~~EPS: define LiPo battery, 3.3V regulator, and solar panel~~ — resolved in §9 (v0.5)
- [x] ~~Solar panel: specification and analysis~~ — 6V 1W lab panel + flight sizing in §9.2 (v0.7)
- [x] ~~TT&C antenna: add to BOM~~ — 6c/6d added, λ/4 analysis in §6.1 (v0.7)
- [ ] Confirm amateur license IARU for 435–438 MHz (amateur satellite frequencies)
- [ ] Calculate number of daily passes over GS based on selected ground station latitude
- [ ] Size flight battery: cover 37 min eclipse @ 2W = 1.23 Wh min (+ 50% margin = 1.85 Wh)
- [ ] Size flight solar panel: 4 lateral faces 1U, ~4.5W raw required (§9.2)
- [ ] Source flight solar cells GaAs/Si (Spectrolab, Azur Space — lead time > 6 months)
- [ ] Define flight antenna: λ/4 dipole at 434 MHz (17.3 cm × velocity factor 0.95 ≈ 16.4 cm) + ground plane
- [ ] Resolve GPIO4/GPIO5 assignment: I2C0 and UART1 are separate build configurations or time-multiplexed
- [ ] Add direction GPIOs for TB6612 (RW) and DRV8833 (magnetorquers) in `pico_pins.h`
- [ ] Confirm manufacturers and suppliers (Mouser, DigiKey, AliExpress for prototype)
- [ ] Validate radiation tolerance of components (polar LEO, ~97° orbit, proton and electron fluence)

**PDR Result: ✅ PASS** — Architecture solid, RF design correct (E22 link budget verified at 2300 km slant), ADCS immature but acceptable at PDR gate.

**Pending items for CDR:**
- [ ] CDR: Complete flight EPS design (space-grade solar array, battery sizing, MPPT regulation)
- [ ] CDR: Reaction wheels final specification (BLDC motor, encoder, moment of inertia budget)
- [ ] CDR: OBC PCB design (replace Pico 2W breadboard assembly with custom RP2350 PCB)
- [ ] CDR: Confirm HMC5883L vs LIS3MDL decision and update `src/drivers/mag/` accordingly
- [ ] CDR: Qualify all flight components for TID/SEE radiation environment (polar LEO, ~97° orbit)
- [ ] CDR: Measure WCET of all FreeRTOS tasks using DWT cycle counter and document timing budget

---

## 13. Changelog

| Version | Date | Author | Description |
|---------|------|--------|-------------|
| 0.1 | 2026-03-05 | — | Initial creation; GPS GY-NEO6Mv2 evaluated; attitude sensors documented |
| 0.2 | 2026-03-05 | — | TT&C transceiver analyzed; E22-400M30S recommended; LORA32U4 II → GS |
| 0.3 | 2026-03-05 | — | HC-12 Si4463 433 MHz evaluated: ✅ GS/development, ❌ LEO flight (link budget −5 dB) |
| 0.4 | 2026-03-05 | — | Magnetorquer: clean ferrite+DRV8833 setup; P20/15 rejected; EPS stub §9 |
| 0.5 | 2026-03-05 | — | Full EPS: 5V bus, LiPo 1S 18650, MT3608 boost, TP4056; autonomy analysis |
| 0.6 | 2026-03-05 | — | SSO orbital parameters added (§0); link budget corrected to 2300 km slant; eclipse ≅ 35.5 min verified |
| 0.7 | 2026-03-05 | — | Solar panel: 6V 1W lab spec + 4.5W flight sizing (§9.2); TT&C antennas added (6c/6d); §5 updated |
| 0.8 | 2026-03-05 | — | Consolidated lab purchase list §14; estimated total ~$125–155 USD |
| 0.9 | 2026-03-05 | — | Full document translated to English; acquisition notes cleaned up |
| 1.0 | 2026-03-05 | — | PDR review incorporated: HMC5883L discontinuation flagged + flight alternatives (§3.1); I2C pull-ups confirmed (§10 #9); TPS3431 watchdog added (§10 #12); SAW filter 433 MHz added (§6e); deployable tape antenna noted (§6d); GS updated — E22+USB-UART promoted as recommended final GS (§7, §7.2); magnetorquers-first ADCS strategy documented (§8); DWT WCET note added (§2); CDR pending items + PDR PASS result added (§12) |

---

## 14. Lab Purchase List — Buy Now

> **Objective**: complete functional lab prototype to validate ADCS, EPS, TT&C, and GPS firmware.
> Flight components (E22, GaAs solar cells, BLDC RW) will be sourced in the final HW phase.

### 14.1 Consolidated table

| # | Subsystem | Component | Model / Specification | Qty | Est. unit price | Subtotal | Suggested source |
|---|-----------|-----------|----------------------|-----|----------------|---------|-----------------|
| 1 | OBC | Raspberry Pi Pico 2W | RP2350, Wi-Fi/BT, 520 KB SRAM | 2 | ~$7 | ~$14 | DigiKey / Mouser / Pi Store |
| 2 | ADCS | IMU MPU-6050 | GY-521 module (I2C, 3.3V) | 2 | ~$2 | ~$4 | AliExpress |
| 3 | ADCS | Magnetometer HMC5883L | GY-271 module (I2C, 3.3V) | 2 | ~$3 | ~$6 | AliExpress |
| 4 | GPS | GPS Module NEO-7M | GY-NEO6Mv2 + ceramic antenna | 2 | ~$8 | ~$16 | AliExpress |
| 5 | TT&C | Transceiver HC-12 | Si4463, 433 MHz FSK, UART TTL | 2 | ~$4 | ~$8 | AliExpress |
| 6 | TT&C | 433 MHz Antenna | Rubber-duck / whip SMA (5–8 dBi) | 4 | ~$1.5 | ~$6 | AliExpress |
| 7 | RW | DC motor with encoder | GA12-N20 6V 100–300 RPM | 3 | ~$4 | ~$12 | AliExpress |
| 8 | RW | H-bridge motor driver | TB6612FNG (breakout module) | 3 | ~$2.5 | ~$7.50 | AliExpress |
| 9 | RW | Inertia disk | Acrylic or aluminum ~5 cm Ø, 5–10 mm | 3 | ~$1 | ~$3 | AliExpress / hardware store |
| 10 | MTQ | H-bridge driver | DRV8833 (module) | 3 | ~$1.5 | ~$4.50 | AliExpress |
| 11 | MTQ | Ferrite core | MnZn bar 8×70 mm | 3 | ~$1.5 | ~$4.50 | AliExpress |
| 12 | MTQ | Copper wire | AWG28 enameled, 50 m spool | 1 | ~$4 | ~$4 | AliExpress / local electronics |
| 13 | EPS | LiPo 18650 battery | Samsung 30Q 3000 mAh, 3.7V 1S | 2 | ~$8 | ~$16 | Local electronics store |
| 14 | EPS | 5V Boost converter | MT3608 module (up to 28V, 2A) | 2 | ~$1 | ~$2 | AliExpress |
| 15 | EPS | LiPo charger | TP4056 with protection IC (micro-USB) | 2 | ~$1 | ~$2 | AliExpress |
| 16 | EPS | Solar panel | Monocrystalline 6V 1W (135×110 mm) | 1 | ~$4 | ~$4 | AliExpress |
| 17 | EPS | Schottky diode | 1N5819 (DO-41, Vf ≈ 0.3V), ×10 pack | 1 | ~$1 | ~$1 | AliExpress |
| 18 | Misc | I2C pull-up resistors | 4.7 kΩ ¼W (pack × 100) | 1 | ~$1 | ~$1 | AliExpress |
| 19 | Misc | Vbatt divider resistors | R1 = 330 kΩ + R2 = 100 kΩ ¼W (pack) | 1 | ~$1 | ~$1 | AliExpress |
| 20 | Misc | Breadboard | 830-point (half size) | 2 | ~$3 | ~$6 | AliExpress / local electronics |
| 21 | Misc | DuPont jumper wires | M-M / M-F / F-F 20 cm (120 pcs kit) | 1 | ~$2 | ~$2 | AliExpress |
| 22 | Misc | 18650 battery holder | With switch and JST connector | 1 | ~$1.5 | ~$1.5 | AliExpress |
| | | | | | **TOTAL estimated** | **~$125–155 USD** | |

> Price range depends on supplier and shipping cost. Estimated **$90–110 USD** excluding shipping (AliExpress standard shipping).

### 14.2 Purchase priorities by development phase

| Priority | Items | Unlocked by | Est. cost |
|----------|-------|-------------|-----------|
| **1 — Immediate** (firmware already works) | #1 Pico 2W, #2 GY-521, #3 GY-271 | Nothing — firmware integrated | ~$24 |
| **2 — Comms lab** | #5 HC-12 × 2, #6 antennas × 4 | CSP/KISS ready (UART1) | ~$14 |
| **3 — EPS lab** | #13 LiPo 18650, #14 MT3608, #15 TP4056, #16 solar panel, #17 diode, #22 battery holder | EPS §9 analyzed | ~$26 |
| **4 — Actuators lab** | #7–9 RW + TB6612, #10–12 MTQ + DRV8833 + ferrite + wire | Phase 7 actuator HAL (pending) | ~$36 |
| **5 — GPS** | #4 GY-NEO6Mv2 × 2 | Requires freeing UART0 (GPS driver pending) | ~$16 |
| **6 — Miscellaneous** | #18–21 resistors, breadboard, wires | Always useful | ~$10 |

### 14.3 Acquisition notes

- **LiPo battery**: LiPo cells have shipping restrictions with most carriers — buy locally from an electronics store or hobby shop. Always buy cells with overcharge protection (the TP4056 circuit already provides this, but double protection is safer).
- **Pico 2W**: available on Mouser/DigiKey (~$7 USD + shipping) or from authorized Raspberry Pi distributors. Verify it is the **2W** version (RP2350 with Wi-Fi), not the Pico 1 (RP2040) or Pico 2 (without Wi-Fi).
- **HC-12 vs E22**: buy HC-12 now for lab. The KISS/CSP firmware is identical — when E22 modules arrive for flight, only the hardware changes, not the code.
- **GA12-N20**: specify 6V and RPM ≤ 300 when ordering. The N20 at 3V or 12V is physically identical but has different motor constants — incorrect for our PWM operating point.
