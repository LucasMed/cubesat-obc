# Build Guide

**Last Updated**: 2026-06-24

---

## Overview

The project targets **Linux** for host builds (development and testing) and can be cross-compiled for Pico 2W (RP2350) when the Pico SDK is available.

| Build target | When to use |
|---|---|
| Linux host (`PICO_ENABLED=OFF`) | Day-to-day development, unit tests, CI |
| Pico 2W cross-compile | Hardware validation, final firmware |
| Pico 2W bootloader | Dual-slot boot + golden image recovery |
| Combined UF2 | **Single flash artifact**: bootloader + firmware |

---

## CI Pipeline (Recommended)

The CI pipeline (`scripts/pico_ci.sh`) runs the full validation workflow.
Designed for Docker/dev-container but works locally with tools on PATH.

```bash
# Full pipeline (all stages)
bash scripts/pico_ci.sh all

# Individual stages
bash scripts/pico_ci.sh host-test        # 29/29 CTest suite (Linux)
bash scripts/pico_ci.sh pico-build       # Cross-compile cubesat_obc_pico.uf2
bash scripts/pico_ci.sh bootloader-build # Cross-compile bootloader + combined UF2
bash scripts/pico_ci.sh emu-build        # Build RP2040 smoke-test ELF
bash scripts/pico_ci.sh emulate          # rp2040js boot smoke-test
bash scripts/pico_ci.sh static           # clang-format, clang-tidy, cppcheck
bash scripts/pico_ci.sh coverity         # Coverity Scan analysis
bash scripts/pico_ci.sh coverage         # gcovr HTML + text summary
bash scripts/pico_ci.sh sanitize         # AddressSanitizer + UBSan
```

### Pipeline outputs (in `artifacts/`)

```
artifacts/
├── cubesat_obc_pico.uf2             # Pico 2W firmware (Slot A)
├── cubesat_obc_pico.bin / .hex / .elf / .map
├── cubesat_obc_bootloader.uf2       # Bootloader binary
├── cubesat_obc_bootloader.bin / .hex / .elf / .map
├── cubesat_obc_combined.uf2         # ★ Combined: bootloader + firmware (flash this)
├── cubesat_obc_emu.elf              # RP2040 smoke-test emulation
├── test_results/
│   ├── host_tests.xml               # CTest JUnit XML
│   └── host_tests.txt               # CTest console output
├── coverage/                        # gcovr HTML report
├── build.log                        # Full build log
└── emulate_boot.log                 # Emulation output
```

---

## Recommended: VS Code Dev Container

All dependencies are pre-installed inside the container (Pico SDK, ARM GCC,
rp2040js emulator, Coverity).

