# Project Progress — CubeSat OBC

**Last Updated**: 2026-02-20  
**Current Phase**: Phase 2 — Pico SDK Integration  
**Current Branch**: `feature/pico-sdk-integration`

---

## Milestones Completed

### Phase 1: Skeleton ✅ (2026-02-12)
- Modular architecture following ECSS-Q-ST-80C
- FreeRTOS task framework with 4 tasks (stubs on host)
- PID controller, attitude dynamics, actuator models
- 3 unit tests passing (100%)
- CI/CD via GitHub Actions
- Documentation framework established

### Phase 2: Pico SDK Integration 🔄 (In Progress)

| Task | Status | Date | Notes |
|------|--------|------|-------|
| 2.1 — Pico SDK Setup | ✅ | 2026-02-16 | SDK integrated, blink test on HW |
| 2.2a — FreeRTOS Kernel | ✅ | 2026-02-16 | Real kernel, dual-core SMP config |
| 2.2b — HW Validation | ✅ | 2026-02-16 | LED blink verified on Pico 2W |
| 2.2c — flash.c Fix | ⏳ | — | Blocker: `pico_flash` + FreeRTOS headers |
| 2.3 — I2C Drivers | ⏳ | — | MPU6050, temperature sensor |
| 2.4 — System Integration | ⏳ | — | End-to-end control loop |
| 2.5 — HW Validation | ⏳ | — | Timing, power, sensor checks |

---

## Active Blockers

### 🔴 `pico_flash` + FreeRTOS Integration
- **Issue**: Pico SDK's `pico_flash` library doesn't include FreeRTOS headers — `flash.c` compilation fails (`pdPASS` undeclared)
- **Impact**: OBC firmware with full task scheduling cannot compile
- **Current workaround**: blink test works but doesn't use FreeRTOS tasks
- **Planned resolution**: Use proper FreeRTOS-SMP library for Pico SDK, ensuring flash module compatibility

---

## Immediate Next Steps

1. **Resolve flash.c blocker** — Integrate proper FreeRTOS-SMP library for real hardware builds
2. **Task 2.3: I2C Drivers** — Implement `mpu6050_pico.c` and `temperature_pico.c`
3. **Task 2.4: System Integration** — End-to-end control loop with real sensor data
4. **Task 2.5: HW Validation** — Timing, power profiling, sensor checks

---

## Overall Roadmap

| Phase | Target | Description | Status |
|-------|--------|-------------|--------|
| 1 — Skeleton | Feb 2026 | Architecture, FreeRTOS stubs, PID, tests | ✅ Complete |
| 2 — Pico SDK | Mar 2026 | Real FreeRTOS, I2C drivers, HW testing | 🔄 ~40% |
| 3 — Communication | Q2 2026 | WiFi/lwIP, telemetry, UART | ⏳ Pending |
| 4 — Advanced Control | Q2-Q3 2026 | Kalman filter, LQR/MPC | ⏳ Pending |
| 5 — Flight Ready | Q3 2026 | Watchdog, safe states, logging | ⏳ Pending |

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
| Unit Tests | 3/3 | 0 | 3 |
| Integration Tests | 0 | 3 | 3 |
| System Tests | 0 | 2 | 2 |
| **Total** | **3** | **5** | **8** |

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
