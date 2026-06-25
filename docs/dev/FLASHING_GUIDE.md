# Flashing Guide — Combined UF2 (Bootloader + Firmware)

## Overview

The **combined UF2** (`cubesat_obc_combined.uf2`) contains both the bootloader
(at `0x10000000`) and the Slot A firmware (at `0x10010000`) in a single flash
operation. This is the recommended deployment artifact.

See [MEMORY_MAP.md](MEMORY_MAP.md) for the full flash layout.

---

## Build the Combined UF2

### Via CI pipeline

```bash
bash scripts/pico_ci.sh pico-build       # cubesat_obc_pico.uf2
bash scripts/pico_ci.sh bootloader-build # bootloader + combined UF2
```

Output goes to `artifacts/`:

```
artifacts/cubesat_obc_combined.uf2   ← Flash this
```

### Manual build

```bash
# 1. Configure (once)
cmake -B build_pico -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DPICO_ENABLED=ON -DPICO_SDK_PATH=$PICO_SDK_PATH -DPICO_BOARD=pico2_w

# 2. Build firmware + bootloader
cmake --build build_pico --target cubesat_obc_pico -j$(nproc)
cmake --build build_pico --target cubesat_obc_bootloader -j$(nproc)

# 3. Combine into single UF2
python3 scripts/combine_uf2.py \
  build_pico/bootloader/cubesat_obc_bootloader.uf2 \
  build_pico/src/cubesat_obc_pico.uf2 \
  cubesat_obc_combined.uf2
```

---

## Flashing to Pico 2W

### Method A: USB Drag-and-Drop (Easiest)

1. **Hold BOOTSEL button** on Pico 2W
2. **Connect Pico 2W to USB** (while holding BOOTSEL)
   - Pico will appear as **RPI-RP2** mass storage
3. **Copy the combined UF2**:
   ```bash
   cp cubesat_obc_combined.uf2 /media/$USER/RPI-RP2/
   ```
4. **Pico reboots automatically** ✅
   - Bootloader runs CRC32 validation on Slot A
   - If Slot A is valid → jumps to firmware
   - First boot uses **trust-on-first-boot** (valid ARM vector table → jump)

### Method B: picotool

```bash
picotool load -x cubesat_obc_combined.uf2
```

### Method C: Flash only the bootloader

```bash
picotool load -x build_pico/bootloader/cubesat_obc_bootloader.uf2
```

> Use this when updating the bootloader independently. The bootloader will
> detect the existing firmware in Slot A via its vector table check.

---

## Verifying the Boot Flow

### Serial console

Connect at 115200 baud (the bootloader emits no serial output by default).
Once the firmware starts, you should see FreeRTOS startup messages:

```bash
minicom -D /dev/ttyACM0 -b 115200
# or
screen /dev/ttyACM0 115200
```

### POST code readback (SRAM diagnostics)

If the board does not boot, read the POST code from SRAM (retained across soft
reset). Use a debugger or a small RAM-read helper:

| Address | Content |
|---------|---------|
| `0x20040000` | POST code (see below) |
| `0x20040004` | Valid marker = `0x504F5354` ("POST") |

**POST code values** (`bootloader/bootloader.c`):

| Code | Meaning |
|------|---------|
| `0` | Booted from Slot A OK |
| `1` | Booted from Slot B OK |
| `2` | Booted from golden restore OK |
| `3` | Slot A CRC32 FAIL |
| `4` | Slot B CRC32 FAIL |
| `5` | Golden restore FAIL (SPI/erase error) |
| `6` | Golden restore CRC32 FAIL (halt) |

### Boot metadata (FMM sector, `0x10310000`)

Read back the boot metadata to see failure counts and last boot reason:

```bash
# Using picotool (needs debug build)
picotool info -b
```

---

## Troubleshooting

### Board does not boot (no serial output)

1. Re-flash the combined UF2 via BOOTSEL mode
2. If it still fails, flash the bootloader alone, then the firmware alone
3. Read POST code from `0x20040000` (see above)

### Bootloader not running

- Ensure you flashed the **combined UF2**, not just the firmware UF2
- The firmware alone at `0x10010000` has no bootloader — the chip would try to
  boot from `0x10000000` (unprogrammed = garbage)
- Check that the combined UF2 covers `0x10000000` – `0x1000FFFF` (bootloader region)

### "No USB serial device" (`/dev/ttyACM*` not appearing)

- Ensure USB stdio is enabled in the firmware build
- Try a different USB cable (some are charge-only)
- Check `dmesg | tail` for USB enumeration messages

---

## Related Documentation

- [BUILD_GUIDE.md](BUILD_GUIDE.md) — Build instructions (CI pipeline, individual targets)
- [MEMORY_MAP.md](MEMORY_MAP.md) — Complete flash and SRAM layout
- [`bootloader/bootloader.c`](../../bootloader/bootloader.c) — Boot flow source
- [`scripts/combine_uf2.py`](../../scripts/combine_uf2.py) — UF2 combiner tool

---

**Last Updated**: 2026-06-24  
**Status**: Ready for flashing ✅
