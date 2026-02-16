# Task 2.2b: Hardware Validation Instructions

## Objective
Flash and test `blink_test.uf2` on Pico 2W hardware to validate FreeRTOS kernel integration and CYW43 LED control.

---

## Pre-Flash Checklist

✅ **blink_test.uf2 exists and is valid**:
- Location: `build/examples/blink_test.uf2` (536 KB)
- Format: UF2 firmware image (RP2350-compatible)
- Family: 0xe48bff57 (RP2350)

✅ **FreeRTOS kernel integrated**:
- Real FreeRTOS-Kernel linked (RP2040 port, compatible with RP2350)
- Dual-core SMP configured
- CYW43 LED control enabled

---

## Flashing Procedure (USB Drag-and-Drop)

### Step 1: Mount Pico 2W in BOOTSEL Mode
1. Disconnect Pico 2W from any power/USB
2. Connect Micro-USB cable (leave other end disconnected from computer)
3. **Hold BOOTSEL button** (small black button on board)
4. Connect USB to computer while holding BOOTSEL
5. Release BOOTSEL after 1-2 seconds

**Expected result**: Pico 2W appears as USB mass storage device (e.g., `RPI-RP2` on Linux, `E:` on Windows)

### Step 2: Copy Firmware
```bash
# Linux/macOS
cp build/examples/blink_test.uf2 /media/ljm/RPI-RP2/
# or find mount point: mount | grep RPI-RP2

# Windows (PowerShell)
Copy-Item .\build\examples\blink_test.uf2 E:\
```

**Expected result**: 
- File copy completes (3-5 seconds)
- USB device disconnects automatically
- Pico 2W reboots into firmware

### Step 3: Verify Upload
- Serial output (USB CDC): Should see initialization messages
- **LED**: Green onboard LED should blink at 200ms on / 800ms off pattern
- No errors or device hangs

---

## Expected Behavior

### LED Blink Pattern
- **On**: 200ms (bright green)
- **Off**: 800ms (dark)
- **Frequency**: ~1.1 blinks per second (1000ms cycle)

### Serial Output (via USB CDC at 115200 baud)
```
Pico 2W Blink Test Started
LED initialization: OK
Blinking at 200ms on / 800ms off...
[Repeating blink pattern]
```

---

## Troubleshooting

### LED not blinking
- **Issue**: CYW43 WiFi chip LED not accessible
- **Check**: Board is Pico 2W (has CYW43 chip)
- **Solution**: Verify GPIO initialization in `examples/blink_test.c`

### No USB serial output
- **Issue**: Serial output not detected
- **Check**: USB cable is data cable (not power-only)
- **Try**:
  ```bash
  # Linux: check device
  ls -la /dev/ttyACM*
  screen /dev/ttyACM0 115200
  ```

### Device doesn't respond after flashing
- **Issue**: Firmware crashed or infinite loop
- **Recovery**: Repeat BOOTSEL procedure, re-flash blink_test.uf2

### Pico 2W not recognized by USB
- **Check**: Micro-USB cable is working (try different cable)
- **Check**: BOOTSEL button was held long enough
- **Check**: Hold BOOTSEL for full 2 seconds, then release

---

## Success Criteria ✅

Task 2.2b is complete when:

1. ✅ blink_test.uf2 flashes successfully to Pico 2W
2. ✅ Onboard LED blinks at expected frequency (200ms on / 800ms off)
3. ✅ USB serial output shows initialization messages
4. ✅ NO errors in firmware execution
5. ✅ Device remains stable for at least 30 seconds

---

## FreeRTOS Kernel Status

**Integrated**: Real FreeRTOS kernel from official repository
- **Port**: RP2040 (compatible with RP2350)
- **Configuration**: 
  - Dual-core SMP enabled
  - 32 KB heap
  - 1000 Hz tick rate
  - 5 priority levels

**Note**: Current blink_test does NOT use FreeRTOS tasks (simple GPIO blink for validation). OBC firmware with full task scheduling pending Pico SDK flash.c integration fix.

---

## Next Steps (Task 2.3)

After successful validation:
1. Proceed to Task 2.3: I2C Driver Implementation (MPU6050, temperature sensors)
2. Re-enable OBC firmware compilation after resolving Pico SDK flash.c FreeRTOS integration
3. Create comprehensive system test with all peripherals

---

## Documentation References

- [Flashing Guide](FLASHING_GUIDE.md) - Detailed flashing instructions
- [TASK2_2_PROGRESS.md](TASK2_2_PROGRESS.md) - Task 2.2 overview
- [blink_test.c](../examples/blink_test.c) - Firmware source code
