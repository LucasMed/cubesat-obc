# Project Progress — CubeSat OBC

**Last Updated**: 2026-04-11
**Current Phase**: Phase 8 — Full Testing (branch `feature/bh1750-light-sensor`)
**Current Branch**: `feature/bh1750-light-sensor`

---

## Hardware Validation Complete ✅ (2026-04-11)

### Verified Working Components

| Component | Interface | Status | Notes |
|-----------|-----------|--------|-------|
| **MPU-6050/6500** | I2C0 (GPIO4/5) | ✅ OK | Detected ID 0x70 (MPU-6500 variant) |
| **Temperature** | Internal ADC | ✅ OK | Integrated in MPU-6050 |
| **QMC5883L (Mag)** | I2C0 (GPIO4/5) | ✅ OK | Clone detected at 0x2C (HMC5883L driver updated) |
| **SHT31 (Temp/Hum)** | I2C0 (GPIO4/5) | ✅ OK | 0x44, CRC-8 validation |
| **BH1750 (Light)** | I2C0 (GPIO4/5) | ✅ OK | 0x23, 0.5 lux resolution |
| **GPS NEO-6M/7M** | UART0 (GPIO0/1) | ✅ OK | 9600 baud, fix obtained |
| **HC-12 Radio** | UART1 (GPIO8/9) | ✅ OK | 9600 baud, bidirectional |
| **W25Q64 (Flash)** | SPI0 (GPIO7 CS) | ✅ OK | 8MB, JEDEC ID: M=EF, T=40, C=17 |
| **Telemetry** | HC-12 TX | ✅ OK | Text format working |
| **Commands** | HC-12 RX | ✅ OK | REBOOT, MODE, ECHO, CAPTURE, I2CSCAN, BH1750_TEST |
| **Camera** | SPI0 | ❌ Not connected | OV2640 pending |
| **Reaction Wheels** | PWM | ❌ Not connected | Pending |
| **Magnetorquers** | PWM/GPIO | ❌ Not connected | Pending |

### Connection Summary (Verified)

```
Pico 2W Pinout (Verified 2026-03-30):
┌─────────────────────────────────────────────────────────────┐
│ GPIO4  ←→ MPU-6050 SDA                                      │
│ GPIO5  ←→ MPU-6050 SCL                                      │
│ GPIO0  →  GPS RX                                            │
│ GPIO1  ←  GPS TX                                            │
│ GPIO8  →  HC-12 RX                                         │
│ GPIO9  ←  HC-12 TX                                         │
│ 3.3V  →  MPU-6050 VCC, GPS VCC                            │
│ 5V     →  HC-12 VCC                                         │
│ GND    →  All grounds                                       │
└─────────────────────────────────────────────────────────────┘
```

### Ground Station Validation

- **Arduino Nano** (HC-12 bridge): ✅ Working
- **Telemetry parsing**: ✅ Working
- **Commands**: All tested OK
  - `HELP` → Returns command list
  - `REBOOT` → System restarts
  - `MODE=1/2/3` → Changes flight mode
  - `ECHO` → Test command
  - `CAPTURE` → Payload trigger (no camera yet)

### Sample Telemetry Output
```
[TLM] mode=3 att=-1.4,0.6,-0.9 flags=0x03 gps_lat=-34.780453 gps_lon=-58.288147 gps_alt=12.3 gps_valid=1
```

Decoded:
- Mode: 3 (NOMINAL)
- Attitude: Roll=-1.4°, Pitch=0.6°, Yaw=-0.9°
- Flags: 0x03 (IMU=OK, Temp=OK)
- GPS: Lat=-34.78°, Lon=-58.29°, Alt=12.3m, Valid=1

---

## Milestones Completed

### Phase 1: Skeleton ✅ (2026-02-12)
- Modular architecture following ECSS-Q-ST-80C
- FreeRTOS task framework with 4 tasks (stubs on host)
- PID controller, attitude dynamics, actuator models
- 3 unit tests passing (100%)
- CI/CD via GitHub Actions
- Documentation framework established

---

### Phase 2: Pico SDK Integration ✅ (2026-02-20)
- Full integration of Pico SDK with FreeRTOS SMP
- Thread-safe system state management
- I2C master driver and MPU6050/Temperature sensor support
- Resolved critical FPU incompatibility (forced `-mfloat-abi=soft`)
- Verified real-time timing (Jitter: ±22 µs @ 20 Hz)
- Robust USB CDC and UART diagnostics

