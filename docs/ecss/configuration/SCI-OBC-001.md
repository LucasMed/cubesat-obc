# Software Configuration Index — SCI-OBC-001

**Version**: 1.0
**Date**: 2026-07-23
**Status**: Released
**Project**: CubeSat OBC Flight Software

---

## 1. Software Identification

| Field | Value |
|-------|-------|
| Name | CubeSat OBC Flight Software |
| Version | 0.29.0 |
| Release Date | 2026-07-23 |
| Status | Development (pre-AR) |
| Target Platform | RP2350 (Raspberry Pi Pico 2) |
| Repository | https://github.com/LucasMed/cubesat-obc.git |
| Branch | dev |

---

## 2. Build Configuration

### Host Build (Unit Tests)

| Parameter | Value |
|-----------|-------|
| CMAKE_BUILD_TYPE | Debug |
| PICO_ENABLED | OFF |
| Compiler | GCC (Apple clang or GNU) |
| Sanitizers | ASan + UBSan (optional) |
| Test Framework | Unity v2.6.1 |

### Target Build (RP2350)

| Parameter | Value |
|-----------|-------|
| CMAKE_BUILD_TYPE | Release |
| PICO_ENABLED | ON |
| PICO_BOARD | pico2_w |
| Compiler | ARM GNU Toolchain |
| FreeRTOS | Enabled (RP2040/RP2350 port) |
| Optimize | -O2 (default for Release) |

---

## 3. Toolchain Versions

| Tool | Version | Notes |
|------|---------|-------|
| CMake | ≥ 3.13 | Required by Pico SDK |
| ARM GNU Toolchain | 13.2.Rel1 | For RP2350 target builds |
| Host GCC/Clang | System default | For host unit tests |
| cppcheck | 2.x | Static analysis |

---

## 4. Third-Party Libraries

| Library | Version | Source | Purpose |
|---------|---------|--------|---------|
| FreeRTOS Kernel | v11.1.0 | third_party/FreeRTOS-Kernel | RTOS for RP2350 |
| Pico SDK | 2.1.1 | third_party/pico-sdk | HAL for RP2040/RP2350 |
| libcsp | develop | third_party/libcsp | Cubesat Space Protocol |
| Unity | v2.6.1 | third_party/Unity | Unit test framework |

---

## 5. Build Procedure

### Prerequisites

1. Install CMake ≥ 3.13
2. Install GCC or Clang (host builds)
3. Install ARM GNU Toolchain (target builds)
4. Clone with submodules:
   ```bash
   git clone --recursive https://github.com/LucasMed/cubesat-obc.git
   ```

### Host Build (Unit Tests)

```bash
cmake -S . -B build_host -DPICO_ENABLED=OFF -DCMAKE_BUILD_TYPE=Debug
cmake --build build_host
ctest --test-dir build_host
```

### Target Build (RP2350)

```bash
export PICO_SDK_PATH=/path/to/pico-sdk
cmake -S . -B build_target -DPICO_ENABLED=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build_target
# Output: build_target/cubesat_obc.uf2
```

### Full CI Pipeline

```bash
bash scripts/pico_ci.sh all
```

---

## 6. Configuration Items

| Item | Baseline | Description |
|------|----------|-------------|
| Source code | dev branch | Latest development |
| Test suite | 72/72 passing | Unit + integration tests |
| RTM | 28/28 traced | Requirements traceability |
| Static analysis | 0 issues | cppcheck clean |

---

## 7. Change History

| Version | Date | Author | Description |
|---------|------|--------|-------------|
| 1.0 | 2026-07-23 | ECSS Gate Agent | Initial SCI for AR preparation |
