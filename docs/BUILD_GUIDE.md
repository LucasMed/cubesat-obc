# Build Guide

**Last Updated**: 2026-02-27

---

## Overview

The project targets **Linux** for host builds (development and testing) and can be cross-compiled for Pico 2W (RP2350) when the Pico SDK is available.

| Build target | When to use |
|---|---|
| Linux host (`PICO_ENABLED=OFF`) | Day-to-day development, unit tests, CI |
| Pico 2W cross-compile | Hardware validation, final firmware |

---

## Recommended: VS Code Dev Container

All dependencies are pre-installed inside the container.

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
bash run_linux.sh test       # build + run all 8 tests
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
# Expected: 100% tests passed, 0 tests failed out of 8
```

### Run firmware (host stub)

```bash
./build/src/cubesat_obc_firmware
```

---

## Pico 2W Cross-compilation

### Prerequisites

```bash
# ARM cross-compiler
sudo apt install gcc-arm-none-eabi libnewlib-arm-none-eabi

# Pico SDK
git clone https://github.com/raspberrypi/pico-sdk.git /opt/pico-sdk
cd /opt/pico-sdk && git submodule update --init
export PICO_SDK_PATH=/opt/pico-sdk
```

### Build

```bash
cmake -B build          # PICO_SDK_PATH triggers Pico build automatically
cmake --build build -j$(nproc)
```

### Output artifacts

- `build/examples/blink_test.uf2` — LED blink validation

---

## Flash to Pico 2W

### Method A — USB drag-and-drop
1. Hold **BOOTSEL** button on Pico 2W
2. Connect USB while holding BOOTSEL
3. Pico appears as `RPI-RP2` mass storage
4. Copy UF2 file:
   ```bash
   cp build/examples/blink_test.uf2 /media/$USER/RPI-RP2/
   ```
5. Pico reboots automatically

### Method B — picotool
```bash
picotool load -x build/examples/blink_test.uf2
```

See [FLASHING_GUIDE.md](FLASHING_GUIDE.md) for detailed troubleshooting.

---

## Build CMake flags

| Flag | Default | Description |
|------|---------|-------------|
| `PICO_ENABLED` | `ON` | Set to `OFF` to force Linux host build |
| `PICO_SDK_PATH` | (env) | Path to Pico SDK; auto-enables cross-compilation |
| `PICO_BOARD` | `pico2_w` | Target board variant |
| `CMAKE_BUILD_TYPE` | `MinSizeRel` | Use `Debug` for development, `Release` for release |

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

---

## Static analysis

```bash
bash scripts/static_analysis.sh
```
