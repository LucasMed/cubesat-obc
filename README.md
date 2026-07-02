# CubeSat OBC - Pico 2W Flight Software

**Professional-Grade Spacecraft Attitude Control OBC (On-Board Computer) for CubeSats**

Implements a FreeRTOS-based control system following **ECSS-Q-ST-80C** aerospace software standards. Designed for Pico 2W with extensibility to flight-ready systems.

**Status:** v0.34.0 — FM_PAYLOAD completeness & I2C bus mutex protection
**Platform:** Linux (native, Docker, or VS Code Dev Container)
**License:** MIT  
**Maintainers:** ExArsultre

---

## 🚀 Key Features

### Attitude Determination
- **6-State EKF** `x = [roll, pitch, yaw, bx, by, bz]` with gyro-bias estimation
- **Full yaw observability** via tilt-compensated HMC5883L magnetometer update (`H=[0,0,1,0,0,0]`)
- **RK2 midpoint integrator** for attitude dynamics (error <0.1 mrad/step at 20 Hz)

### Control System
- **3-DOF Attitude Control** (Roll, Pitch, Yaw)
- **LQR full-state controller** `u = -Kx` (ωn=10 rad/s, ζ=1 default gains)
- **PID fallback** per-axis in FM_DIAGNOSTIC or during EKF convergence
- **Momentum dump** — B×L detumble law, FM_DETUMBLE guard, DLA write
- **Actuator Models**: Reaction wheels + magnetorquers

### Real-Time OS
- **FreeRTOS** with 7 concurrent tasks
- Task priorities: Startup (4) → Sensor/AttitudeCtrl/CSPRouter (3) → Telemetry/Command/GPS/LED (2) → Health/Heartbeat/Payload (1)
- WCET instrumentation via DWT->CYCCNT on RP2350; dual-core SMP ready for Pico 2W

### Sensors & Drivers
- **MPU6050** (6-DOF IMU: gyro + accel) — I²C, host stub, hardware-verified
- **QMC5883L** (3-axis magnetometer) — I²C, clone detection for HMC5883L driver
- **DS3231** (RTC) — I²C 0x68, ±2 ppm, battery backup, Unix epoch output
- **SHT31** (temp + humidity) — I²C 0x44, CRC-8 validation
- **BH1750** (light) — I²C 0x23, 0.5 lux resolution
- **INA219** (power monitor) — I²C 0x40, high-side current/power sensing
- **GPS NEO-6M/7M** (UART) — NMEA parsing, UTC sync, 9600 baud
- All drivers host-testable with `PICO_ENABLED=OFF`

### Hardware Watchdog
- `watchdog_hal_init/kick/enable` with weak-symbol stubs
- Kick wired into `vHealthMonitorTask_Step()` every health-monitor tick

### Ground Station Communication
- **HC-12 Radio** (433 MHz) + Arduino Nano ground station
- **JSON/TEXT Format Switching** via `TLMFMT=JSON` / `TLMFMT=TEXT` commands
- **CRC8 Checksum** (CASPAC polynomial 0x07) for telemetry integrity
- **Flight Mode Control** via `MODE=0-5` commands with error feedback
- **Sun Sensor Integration** — dual-axis photodiode telemetry

