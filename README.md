# CubeSat OBC - Pico 2W Flight Software

**Professional-Grade Spacecraft Attitude Control OBC (On-Board Computer) for CubeSats**

Implements a FreeRTOS-based control system following **ECSS-Q-ST-80C** aerospace software standards. Designed for Pico 2W with extensibility to flight-ready systems.

**Status:** v0.3.0-dev — Phase 3 (Communication & Telemetry) Completed
**Platform:** Linux (native, Docker, or VS Code Dev Container)
**License:** MIT  
**Maintainers:** ExArsultre

---

## 🚀 Key Features

### Control System
- **3-DOF Attitude Control** (Roll, Pitch, Yaw)
- **PID Controllers** with configurable gains
- **Actuator Models**: Reaction wheels + magnetorquers  
- **Dynamics Simulator** with real-time Euler integration
- Extensible to LQR, MPC, or adaptive control

### Real-Time OS
- **FreeRTOS** with 4 concurrent tasks
- Task priorities: Sensor (HIGH) → Control (HIGH) → Telemetry (MEDIUM) → Health (LOW)
- Configurable tick rate, heap, stack sizes
- Dual-core ready for Pico 2W

### Hardware Targets
- **Pico 2W (RP2350)**: ARM Cortex-M33, dual-core, WiFi/BLE
- **Interfaces**: I2C (sensors), UART (logs), SPI (expandable)
- **Sensors**: MPU6050 (IMU), TMP102 (temperature), etc.

### Development Quality
- ✅ **Unit Tests**: 8 tests (PID, dynamics, actuators, CSP, telemetry, commands, tasks, I2C) - 100% passing
- ✅ **CI/CD**: GitHub Actions with automated build+test
- ✅ **Static Analysis**: cppcheck integration
- ✅ **CMake Build**: Reproducible, Linux-native and Docker
- ✅ **Dev Container**: One-click VS Code environment via `.devcontainer/`
- ✅ **Standards**: MISRA C, ECSS conventions, modular architecture
- ✅ **Documentation**: API docs, coding guides, build guides

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

git clone https://github.com/yourusername/cubesat-obc.git
cd cubesat-obc
git submodule update --init --recursive
bash scripts/apply_patches.sh          # patches libcsp for Linux
cmake -B build -DPICO_ENABLED=OFF
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
# Expected: 100% tests passed, 0 tests failed out of 8
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
│   ├── obc_main.c              # Entry point, FreeRTOS init
│   ├── core/                   # State management
│   ├── drivers/                # Hardware drivers (I2C, UART, WiFi)
│   ├── actuators/              # RW, magnetorquer models
│   ├── control/                # PID, attitude control laws
│   ├── dynamics/               # Attitude dynamics simulator
│   └── tasks/                  # FreeRTOS tasks (4 tasks)
├── include/                    # Public APIs
├── tests/unit/                 # Unit tests (8 tests)
├── config/                     # FreeRTOS configuration
├── docs/                       # Architecture, guides, standards
├── scripts/                    # Build, test, analysis, patch scripts
├── patches/                    # Local patches for third-party submodules
│   └── libcsp/                 # Linux/POSIX compatibility patches for libcsp
├── third_party/
│   ├── FreeRTOS-Kernel/        # FreeRTOS kernel (git submodule)
│   └── libcsp/                 # CSP library (git submodule)
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
cd build
ctest --output-on-failure --verbose
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
- [ ] Code follows [Coding Standards](docs/CODING_STANDARDS.md)
- [ ] CHANGELOG.md updated
- [ ] Documentation updated (if applicable)

---

## 📚 Documentation

| Document | Audience | Content |
|----------|----------|---------|
| [CONTRIBUTING.md](CONTRIBUTING.md) | Developers | How to contribute, code style, testing |
| [docs/CODING_STANDARDS.md](docs/CODING_STANDARDS.md) | Developers | MISRA-like conventions, naming, safety |
| [docs/standards/CODING_STANDARDS.md](docs/standards/CODING_STANDARDS.md) | Developers | Full coding standards reference |
| [docs/BUILD_GUIDE.md](docs/BUILD_GUIDE.md) | Users | Build setup, Pico SDK, flashing, troubleshooting |
| [docs/PROJECT_PROGRESS.md](docs/PROJECT_PROGRESS.md) | Project Leads | Roadmap, current status, blockers |
| [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) | Engineers | System design, data flow, decisions |
| [docs/design/SYSTEM_DESIGN.md](docs/design/SYSTEM_DESIGN.md) | Engineers | Detailed system design document |
| [docs/requirements/SOFTWARE_REQUIREMENTS.md](docs/requirements/SOFTWARE_REQUIREMENTS.md) | Engineers | Requirements specification (FR, NFR, IR) |
| [docs/test_plans/TEST_PLANS.md](docs/test_plans/TEST_PLANS.md) | QA/Test | Test plans, coverage, execution |
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

### Phase 4 - Advanced Control
- [ ] Kalman filter (attitude estimation)
- [ ] Control optimization (LQR, MPC)
- [ ] Robustness testing

### Phase 5 - Flight Ready
- [ ] Watchdog & fault recovery
- [ ] Safe states & shutdown
- [ ] Comprehensive logging
- [ ] Flight qualification

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
| Control System | ✅ Functional | PID + dynamics working |
| Tests | ✅ Complete | 8/8 unit tests passing (100%) |
| Documentation | ✅ Complete | Design, requirements, standards, test plans |
| I2C Drivers | 🔄 In Progress | MPU6050, TMP102 — Task 2.3 |
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

- **Questions**: Open a [GitHub Discussion](https://github.com/yourusername/cubesat-obc/discussions)
- **Bugs**: [GitHub Issues](https://github.com/yourusername/cubesat-obc/issues)
- **Email**: [team email]
- **Wiki**: [Project Wiki](https://github.com/yourusername/cubesat-obc/wiki)

---

**Last Updated:** 2026-02-27  
**Version:** 0.3.0-dev (Phase 3 — Communication Completed)

⭐ If you find this project useful, please star us on GitHub!