| Task | Status | Date | Notes |
|------|--------|------|-------|
| 2.1 — Pico SDK Setup | ✅ | 2026-02-16 | SDK integrated, blink test on HW |
| 2.2 — FreeRTOS Port | ✅ | 2026-02-20 | SMP supported, flash.c fix applied |
| 2.3 — I2C Drivers | ✅ | 2026-02-20 | MPU6050 & Temp sensor drivers |
| 2.4 — System Integration | ✅ | 2026-02-20 | End-to-end control loop stable |
| 2.5 — HW Validation | ✅ | 2026-02-20 | Jitter confirmed ±22µs, stable USB |

---

### Phase 3: Communication & Telemetry ✅ (2026-02-22)
- **Goal**: Integrate `libcsp` for binary telemetry and remote command handling.
- **Outcomes**:
  - Successfully compiled `libcsp` for FreeRTOS SMP on RP2350.
  - Implemented `pico_usart` to bridge CSP to UART1 via KISS framing.
  - Refactored `telemetry_task` to emit packed binary packets at 1 Hz.
  - Created `command_task` listening on Port 20 for Echo and Reboot commands.
  - Defined strict interface specifications in `PHASE3_COMM_SPEC.md`.
  - Achieved comprehensive unit testing (64% project line coverage) for packet packing, command parsing, and topology initialization.

---

### Spec-Review Alignment ✅ PRs 1–10 (2026-03-01)
- **Goal**: Close gaps identified during the REVISION_PHASE against SPEC-2 v2.0 / SPEC-3 / SPEC-5.
- **Branch**: `feature/spec-review-alignment`
- **Outcomes (PRs 1–10 committed, 17/17 tests passing)**:

| PR | Commit | Description | Tests |
|----|--------|-------------|-------|
| PR-1 | `76bf234` | Foundation Types headers (7 files) | types_test: ✅ |
| PR-2 | `42709b4` | Data Layer Abstraction — `data_layer.c` + `system_state` shim | data_layer_test: ✅ |
| PR-3 | `36831b7` | Flight Mode Manager — `flight_mode_manager.c` | fmm_test: 11/11 ✅ |
| PR-4 | `f71d021` | Fault Manager — 32-slot table, FSM, anti-cascade | fault_manager_test: 12/12 ✅ |
| PR-5 | `2ea9440` | EPS Monitor — Schmidt-trigger hysteresis, energy states | eps_monitor_test: 12/12 ✅ |
| PR-6 | `02d779e` | Persistent Logger — 320-entry ring buffer, class filtering | logger_test: 12/12 ✅ |
| PR-7 | `1763bdd` | Sensor Read Task → DLA (gyro deg/s→rad/s conversion) | sensor_read_task_test: 7/7 ✅ |
| PR-8 | `9592c7b` | Attitude Control Task → DLA (FM guard + imu_valid guard) | attitude_control_task_test: 8/8 ✅ |
| PR-9 | `4b89aee` | Telemetry Task → DLA (FM guard, energy state in flags) | telemetry_test: 6/6 ✅ |
| PR-10 | `e59b91b` | Health Monitor — wire `fault_manager_tick` + `eps_monitor_tick` | health_monitor_task_test: 3/3 ✅ |

- **Active blockers resolved**:
  - `pico_flash` + FreeRTOS conflict — resolved by isolating flash calls behind a weak-symbol HAL stub; host build uses stub.
  - `tasks_lib` missing `core_lib` dependency — fixed in `src/tasks/CMakeLists.txt`.

---

### Phase 4: Advanced Control ✅ PRs 11–15 (2026-03-08)
- **Goal**: Implement EKF attitude estimator, LQR controller, RK2 dynamics, and wire them into running tasks.
- **Branch**: `feature/phase4-advanced-control`
- **Outcomes (PRs 11–15 committed, 19/19 tests passing)**:

