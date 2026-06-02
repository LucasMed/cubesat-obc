# HW-CAM-001 — OV2640 Camera Hardware Integration Procedure

| Field           | Value                                        |
|-----------------|----------------------------------------------|
| Document ID     | HW-CAM-001                                   |
| Version         | 1.0                                          |
| Date            | 2026-05-28                                   |
| Author          | CubeSat OBC Team                             |
| Status          | Draft                                        |
| Classification  | Internal                                     |

## 1. Scope

This document defines the step-by-step hardware integration procedure for
connecting the **Arducam Mini OV2640 2MP Camera Module** to the CubeSat OBC
(Raspberry Pi Pico 2W / RP2350).

It covers:
- Electrical connections (pin map, wiring)
- Power-on sequence and verification
- First capture test
- Troubleshooting

Reference documents:
- ICD-PAYLOAD-001 §4, §5, §11
- PAYLOAD-SPEC-001 §5
- `config/pico_pins.h`
- `mechanical/hexsat130_v1.scad` (camera_mount_lateral module)

---

## 2. Required Components

### 2.1 Bill of Materials

| Item | Description | Qty | Notes |
|:-----|:------------|:----|:------|
| **CAM-001** | Arducam Mini OV2640 2MP SPI camera module (M12 mount) | 1 | Verify it has the AL422B FIFO bridge |
| **J1** | 2×7 pin header (2.54 mm pitch) | 1 | For Arducam module interface |
| **J2–J4** | Dupont female-to-female jumper wires, ~100 mm | 7 | For initial breadboard testing |
| **C1** | 100 nF ceramic capacitor (0603 or DIP) | 1 | Camera module decoupling |
| **C2** | 10 µF electrolytic capacitor | 1 | Bulk decoupling |
| **—** | M2 × 8 mm nylon screws + nuts | 4 | Camera PCB mounting |
| **—** | M3 × 10 mm nylon standoffs + screws | 2–4 | Camera bracket to chassis |

### 2.2 Test Equipment

| Item | Purpose |
|:-----|:--------|
| USB oscilloscope (or logic analyser) | Verify I2C and SPI signals |
| Multimeter | Continuity and voltage checks |
| USB-UART adapter (CP2102 / FTDI) | Monitor OBC debug output (UART1) |
| Bench power supply (3.3 V, 500 mA) | Power module independently for first test |

---

## 3. Electrical Interface — Pin Map

### 3.1 Arducam Mini Module Pinout

**8-pin SPI + I2C module** (no RESET, TRIG, or FIFO_RDY pins).
Reset handled via SCCB software register (0x12 = 0x80); capture completion
polled via SPI ARDUCHIP_STATUS register (0x07, bit 0).

Verify your module's pin numbering against the table below — some revisions
reverse the pin order.

```
 Top view (looking at lens):
 ┌──────────────────┐
 │ 1 2 3 4 5 6 7 8  │
 └──────────────────┘
```

| Pin | Signal | Direction | OBC GPIO | Notes |
|:----|:-------|:----------|:---------|:------|
| 1   | **CS** | OBC → CAM | GPIO14   | SPI chip select (active low) |
| 2   | **MOSI** | OBC → CAM | GPIO19 | SPI data to camera |
| 3   | **MISO** | CAM → OBC | GPIO17 | SPI data from camera |
| 4   | **SCK** | OBC → CAM | GPIO18  | SPI clock (10 MHz max) |
| 5   | **GND** | —         | GND      | Power ground |
| 6   | **VCC** | —         | 3.3 V    | Module power (3.3 V, 60 mA typ) |
| 7   | **SDA** | OBC ↔ CAM | GPIO2    | I2C1 data (SCCB register config) |
| 8   | **SCL** | OBC ↔ CAM | GPIO3    | I2C1 clock (SCCB register config) |

> **⚠️ CRITICAL**: Always check continuity with a multimeter before applying
> power. Some Arducam revisions have a 2×4 header with staggered numbering.

### 3.2 Power Connections

```
OBC 3.3 V rail ──┬── C2 (10 µF) ── GND
                  │
                  ├── C1 (100 nF) ── GND
                  │
                  └── CAM VCC (pin 6)
```

Decoupling capacitors should be placed as close to the camera module header as
possible.

### 3.3 Payload Power Rail

The camera's 3.3 V supply is derived from the PAYLOAD_5V rail through an
on-module LDO regulator. The 5 V rail is switched by GPIO21 (PAYLOAD_ENABLE):

```
GPIO21 ─── 5 V rail MOSFET gate ─── 5V_OUT ─── CAM module LDO ─── 3.3 V
```

Before the module receives power, `PAYLOAD_ENABLE` must be asserted (high) via
`payload_manager_enable(true)`. The driver's `camera_init()` must be called
at least 50 ms after rail enable to allow the regulator to stabilise.

---

## 4. Step-by-Step Integration

