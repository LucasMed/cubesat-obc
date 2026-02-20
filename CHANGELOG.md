# Changelog

All notable changes to the CubeSat OBC project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.2.0] - 2026-02-20

### Added
- MPU6050 IMU driver and onboard Temperature sensor driver (Pico ADC4)
- Thread-safe system state management using FreeRTOS mutexes
- High-resolution task jitter telemetry (µs precision)
- Integrated sensor-to-control loop in `obc_main.c`

### Changed
- Increased `configMINIMAL_STACK_SIZE` to 4KB and `configTOTAL_HEAP_SIZE` to 128KB for SMP/USB stability
- Forced `-mfloat-abi=soft` to ensure FreeRTOS SMP compatibility on RP2350
- Enabled UART0 and USB CDC stdio with FreeRTOS-aware initialization
- Fixed `pico_flash` header shadowing issues

### Fixed
- Resolved `FATAL: Stack overflow in task 'IDLE0'` caused by FPU context switching mismatch
- Resolved USB CDC enumeration issues on boot

### Known Issues
- Power consumption baseline measurement pending final hardware sign-off

## [0.1.0] - 2026-02-12

### Added
- Initial skeleton implementation
  - Modular architecture following ECSS-Q-ST-80C standards
  - Core subsystems: drivers, actuators, control, dynamics, tasks
  - FreeRTOS task framework with stub implementation for host builds
  - Configuration system (config/FreeRTOSConfig.h)
- Development environment
  - CMake build system with per-module organization
  - Unit testing framework (3 basic tests)
  - CI workflow via GitHub Actions
  - Build automation scripts
- Documentation
  - README with project overview
  - Coding standards (MISRA-like guidelines)
  - Build guide
  - Next steps and verification plan
- Project governance
  - MIT License
  - Contributing guidelines
  - `.gitignore` for embedded projects

### Project Status
**✅ Complete:** Architecture, FreeRTOS integration, testing framework, documentation.

**🔄 In Progress:** N/A

**⏳ TODO (Priority Order):**
1. Pico SDK integration (Pico 2W hardware support)
2. I2C driver for MPU6050 (real sensor readout)
3. WiFi and lwIP integration (telemetry transmission)
4. Kalman filter for attitude estimation
5. Advanced control algorithms (LQR, MPC)
6. Flight-ready hardening (watchdog, safe states, logging)

---

## Development Notes

### Versions 0.1.x Series
- **0.1.0**: Skeleton with FreeRTOS and basic control logic
- **0.2.0**: (Planned) Pico SDK + hardware driver support
- **0.3.0**: (Planned) WiFi and telemetry integration

### Estimated Timeline
- Phase 2 (Pico SDK): Q1 2026
- Phase 3 (WiFi): Q2 2026
- Phase 4 (Advanced Control): Q2-Q3 2026
- Phase 5 (Flight Ready): Q3 2026

---

## Unreleased Commits

View unreleased changes with:
```bash
git log $(git describe --tags --abbrev=0)..HEAD --oneline
```

---

**Last Updated:** 2026-02-12