| PR | Commit | Description | Tests |
|----|--------|-------------|-------|
| PR-11 | `93a8559` | RK2 midpoint dynamics integrator | dynamics_test: 5/5 ✅ |
| PR-12 | `5812ba1` | EKF estimator (6-state, bias correction) | ekf_test: 6/6 ✅ |
| PR-13 | `916672d` | LQR controller (3×6 gain matrix) | lqr_test: 7/7 ✅ |
| PR-14 | `96f969d` | Sensor fusion — EKF integrated into sensor_read_task | sensor_read_task_test: 10/10 ✅ |
| PR-15 | `254bde1` | LQR/PID dispatch in attitude_control_task | attitude_control_task_test: 11/11 ✅ |

---

### Phase 5: Flight Readiness ✅ PRs 16–20 (2026-03-12)
- **Goal**: Hardware abstraction layer for all remaining peripherals; full yaw observability.
- **Branch**: `feature/phase5-flight-ready`
- **Outcomes (PRs 16–20 committed, 23/23 tests passing)**:

| PR | Commit | Description | Tests |
|----|--------|-------------|-------|
| PR-16 | `64d5f0e` | Watchdog HAL stub + health monitor kick | watchdog_test: 5/5 ✅ |
| PR-17 | `b1bf725` | Momentum dump (B×L, FM_DETUMBLE guard) | momentum_dump_test: 5/5 ✅ |
| PR-18 | `cfea46a` | HMC5883L driver + DLA mag fields | hmc5883l_test: 4/4 ✅ |
| PR-19 | `4ee1213` | EKF yaw update via magnetometer tilt compensation | ekf_mag_test: 6/6 ✅ |
| PR-20 | `3763dcd` | Docs finalization (CHANGELOG, RELEASE_NOTES, TEST_PLANS, TRACEABILITY) | — |

---

### Hardware BOM / PDR ✅ (2026-03-05, branch `feature/hardware-bom`)
- **Goal**: Define complete hardware Bill of Materials; reach PDR PASS gate.
- **PDR Result**: ✅ **PASS** — architecture solid, RF design correct, ADCS coherent.
- **Key decisions**:
  - **LIS3MDL** as flight-grade magnetometer (HMC5883L discontinued; QMC5883L clone risk)
  - **TPS3431** external watchdog: GPIO20, 3 s timeout, kick from `HealthMonitorTask`
  - **GPS NEO-7M**: UART0 @ 9600 baud NMEA 0183; debug → USB CDC
  - **SAW filter 433 MHz**: added to E22-400M30S RF chain for EMI immunity
  - **Magnetorquers-first ADCS**: B-dot MTQ-only = Phase 1; RW precision pointing = Phase 2
  - **Ground station**: E22-400M30S + CP2102 USB-UART (symmetric 433 MHz, no custom firmware)
  - **Link budget** verified: +8.5 dB margin at 2300 km SSO horizon slant ✅
- **CDR pending**: LIS3MDL driver migration, GPS NMEA driver, B-dot controller, PWM HAL,
  OBC PCB design, WCET measurement, radiation qualification.

---

### Phase 6: Closed-Loop Stability & Architectural Consolidation ✅ PRs 21–27 (2026-03-20)
- **Goal**: Verify full EKF→LQR closed-loop stability via host simulation; close architectural debt (quaternion utility, gain scheduling, integration tests T-FMS-01 + T-SAFE-01, flash logger backend, coverage ≥90%, MISRA audit).
- **Branch**: `feature/phase6-closed-loop`
- **Plan**: [docs/PHASE6_PLAN.md](PHASE6_PLAN.md)
- **Outcomes (PRs 21–27 committed, 29/29 tests passing)**:

| PR | Commit | Description | Tests |
|----|--------|-------------|-------|
| PR-21 | `bbe9f9a` | Quaternion utility library (`quaternion.c/h`) | quaternion_test: 5/5 ✅ |
| PR-22 | `d7af9fc` | Configurable magnetic declination (`OBC_MAG_DECLINATION_RAD`) | ekf_mag_test +1 (T-EKFM-07) ✅ |
| PR-23 | `66798de` | LQR gain scheduling per energy state (`lqr_schedule.c/h`) | lqr_schedule_test: 3/3 ✅ |
| PR-24 | `5ada29a` | Closed-loop simulation harness (`closed_loop_sim.c/h`) | closed_loop_test: 6/6 ✅ |
| PR-25 | `36afd95` | Integration tests T-FMS-01a..d + T-SAFE-01a..c; watchdog safe-mode path | test_fault_safe: ✅  test_safe_trigger: ✅ |
| PR-26 | `ff630b2` | Flash-backend stub + event logger flush hook | event_logger_test: 4/4 ✅ |
| PR-27 | `454fcdf` | MISRA C audit (0 required/mandatory violations) + gcovr 91.8% | — |