### 4.1 Bench-Level Connection (Breadboard)

> Perform this step BEFORE installing the camera in the mechanical mount.

1. **Power off** all equipment.
2. Connect the 8 wires between the OBC and Arducam module per §3.1:

   | Wire | OBC Header Pin → CAM Pin |
   |:-----|:-------------------------|
   | CS   | OBC GPIO14  → CAM pin 1 |
   | MOSI | OBC GPIO19  → CAM pin 2 |
   | MISO | OBC GPIO17  → CAM pin 3 |
   | SCK  | OBC GPIO18  → CAM pin 4 |
   | GND  | OBC GND     → CAM pin 5 |
   | VCC  | OBC 3.3 V   → CAM pin 6 |
   | SDA  | OBC GPIO2   → CAM pin 7 |
   | SCL  | OBC GPIO3   → CAM pin 8 |

3. **Double-check continuity** with a multimeter on each connection. Ensure
   no shorts between adjacent pins.
4. Add decoupling capacitors (C1, C2) between VCC and GND as close to the
   camera header as possible.
5. Verify the OBC is NOT powered. Connect the camera module.
6. Power on the OBC.

### 4.2 First Power-On — I2C Scan

After connecting and powering on:

1. Flash the firmware with payload task support (already in `obc_main.c`).
2. Connect to the OBC debug UART (UART1, GPIO8/9 → USB-UART → PC).
3. Observe boot messages. The camera init will attempt to read the OV2640
   chip ID (0x0A → 0x26, 0x0B → 0x42).

Expected debug output:
```
[PayloadTask] Started
[CamInit] OV2640 detected (PID=0x2642)
```

If the chip ID is not detected, the camera_init() returns false, and the
boot sequence continues without camera support. See §6 (Troubleshooting).

### 4.3 SPI Verification

Once I2C communication is verified, test the Arducam SPI register access:

```c
// Write test pattern to Arducam TEST register (0x00)
cam_spi_transfer(ARDUCHIP_TEST1, 0x55);

// Read back
uint8_t test = cam_spi_transfer(ARDUCHIP_TEST1 | 0x80, 0x00);
// Expected: test == 0x55
```

On an oscilloscope, you should see:
- CS (GPIO14) pulled low for ~2 µs
- SCK toggling at 1 MHz (init baud rate)
- MOSI sending address byte + data byte
- MISO returning the previous register value

### 4.4 First Capture Test

```c
// Enable payload rail
payload_manager_enable(true);
sleep_ms(50);

// Initialize camera (writes all init + JPEG registers)
if (camera_init()) {
    // Trigger capture with 5 second timeout
    if (camera_capture(5000)) {
        uint32_t len = camera_get_fifo_length();
        printf("FIFO length: %lu bytes\n", len);

        if (len > 0 && len < 200000) {
            uint8_t buf[128 * 1024];
            size_t read_len = (len > sizeof(buf)) ? sizeof(buf) : len;
            if (camera_read_fifo_burst(buf, read_len)) {
                // First 2 bytes of JPEG: 0xFF 0xD8 (SOI marker)
                printf("First bytes: 0x%02X 0x%02X 0x%02X 0x%02X\n",
                       buf[0], buf[1], buf[2], buf[3]);
            }
        }
    }
}
```

A successful capture produces:
- FIFO length > 20000 bytes (for typical UXGA JPEG)
- First bytes: `0xFF 0xD8 0xFF 0xE0` (JPEG SOI + APP0 marker)

### 4.5 Mechanical Mounting

After bench verification:

1. Print or machine the camera bracket per `hexsat130_v1.scad` → `camera_mount_lateral()`.
2. Mount the OV2640 PCB to the bracket using 4× M2 nylon screws.
3. Verify the lens barrel protrudes through the bracket's window cutout.
4. Mount the bracket to the side panel standoffs using 2× or 4× M3 screws.
5. Route the ribbon cable flat against the panel (avoid sharp bends).
6. Secure wiring with Kapton tape to prevent chafing during vibration.

---

## 5. Verification Checklist

| Step | Check | Method | P/F |
|:-----|:------|:-------|:----|
| 1 | Continuity: all 11 signal pins | Multimeter | □ |
| 2 | No shorts: VCC ↔ GND | Multimeter (power off) | □ |
| 3 | 3.3 V at camera VCC pin | Multimeter (power on) | □ |
| 4 | I2C1 scan detects device at 0x30 | I2C scan tool / firmware | □ |
| 5 | Chip ID reads 0x2642 | camera_init() returns true | □ |
| 6 | Arducam SPI test register works | Write 0x55, read back | □ |
| 7 | Capture triggers and fills FIFO | camera_capture() returns true | □ |
| 8 | FIFO length > 0 | camera_get_fifo_length() | □ |
| 9 | JPEG SOI marker (0xFF 0xD8) | Inspect first 2 bytes of FIFO data | □ |
| 10 | Image stores to SD/flash | storage_write_image() succeeds | □ |

