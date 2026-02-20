# Build Guide

**Last Updated**: 2026-02-20

---

## Prerequisites

### Host Build (Linux/macOS)
```bash
# Debian/Ubuntu
sudo apt install cmake build-essential gcc

# macOS
brew install cmake gcc
```

### Pico SDK Build (Cross-compilation)
```bash
# 1. Install ARM cross-compiler
sudo apt install gcc-arm-none-eabi libnewlib-arm-none-eabi

# 2. Clone Pico SDK
git clone https://github.com/raspberrypi/pico-sdk.git /opt/pico-sdk
cd /opt/pico-sdk && git submodule update --init

# 3. Set environment variable
export PICO_SDK_PATH=/opt/pico-sdk
```

---

## Build on Host (Development/Testing)

```bash
cd cubesat-obc
mkdir -p build && cd build
cmake ..
cmake --build . -- -j$(nproc)
```

### Run Unit Tests
```bash
ctest --output-on-failure
# Expected: 3/3 tests passing (100%)
```

### Run Firmware (Host Stub)
```bash
./src/cubesat_obc_firmware
```

---

## Build for Pico 2W

```bash
cd cubesat-obc
rm -rf build && mkdir build && cd build
cmake ..
cmake --build . -- -j$(nproc)
```

### Output Artifacts
- `build/examples/blink_test.uf2` — LED blink validation (536 KB)
- `build/examples/obc_firmware_pico.uf2` — OBC firmware (TBD — pending flash.c fix)

---

## Flash to Pico 2W

### Method A: USB Drag-and-Drop
1. Hold **BOOTSEL** button on Pico 2W
2. Connect USB while holding BOOTSEL
3. Pico appears as `RPI-RP2` mass storage
4. Copy UF2 file:
   ```bash
   cp build/examples/blink_test.uf2 /media/$USER/RPI-RP2/
   ```
5. Pico reboots automatically

### Method B: picotool
```bash
picotool load -x build/examples/blink_test.uf2
```

See [FLASHING_GUIDE.md](FLASHING_GUIDE.md) for detailed troubleshooting.

---

## Build Configurations

| Flag | Default | Description |
|------|---------|-------------|
| `PICO_SDK_PATH` | (not set) | Path to Pico SDK; enables cross-compilation |
| `PICO_BOARD` | `pico2_w` | Target board variant |
| `CMAKE_BUILD_TYPE` | Debug | Use `Release` for optimized builds |

---

## Troubleshooting

| Problem | Solution |
|---------|----------|
| `CMake Error: pico_sdk_import.cmake not found` | Set `PICO_SDK_PATH` environment variable |
| `arm-none-eabi-gcc: not found` | Install `gcc-arm-none-eabi` |
| Tests fail | Ensure host build (no `PICO_SDK_PATH`) |
| UF2 not generated | Ensure Pico SDK build (with `PICO_SDK_PATH`) |

---

## Static Analysis

```bash
# cppcheck
cppcheck --enable=all --suppress=missingInclude src/ include/

# or via script
scripts/static_analysis.sh
```
