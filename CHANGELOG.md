# Changelog

All notable changes to the CubeSat OBC project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- Real FreeRTOS kernel integration for RP2350 dual-core SMP
- Pico SDK integration with CMake conditional build
- Blink test validated on Pico 2W hardware (CYW43 LED control)
- FreeRTOSConfig.h enhanced for RP2350 (32 KB heap, 1000 Hz tick, dual-core)
- Documentation: `docs/design/SYSTEM_DESIGN.md` — system design document
- Documentation: `docs/requirements/SOFTWARE_REQUIREMENTS.md` — requirements spec
- Documentation: `docs/standards/CODING_STANDARDS.md` — full coding standards
- Documentation: `docs/test_plans/TEST_PLANS.md` — test plans and coverage
- Documentation: `docs/release/RELEASE_NOTES.md` — release notes per version
- Documentation: `docs/PROJECT_PROGRESS.md` — consolidated project progress

### Changed
- `BUILD_GUIDE.md` expanded with Pico SDK setup, flashing, and troubleshooting
- `CODING_STANDARDS.md` now links to full standards document
- `README.md` updated with Phase 2 status and expanded docs table

### Removed
- `TASK2_2_PROGRESS.md` — obsolete task progress report
- `TASK2_2_COMPLETION_REPORT.md` — redundant with git history
- `TASK2_2B_VALIDATION.md` — consolidated into FLASHING_GUIDE
- `TASK2_2B_VALIDATION_REPORT.md` — unfilled template
- `NEXT_STEPS.md` — replaced by `PROJECT_PROGRESS.md`

### Known Issues
- `pico_flash` library conflicts with FreeRTOS headers — OBC firmware task scheduling pending

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