---

### Phase 7: Scientific Payload Integration ✅ PRs 27–31 (2026-03-18)
- **Goal**: Integrate GPS, IMU, and Magnetometer drivers with integration tests; maximize coverage.
- **Branch**: `feature/phase7-payload`
- **Outcomes (PRs 27–31 committed, 27/27 unit tests + 3/3 integration tests passing)**:

| PR | Commit | Description | Tests |
|----|--------|-------------|-------|
| PR-27 | `454fcdf` | MISRA C audit (0 required/mandatory violations) + gcovr 91.8% | — |
| PR-28 | `xxxxxx` | Preliminary Design Review Report for FSW | — |
| PR-29 | `xxxxxx` | Fix/doc alignment PDR | — |
| PR-30 | `xxxxxx` | IMU Integration (data flow validation, temp_available fix) | test_imu_integration: ✅ |
| PR-31 | `xxxxxx` | GPS, IMU, Magnetometer integration (NMEA parser, GPRMC, UTC sync) | test_gps_integration: ✅ test_mag_integration: ✅ |

- **CI Pipeline**: 6/6 stages passing (host-test, pico-build, static analysis, coverage)
- **Coverage**: Line 93.0%, Function 92.4%
- **Deferred (Phase 8)**: Camera driver, LIS3MDL driver migration, FM_PAYLOAD, W25Qxx storage, PWM HAL, flash backend, MC/DC coverage

---

### Phase 8: Full Testing 🔄 PRs 32–xx (In Progress)
- **Goal**: Expand coverage to >95%, complete deferred Phase 7 items, hardware validation.
- **Branch**: `feature/phase8-full-testing`
- **Outcomes (in progress)**:

| PR | Commit | Description | Status |
|----|--------|-------------|--------|
| PR-32 | HW validation | MPU-6050/6500 driver fix (ID 0x70) | ✅ Complete |
| PR-33 | HW validation | GPS NEO-7M driver (UART0, 9600 baud) | ✅ Complete |
| PR-34 | HW validation | HC-12 radio integration (UART1, 9600 baud) | ✅ Complete |
| PR-35 | HW validation | Text telemetry + command parser | ✅ Complete |
| PR-36 | — | Camera driver | 🔄 Pending |
| PR-37 | — | LIS3MDL driver migration (HMC5883L discontinued) | 🔄 Pending |
| PR-38 | — | FM_PAYLOAD mode implementation | 🔄 Pending |
| PR-39 | `e3f9199` | W25Q64 external SPI flash storage | ✅ Complete |
| PR-40 | — | PWM HAL for reaction wheels/magnetorquers | 🔄 Pending |
| PR-41 | — | Full flash backend implementation | 🔄 Pending |
| PR-42 | — | MC/DC coverage analysis | 🔄 Pending |

- **Hardware validated**: ✅ IMU, GPS, HC-12 radio, temperature sensor, W25Q64 flash
- **Software validated**: ✅ Telemetry (text), Commands (REBOOT/MODE/ECHO/CAPTURE)
- **Current test status**: 44/44 tests passing
- **Current coverage**: Line 93.0%, Function 92.4% (target: >95%)
- **CI Pipeline**: 6/6 stages passing

---

## Overall Roadmap