1. Install [Docker](https://docs.docker.com/get-docker/) and the [Dev Containers extension](https://marketplace.visualstudio.com/items?itemName=ms-vscode-remote.remote-containers)
2. Open the repo in VS Code → **Reopen in Container**
3. Wait for the setup to finish (submodules are initialised and patches applied automatically)
4. Build and test:
   ```bash
   cmake --build build
   ctest --test-dir build --output-on-failure
   ```

---

## Docker (without VS Code)

```bash
bash run_linux.sh build      # configure + build
bash run_linux.sh test       # build + run all 29 tests
bash run_linux.sh coverage   # build + tests + HTML coverage report
bash run_linux.sh shell      # drop into interactive bash shell
bash run_linux.sh clean      # remove build/ directory
```

---

## Native Linux

### Prerequisites

```bash
# Debian/Ubuntu
sudo apt install cmake build-essential ninja-build git
```

### First-time setup

```bash
git clone https://github.com/yourusername/cubesat-obc.git
cd cubesat-obc

# 1. Initialise submodules (libcsp + FreeRTOS-Kernel)
git submodule update --init --recursive

# 2. Apply Linux/POSIX compatibility patches to libcsp (idempotent)
bash scripts/apply_patches.sh
```

### Build

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DPICO_ENABLED=OFF
cmake --build build -j$(nproc)
```

### Run unit tests

```bash
ctest --test-dir build --output-on-failure
# Expected: 100% tests passed, 0 tests failed out of 29
```

### Run firmware (host stub)

```bash
./build/src/cubesat_obc_firmware
```

### AddressSanitizer + UBSan

```bash
cmake -B build_sanitize -G Ninja -DCMAKE_BUILD_TYPE=Debug -DPICO_ENABLED=OFF -DSANITIZE=ON
cmake --build build_sanitize -j$(nproc)
ctest --test-dir build_sanitize --output-on-failure
```

---

## Pico 2W Cross-compilation (Firmware + Bootloader)

### Prerequisites

```bash
# ARM cross-compiler
sudo apt install gcc-arm-none-eabi libnewlib-arm-none-eabi

# Pico SDK
git clone https://github.com/raspberrypi/pico-sdk.git /opt/pico-sdk
cd /opt/pico-sdk && git submodule update --init
export PICO_SDK_PATH=/opt/pico-sdk
```

### Build firmware and bootloader separately

```bash
# Configure once (shared build directory)
cmake -B build_pico -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DPICO_ENABLED=ON -DPICO_SDK_PATH=$PICO_SDK_PATH -DPICO_BOARD=pico2_w

# Build firmware (Slot A)
cmake --build build_pico --target cubesat_obc_pico -j$(nproc)

# Build bootloader
cmake --build build_pico --target cubesat_obc_bootloader -j$(nproc)
```

### Generate combined UF2 (bootloader + firmware)

```bash
python3 scripts/combine_uf2.py \
  build_pico/bootloader/cubesat_obc_bootloader.uf2 \
  build_pico/src/cubesat_obc_pico.uf2 \
  cubesat_obc_combined.uf2
```

The combined UF2 is the **recommended deployment artifact** — flash it once and
both the bootloader (at `0x10000000`) and firmware (at `0x10010000`) are written
to the correct XIP addresses.

### Output artifacts

```
build_pico/src/cubesat_obc_pico.uf2                  # Firmware (Slot A)
build_pico/bootloader/cubesat_obc_bootloader.uf2     # Bootloader
cubesat_obc_combined.uf2                              # Combined (flash this)
```

---

## Flash to Pico 2W

### Method A — USB drag-and-drop
1. Hold **BOOTSEL** button on Pico 2W
2. Connect USB while holding BOOTSEL
3. Pico appears as `RPI-RP2` mass storage
4. Copy the **combined UF2** file:
   ```bash
   cp cubesat_obc_combined.uf2 /media/$USER/RPI-RP2/
   ```
5. Pico reboots automatically → bootloader runs → validates Slot A → jumps to firmware

### Method B — picotool
```bash
picotool load -x cubesat_obc_combined.uf2
```

See [FLASHING_GUIDE.md](FLASHING_GUIDE.md) for detailed troubleshooting.

---

## Emulation smoke-test

The pipeline includes an rp2040js boot smoke-test that verifies the firmware
starts and produces serial output:

```bash
# Install emulator and build
cd docker && npm install && cd ..
bash scripts/pico_ci.sh emu-build    # builds tests/emulation/smoke_test.s
bash scripts/pico_ci.sh emulate      # runs rp2040js, checks boot strings
```

---

## Build CMake flags

| Flag | Default | Description |
|------|---------|-------------|
| `PICO_ENABLED` | `ON` | Set to `OFF` to force Linux host build |
| `PICO_SDK_PATH` | (env) | Path to Pico SDK; auto-enables cross-compilation |
| `PICO_BOARD` | `pico2_w` | Target board variant |
| `CMAKE_BUILD_TYPE` | `MinSizeRel` | Use `Debug` for development, `Release` for release |
| `SANITIZE` | `OFF` | Enable AddressSanitizer + UBSan (host builds only) |

---

## Third-party patches

Libcsp is pinned to an official upstream commit and requires a small set of Linux/POSIX compatibility patches. They are stored in `patches/libcsp/` and applied via:

```bash
bash scripts/apply_patches.sh
```

The script is idempotent — running it multiple times is safe. Dev Container and Docker workflows run it automatically.

See [patches/README.md](../patches/README.md) for the list of patches and rationale.

---

## Troubleshooting

| Problem | Solution |
|---------|----------|
| `CMake Error: pico_sdk_import.cmake not found` | Set `PICO_SDK_PATH` or use `-DPICO_ENABLED=OFF` |
| `arm-none-eabi-gcc: not found` | Install `gcc-arm-none-eabi` |
| `undefined reference to csp_*_hook` | Patches not applied — run `bash scripts/apply_patches.sh` |
| Tests fail | Ensure host build: `cmake -B build -DPICO_ENABLED=OFF` |
| UF2 not generated | Ensure Pico SDK build (with `PICO_SDK_PATH`) |
| Patches already applied error | Script shows `[skip]` safely — not an error |
| GCC 15 + Pico SDK 2.2.0 build error | Known upstream incompatibility (`nvic_hw->icpr`); use GCC ≤ 14 or Pico SDK 2.1.x |

---

## Static analysis

```bash
bash scripts/static_analysis.sh
```

## Memory Map

See [MEMORY_MAP.md](MEMORY_MAP.md) for the complete flash and SRAM layout,
boot metadata format, FreeRTOS heap/stack config, and boot flow diagram.
