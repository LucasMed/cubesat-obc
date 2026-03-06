# Flashing Pico 2W with Blink Test

## Problem Fixed ✅

The LED on **Pico 2W** is controlled through the **CYW43 WiFi chip**, not direct GPIO. The original `blink_test.c` tried to use direct GPIO, which doesn't work.

**Solution**: Use the correct API:
- `cyw43_arch_init()` — Initialize CYW43 architecture
- `cyw43_arch_gpio_put()` — Control LED via CYW43
- `CYW43_WL_GPIO_LED_PIN` — Use WiFi chip LED pin

---

## Build Instructions

### Prerequisites
- Pico SDK installed at `/home/pico-sdk`
- Pico 2W board (with USB cable)

### Step 1: Build
```bash
cd cubesat-obc
rm -rf build && mkdir build && cd build
cmake ..
cmake --build . -- -j$(nproc)
```

**Output**: `build/examples/blink_test.uf2` (536 KB)

---

## Flashing to Pico 2W

### Method A: USB Drag-and-Drop (Easiest)

1. **Hold BOOTSEL button** on Pico 2W
2. **Connect Pico 2W to USB** (while holding BOOTSEL)
   - Pico will appear as **RPI-RP2** drive
3. **Copy the .uf2 file**:
   ```bash
   cp build/examples/blink_test.uf2 /mnt/pico/
   ```
   OR drag-and-drop in file manager
4. **Pico will reboot automatically** ✅

### Method B: picotool (Alternative)

If you have `picotool` installed:
```bash
picotool load -x build/examples/blink_test.uf2
```

---

## Verification

1. Once flashed, the **LED should blink** (250ms on/off cycle for 10 iterations)
2. **Serial output** (via USB):
   - May appear on your terminal if connected
   - Expected messages about CYW43 initialization and blink pattern

---

## Troubleshooting

### LED Not Blinking
- **Check USB power**: Pico 2W may need adequate power
- **Verify BOOTSEL**: Sometimes the button needs to be held longer
- **Re-flash**: Try flashing again, ensuring the file fully copies

### LED Faint or Blinking Slowly
- Could be board revision issue; try faster blink (reduce `sleep_ms()` values)

### Serial Console Not Showing
- USB stdio is enabled; open serial monitor on `/dev/ttyACM?` (on Linux)
- Or check dmesg: `dmesg | tail`

---

## File Locations

- **Source**: `examples/blink_test.c`
- **Build Config**: `examples/CMakeLists.txt`
- **Compiled UF2**: `build/examples/blink_test.uf2`
- **Other formats**: 
  - ELF: `build/examples/blink_test.elf`
  - HEX: `build/examples/blink_test.hex`
  - BIN: `build/examples/blink_test.bin`

---

## Next Steps (After Validation)

Once blink_test works on Pico 2W:

1. ✅ **Task 2.1 Complete**: SDK integration validated
2. **Task 2.2**: Integrate real FreeRTOS kernel
3. **Task 2.3**: Implement I2C drivers (MPU6050, temp sensor)
4. **Task 2.4**: System integration and control loop
5. **Task 2.5**: Hardware validation on Pico 2W

---

**Last Updated**: 2026-02-16  
**Status**: Ready for flashing ✅
