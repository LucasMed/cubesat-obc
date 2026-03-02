# Project Progress — CubeSat OBC

**Last Updated**: 2026-03-08
**Current Phase**: Phase 5 — Flight Readiness (branch `feature/phase4-advanced-control` → merge to dev pending)
**Current Branch**: `feature/phase4-advanced-control`

---

## Milestones Completed

### Phase 1: Skeleton ✅ (2026-02-12)
- Modular architecture following ECSS-Q-ST-80C
- FreeRTOS task framework with 4 tasks (stubs on host)
- PID controller, attitude dynamics, actuator models
- 3 unit tests passing (100%)
- CI/CD via GitHub Actions
- Documentation framework established

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

### Phase 3: Communication & Telemetry ✅ (2026-02-22)
- **Goal**: Integrate `libcsp` for binary telemetry and remote command handling.
- **Outcomes**:
  - Successfully compiled `libcsp` for FreeRTOS SMP on RP2350.
  - Implemented `pico_usart` to bridge CSP to UART1 via KISS framing.
  - Refactored `telemetry_task` to emit packed binary packets at 1 Hz.
  - Created `command_task` listening on Port 20 for Echo and Reboot commands.
  - Defined strict interface specifications in `PHASE3_COMM_SPEC.md`.
  - Achieved comprehensive unit testing (64% project line coverage) for packet packing, command parsing, and topology initialization.

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
| PR-15 | `(current)` | LQR/PID dispatch in attitude_control_task | attitude_control_task_test: 11/11 ✅ |

---

## Overall Roadmap

| Phase | Target | Description | Status |
|-------|--------|-------------|--------|
| 1 — Skeleton | Feb 2026 | Architecture, FreeRTOS stubs, PID, tests | ✅ Complete |
| 2 — Pico SDK | Feb 2026 | Real FreeRTOS, I2C drivers, HW testing | ✅ Complete |
| 3 — Communication | Mar 2026 | CSP Protocol, Telemetry, Ground Station | ✅ Complete |
| Spec Alignment | Mar 2026 | REVISION_PHASE PRs 1–10 vs SPEC-2 v2.0 | ✅ Complete (10/10 PRs) |
| 4 — Advanced Control | Q2 2026 | EKF estimator, LQR controller, RK2 dynamics | ✅ Complete (5/5 PRs) |
| 5 — Flight Ready | Q3 2026 | Watchdog, safe states, WiFi, HW drivers | ⏳ Pending |

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
| Unit Tests | 19/19 | 0 | 19 |
| Integration Tests | 2/2 | 2 | 4 |
| System Tests | 0 | 2 | 2 |
| **Total** | **21** | **4** | **25** |

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
| MISRA C | ✅ Naming, static memory, safety |
| IEC 61508 | ✅ Task priorities, determinism |
| NASA SWE-130 | ✅ Modular design, test automation |

---

## Known Limitations (Host Build)

- FreeRTOS uses stubs on Linux (no real multitasking)
- I2C/sensors return zeros (simulated)
- WiFi/UART not functional on host
- **Resolution**: Pico SDK integration (Phase 2)