---

## 6. Troubleshooting

### 6.1 Camera Not Detected (I2C NACK)

| Possible Cause | Check |
|:---------------|:------|
| SDA/SCL swapped | Verify GPIO2 (SDA) → CAM pin 7, GPIO3 (SCL) → CAM pin 8 |
| Missing pull-ups | RP2350 has internal pull-ups (enabled by driver), but external 4.7 kΩ may help |
| Wrong I2C address | OV2640 SCCB address is 0x30 (7-bit). Scan all addresses. |
| Module not powered | Verify 3.3 V on CAM pin 6 |
| SW reset failed | Driver does SCCB reset (0x12 = 0x80) — verify I2C communication first |

### 6.2 SPI Communication Failure

| Possible Cause | Check |
|:---------------|:------|
| CS not toggling | Verify GPIO14 is configured as output, check with logic analyser |
| MISO/MOSI swapped | MISO = GPIO17 (CAM → OBC), MOSI = GPIO19 (OBC → CAM) |
| Baud rate too high | Driver init uses 1 MHz; try 500 kHz for debugging |
| Wrong SPI mode | Arducam uses SPI Mode 0 (CPOL=0, CPHA=0) |

### 6.3 Capture Timeout

| Possible Cause | Check |
|:---------------|:------|
| SPI status poll timeout | Driver polls ARDUCHIP_STATUS (0x07) bit 0 — verify SPI register access works |
| Exposure too long | Dark scene may cause long AEC settling time; increase timeout or add light |
| FIFO not clearing | `camera_clear_fifo()` must be called before each capture |

### 6.4 Corrupted / All-White Image

| Possible Cause | Check |
|:---------------|:------|
| JPEG init not written | `camera_init()` must complete before capture |
| Wrong resolution table | `camera_set_resolution()` after init to select size |
| SPI clock too high | Use 1 MHz for debug; raise to 10 MHz after verified |
| Module 5V rail unstable | Check PAYLOAD_ENABLE timing; add 100 ms delay after enable |

### 6.5 GPIO10 (FIFO_RDY) Shared with RW_MOTOR1

GPIO10 is shared between CAM_FIFO_RDY (input) and RW_MOTOR1_PWM (output).
This is **safe** on the RP2350 because:

- GPIO function mux is independent per peripheral: a PWM slice can output on
  a pin while the GPIO input path reads the pin's digital level.
- No conflicts occur as long as the PWM slice is configured for output and
  the GPIO pad is in input mode (the pad is always readable).

**If CAM_FIFO_RDY stays low during capture tests:**
1. Check that no PWM peripheral is configured on GPIO10 during payload mode.
2. Verify the camera module's FIFO_RDY pin is connected.
3. Temporarily use a scope on GPIO10 to see if the camera asserts the signal.

---

## 7. OV2640 Configuration Reference (Driver Register Tables)

The driver includes pre-verified register tables from the ArduCAM SDK:

| Table | Purpose | Registers |
|:------|:--------|:----------|
| `OV2640_JPEG_INIT` | Full sensor init (PLL, AEC/AGC, AWB, gamma, LENC, colour) | ~170 entries |
| `OV2640_JPEG` | Switch DSP output to JPEG format | 6 entries |
| `OV2640_*x*_JPEG` (×9) | Resolution-specific window & DSP registers | ~40 entries each |

These are sourced from the ArduCAM `ov2640_regs.h` and the ESP32 camera
component `ov2640_settings.h`. See `src/drivers/payload/camera_driver.c`
for the full register tables.

**Key registers for debugging:**

| Register | Name | Typical Value | Meaning |
|:---------|:-----|:--------------|:--------|
| `0xFF` = `0x00` | BANK_SEL = DSP | — | Select DSP register bank |
| `0xFF` = `0x01` | BANK_SEL = SENS | — | Select sensor register bank |
| `0x0A` / `0x0B` | PIDH / PIDL | 0x26 / 0x42 | Manufacturer chip ID |
| `0x12` | COM7 | 0x40 | UXGA, output YUV (sensor) |
| `0xDA` | IMAGE_MODE | 0x10 | JPEG output format (DSP) |
| `0xD3` | R_DVP_SP | 0x04 | DVP speed control |

---

## 8. Related Documents

| Document | Description |
|:---------|:------------|
| ICD-PAYLOAD-001.md | Payload interface control document |
| PAYLOAD-SPEC-001.md | Payload instrument specification |
| ITP-OBC-001.md | Integration test plan |
| `config/pico_pins.h` | GPIO pin definitions |
| `src/drivers/payload/camera_driver.c` | Camera driver source |
| `tests/unit/test_camera_driver.c` | Unit tests |
| `tests/integration/test_payload_integration.c` | Integration tests |
| `mechanical/hexsat130_v1.scad` | Mechanical model (camera mount) |