### Development Quality
- ✅ **Unit Tests**: 68 tests (PID, dynamics, actuators, EKF, LQR, watchdog, momentum dump, magnetometer, telemetry, commands, tasks, DS3231 RTC, SHT31, INA219, BH1750, closed-loop simulation, fault-to-safe integration, fault injection matrix, CRC8, sun sensor, deploy monitor, POST, diskio, eps_hal, spi_payload, watchdog_hal, camera, radiation, rm3100, w25q64, payload, i2c_mock …) — **68/68 passing**
- ✅ **CI/CD**: GitHub Actions with automated build + test (7 stages)
- ✅ **Static Analysis**: cppcheck + clang-tidy + clang-format-14 + Coverity Scan
- [![Coverity Scan Build Status](https://scan.coverity.com/projects/33049/badge.svg)](https://scan.coverity.com/projects/cubesat-obc)
- ✅ **CMake Build**: Reproducible, Linux-native and Docker (`PICO_ENABLED=OFF`)
- ✅ **Dev Container**: One-click VS Code environment via `.devcontainer/`
- ✅ **Standards**: MISRA C, ECSS conventions, modular architecture
- ✅ **Documentation**: API docs, coding guides, build guides, traceability matrix

### Safety & FDIR
- **ISR-Safe Safe Mode**: `fmm_force_safe()` callable from any hardware ISR via critical-section lock (CDR-SAF-01)
- **Fault Injection Suite**: All 25 fault IDs exercised across 10 subsystems, WARNING/ERROR/CRITICAL levels, cross-subsystem multi-fault matrix (CDR-SAF-04)
- **WCET Profiler**: ARM Cortex-M33 DWT->CYCCNT instrumentation for all 7 FreeRTOS tasks (CDR-SAF-03)
- **Priority Inheritance**: FreeRTOS mutex inheritance documented and verified (PI-OBC-001, CDR-SAF-02)
- **StartupTask ALIVE Loop**: Diagnostic-only (HWM + heap), not safety-critical (STA-OBC-001, CDR-SAF-06)

---

## 📋 Quick Start

> **Recommended:** Use the VS Code Dev Container or Docker workflow below — dependencies are pre-installed and third-party patches are applied automatically.

### Option A — VS Code Dev Container (recommended)

1. Install [Docker](https://docs.docker.com/get-docker/) and the [Dev Containers extension](https://marketplace.visualstudio.com/items?itemName=ms-vscode-remote.remote-containers)
2. Open the repo in VS Code → click **Reopen in Container**
3. The container runs `git submodule update --init --recursive` and `bash scripts/apply_patches.sh` automatically
4. Build and test:
   ```bash
   cmake --build build
   ctest --test-dir build --output-on-failure
   ```

### Option B — Docker without VS Code

```bash
bash run_linux.sh build    # configure + build
bash run_linux.sh test     # build + run all tests
bash run_linux.sh coverage # build + tests + HTML coverage
bash run_linux.sh shell    # interactive shell
```

### Option C — Native Linux

```bash
# Prerequisites (Debian/Ubuntu)
sudo apt install cmake build-essential ninja-build git

git clone https://github.com/LucasMed/cubesat-obc.git
cd cubesat-obc
git submodule update --init --recursive
bash scripts/apply_patches.sh          # patches libcsp for Linux
cmake -B build -DPICO_ENABLED=OFF
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
    # Expected: 100% tests passed, 0 tests failed out of 68
    ```

### Flash to Pico 2W (When SDK Ready)

```bash
export PICO_SDK_PATH=/path/to/pico-sdk
cmake -B build
cmake --build build
picotool load -x build/examples/blink_test.uf2
```

---

## 📁 Project Structure

```
cubesat-obc/
├── src/
│   ├── obc_main.c              # Entry point, FreeRTOS init, StartupTask
│   ├── core/                   # DLA, FMM, Fault Manager, EPS Monitor, Logger
│   ├── drivers/               # Hardware drivers (MPU6050, HMC5883L, DS3231, SHT31, INA219, BH1750, GPS NEO-7M, W25Q64, RM3100, camera, radiation)
│   ├── actuators/              # RW, magnetorquer models
│   ├── control/                # EKF, LQR, PID, RK2 dynamics
│   ├── dynamics/               # RK2 attitude dynamics integrator
│   ├── services/              # Watchdog HAL, momentum dump, WCET profiler, comm/CSP
│   │   └── wcet/              # WCET profiler (DWT CYCCNT on RP2350)
│   └── tasks/                  # FreeRTOS tasks (7 tasks: SensorRead, AttitudeCtrl, Telemetry, Command, HealthMon, GpsTask, PayloadTask)
├── include/                    # Public APIs
├── tests/unit/                 # Unit tests (60 tests)
├── tests/integration/           # Integration tests (fault-to-safe, trigger)
├── config/                     # FreeRTOS configuration
├── docs/                       # Architecture, guides, standards, safety, design
│   └── ecss/
│       ├── safety/            # FMEA, priority inheritance, ALIVE loop eval
│       └── verification/       # RTM, STP
├── scripts/                    # Build, test, CI, analysis, patch scripts
├── patches/                    # Local patches for third-party submodules
│   └── libcsp/                 # Linux/POSIX compatibility patches for libcsp
├── third_party/
│   ├── FreeRTOS-Kernel/        # FreeRTOS kernel (git submodule)
│   ├── pico-sdk/              # Raspberry Pi Pico SDK (git submodule)
│   └── libcsp/                 # CSP library (git submodule)
├── .github/workflows/          # GitHub Actions CI (build, test, emulate, Coverity)
├── .devcontainer/              # VS Code Dev Container configuration
├── docker-compose.yml          # Docker workflow (build/test/coverage/shell)
├── run_linux.sh                # Docker convenience wrapper
├── CMakeLists.txt              # Build configuration
├── LICENSE                     # MIT License
├── CONTRIBUTING.md             # Contribution guidelines
├── CHANGELOG.md                # Version history
└── README.md                   # This file
```

---

## 🧪 Testing

### Run All Tests
```bash
ctest --test-dir build --output-on-failure
# Expected: 100% tests passed, 0 tests failed out of 68
```

### Run Specific Test
```bash
ctest --test-dir build -R test_pid --output-on-failure
```

### Run Static Analysis
```bash
scripts/static_analysis.sh
```

---

## ⚙️ CI/CD Pipeline

The project uses a 7-stage CI pipeline (`scripts/pico_ci.sh all`):

| Stage | Description |
|-------|-------------|
| host-test | CMake build + ctest (65 tests) |
| pico-build | Cross-compile firmware for RP2350 |
| emu-build | Build with QEMU ARM emulation |
| emulate | Run tests under QEMU |
| static | clang-format + clang-tidy + cppcheck |
| coverity | Coverity Scan deep static analysis |
| coverage | gcovr HTML + text coverage report |

CI runs on GitHub Actions for every push to `main`, `dev`, and feature branches.
Coverity Scan executes on `main` and `dev` pushes (requires `COVERITY_SCAN_EMAIL` + `COVERITY_SCAN_TOKEN` secrets).

---

## 🤝 Contributing

We welcome contributions! Please read [CONTRIBUTING.md](CONTRIBUTING.md) for:

- Development workflow & branch naming
- Code style & standards  
- Testing requirements
- Commit message format
- Pull request process

**Quick checklist for PRs:**
- [ ] Tests pass: `ctest --output-on-failure`
- [ ] No warnings: compiles with `-Wall -Wextra -pedantic`
- [ ] Code follows [Coding Standards](docs/ecss/standards/CODING_STANDARDS.md)
- [ ] CHANGELOG.md updated
- [ ] Documentation updated (if applicable)

---

## 📚 Documentation

| Document | Audience | Content |
|----------|----------|---------|
| [CONTRIBUTING.md](CONTRIBUTING.md) | Developers | How to contribute, code style, testing |
| [docs/ecss/standards/CODING_STANDARDS.md](docs/ecss/standards/CODING_STANDARDS.md) | Developers | Full coding standards reference (MISRA-like conventions, naming, safety) |
| [docs/dev/BUILD_GUIDE.md](docs/dev/BUILD_GUIDE.md) | Users | Build setup, Pico SDK, flashing, troubleshooting |
| [docs/project/PROJECT_PROGRESS.md](docs/project/PROJECT_PROGRESS.md) | Project Leads | Roadmap, current status, blockers |
| [docs/architecture/ARCHITECTURE.md](docs/architecture/ARCHITECTURE.md) | Engineers | System design, data flow, decisions |
| [docs/architecture/SYSTEM_DESIGN.md](docs/architecture/SYSTEM_DESIGN.md) | Engineers | Detailed system design document |
| [docs/ecss/requirements/SRS-OBC-001.md](docs/ecss/requirements/SRS-OBC-001.md) | Engineers | Software Requirements Specification (FR, NFR, IR) |
| [docs/ecss/verification/STP-OBC-001.md](docs/ecss/verification/STP-OBC-001.md) | QA/Test | Software Test Plan, coverage, execution |
| [docs/README.md](docs/README.md) | Everyone | Full documentation index by ECSS milestone |
| [docs/release/RELEASE_NOTES.md](docs/release/RELEASE_NOTES.md) | Everyone | Release notes per version |
| [CHANGELOG.md](CHANGELOG.md) | Everyone | Version history, features, status |
| [LICENSE](LICENSE) | Legal | MIT License terms |

---

## 🗺️ Roadmap

### Phase 1 ✅ - Skeleton
- [x] Architecture & module structure
- [x] FreeRTOS integration  
- [x] Control loops (PID, dynamics)
- [x] Unit tests
- [x] Documentation & standards

### Phase 2 - Pico SDK Integration
- [x] Pico SDK + real FreeRTOS
- [x] I2C drivers (MPU6050, TMP102)
- [x] Hardware testing

### Phase 3 ✅ - Communication
- [x] libcsp integration
- [x] Telemetry protocol & packets
- [x] Remote command decoding (C&DH)

### Phase 4 ✅ - Advanced Control
- [x] 6-state EKF (gyro-bias estimation, analytic S⁻¹)
- [x] LQR full-state controller (3×6 gain matrix)
- [x] RK2 midpoint dynamics integrator
- [x] EKF wired into sensor_read_task
- [x] LQR/PID dispatch in attitude_control_task

### Phase 5 ✅ - Flight Ready
- [x] Hardware watchdog HAL + health-monitor kick
- [x] Momentum dump (B×L detumble, FM_DETUMBLE guard)
- [x] HMC5883L magnetometer driver (I²C + HAL stub)
- [x] EKF yaw update via tilt-compensated magnetometer
- [x] 23/23 unit tests
- [x] Full documentation update

---

## 🛠️ Development Commands

```bash
# Build
cmake --build build -- -j$(nproc)

# Test
ctest --test-dir build --output-on-failure

# Static analysis
scripts/static_analysis.sh

# Clean build
rm -rf build && mkdir build && cd build && cmake .. && cmake --build .

# Build firmware only
cmake --build build --target cubesat_obc_firmware

# Verbose build
cmake --build build --verbose
```

---

## 📊 Project Status

| Area | Status | Notes |
|------|--------|-------|
| Architecture | ✅ Complete | ECSS-Q-ST-80C compliant |
| FreeRTOS | ✅ Real Kernel | Integrated on RP2350, dual-core SMP |
| Pico SDK | ✅ Integrated | Blink test validated on hardware |
| EKF Estimator | ✅ Complete | 6-state, gyro bias, accel + mag yaw update |
| LQR Controller | ✅ Complete | 3×6 gain matrix, PID fallback |
| RK2 Dynamics | ✅ Complete | <0.1 mrad/step at 20 Hz |
| Watchdog HAL | ✅ Complete | Weak-symbol stub, kick wired to health monitor |
| Momentum Dump | ✅ Complete | B×L law, FM_DETUMBLE guard |
| HMC5883L Driver | ✅ Complete | I²C + host stub |
| Data Layer (DLA) | ✅ Complete | Mutex-protected state store |
| FMM / Fault / EPS | ✅ Complete | 6-mode FSM, Schmidt-trigger EPS |
| Telemetry | ✅ Complete | libcsp, 1 Hz packets, FM guard |
| Tests | ✅ Complete | **68/68** tests passing (100%) |
| Documentation | ✅ Complete | Design, requirements, traceability, test plans |
| I2C Drivers | ✅ Complete | MPU6050, TMP102, HMC5883L |
| WiFi/Telemetry | ✅ Complete | Phase 3 (libcsp) successfully integrated |

---

## 📝 Standards Compliance

- **ECSS-Q-ST-80C**: Software quality & architecture ✅
- **MISRA C**: Code safety & reliability ✅
- **IEC 61508**: Functional safety foundation ✅
- **NASA SWE-130**: Software assurance practices ✅

---

## 🔒 Security & Safety

- ✅ No hardcoded secrets or credentials
- ✅ Static memory allocation (no `malloc` in flight code)
- ✅ Bounds checking on arrays/buffers
- ✅ Safe task priorities and synchronization
- ✅ Comprehensive error handling

---

## 🐛 Reporting Issues

- **Bugs**: [Bug Report Template](.github/ISSUE_TEMPLATE/bug_report.md)
- **Features**: [Feature Request Template](.github/ISSUE_TEMPLATE/feature_request.md)
- **Security**: Report privately to [maintainers]

---

## 📜 License

MIT License - See [LICENSE](LICENSE) file for details.

```
Copyright (c) 2026 CubeSat OBC Contributors
```

---

## 🙏 Acknowledgments

Built with:
- [FreeRTOS](https://www.freertos.org/)
- [Pico SDK](https://github.com/raspberrypi/pico-sdk)
- [ARM CMSIS](https://github.com/ARM-software/CMSIS_5)

---

## 📞 Contact & Support

- **Questions**: Open a [GitHub Discussion](https://github.com/LucasMed/cubesat-obc/discussions)
- **Bugs**: [GitHub Issues](https://github.com/LucasMed/cubesat-obc/issues)

---

**Last Updated:** 2026-07-02  
**Version:** 0.34.0 (FM_PAYLOAD completeness & I2C bus mutex protection)

⭐ If you find this project useful, please star us on GitHub!