| Phase | Target | Description | Status |
|-------|--------|-------------|--------|
| 1 — Skeleton | Feb 2026 | Architecture, FreeRTOS stubs, PID, tests | ✅ Complete |
| 2 — Pico SDK | Feb 2026 | Real FreeRTOS, I2C drivers, HW testing | ✅ Complete |
| 3 — Communication | Mar 2026 | CSP Protocol, Telemetry, Ground Station | ✅ Complete |
| Spec Alignment | Mar 2026 | REVISION_PHASE PRs 1–10 vs SPEC-2 v2.0 | ✅ Complete (10/10 PRs) |
| 4 — Advanced Control | Q2 2026 | EKF estimator, LQR controller, RK2 dynamics | ✅ Complete (5/5 PRs) |
| 5 — Flight Ready | Q3 2026 | Watchdog, momentum dump, magnetometer, EKF yaw | ✅ Complete (4/4 feature PRs + docs) |
| 6 — Closed-Loop Stability | Q1 2026 | Quaternion lib, gain scheduling, closed-loop sim, integration tests, coverage ≥90%, MISRA audit | ✅ Complete (7/7 PRs) |
| 7 — Scientific Payload | Mar 2026 | GPS, IMU, Magnetometer integration, integration tests, coverage 93% | ✅ Complete |
| 8 — Full Testing | Q2 2026 | Coverage expansion (>95%), pending tasks, camera, storage, FM_PAYLOAD, hardware validation | 🔄 In Progress |
| HW BOM / PDR | Mar 2026 | Full hardware BOM; PDR review; LIS3MDL, TPS3431 watchdog, SAW filter, MTQ-first ADCS strategy, GS design | ✅ Complete (BOM v1.0, PDR PASS) |

**Estimated Total**: ~8-10 weeks to flight-ready prototype

---

## Branching Policy

- `main` — stable, production-ready (merge from `dev` when validated)
- `dev` — active development (all feature branches from here)
- Feature branches: `feature/<short-name>`, merge to `dev` via PR

```bash
git checkout dev
git checkout -b feature/<short-name>
# develop → PR to dev → merge to main on release
```

---

## Verification Status

| Test Suite | Passing | Pending | Total |
|------------|---------|---------|-------|
| Unit Tests | 44/44 | 0 | 44 |
| Integration Tests | 0 | 0 | 0 |
| System Tests | 0 | 2 | 2 |
| **Total** | **44** | **0** | **46** |

**New test targets (Phase 7 — PRs 28–30)**:
- `test_gps_integration` — validates GPS driver and telemetry integration
- `test_imu_integration` — validates IMU data flow
- `test_mag_integration` — validates magnetometer EKF operations

**New test targets (Phase 6 — PRs 21–27)**:
- `quaternion_test` — 5 cases (T-QAT-01..05, unit-quaternion math)
- `lqr_schedule_test` — 3 cases (T-LQRS-01..03, gain table selection)
- `closed_loop_test` — 6 cases (T-CLS-01..06, EKF→LQR stability criterion)
- `event_logger_test` — 4 sub-tests (T-LOG-01a..d, flush to flash backend)
- `test_fault_safe` — 4 sub-tests (T-FMS-01a..d, end-to-end CRITICAL→FM_SAFE)
- `test_safe_trigger` — 3 sub-tests (T-SAFE-01a..c, watchdog trigger→FM_SAFE)

**New test targets (PRs 4–10)**:
- `fmm_test` — 11 cases (flight mode FSM, guard transitions)
- `fault_manager_test` — 12 cases (T-FMS-02..04 + anti-cascade)
- `eps_monitor_test` — 12 cases (T-EPS-03..05, Schmidt-trigger)
- `logger_test` — 12 cases (T-LOG-01..03, class-A eviction)
- `sensor_read_task_test` — 10 cases (deg→rad, DLA write, EKF valid flags)
- `attitude_control_task_test` — 11 cases (FM guard, imu_valid, LQR/PID dispatch)
- `telemetry_test` — 6 cases (T-TLM-01..06, DLA read, FM guard, energy flags)
- `health_monitor_task_test` — 3 cases (T-HM-01..03, tick wiring)

```bash
# Run all tests
cd build && cmake .. && cmake --build . && ctest --output-on-failure
```

---

## Standards Compliance

| Standard | Coverage |
|----------|----------|
| ECSS-Q-ST-80C | ✅ Architecture, documentation |
| MISRA C | ✅ 0 required/mandatory violations; advisory deviations in `docs/standards/MISRA_DEVIATIONS.md` |
| IEC 61508 | ✅ Task priorities, determinism |
| NASA SWE-130 | ✅ Modular design, test automation |
| gcovr Line Coverage | ✅ 93.0% line, 92.4% function (target ≥90%) |

---

## Known Limitations (Host Build)

- FreeRTOS uses stubs on Linux (no real multitasking)
- I2C/sensors return zeros (simulated)
- WiFi/UART not functional on host
- **Resolution**: Pico SDK integration (Phase 2) ✅ — Phase 5 adds HAL stubs for all remaining hardware peripherals
