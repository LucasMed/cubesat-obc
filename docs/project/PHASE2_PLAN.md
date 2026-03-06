# Phase 2: Pico SDK Integration Plan

## Overview

**Phase 2** integrates the official Raspberry Pi Pico SDK, replaces FreeRTOS stubs with real RTOS on RP2040, and implements functional I2C drivers for sensors (MPU6050, temperature). This phase validates the control loop on real hardware and establishes the foundation for Phase 3 (communication).

**Duration Estimate**: 2–3 weeks  
**Start Date**: 2026-02-15  
**Target Completion**: Early March 2026

---

## Goals

1. **Pico SDK Integration**
   - Set up CMake to build with official Pico SDK
   - Configure cross-toolchain (arm-none-eabi)
   - Build and flash firmware to RP2040

2. **Real FreeRTOS on Pico**
   - Replace host stubs with actual FreeRTOS kernel
   - Verify task scheduling on dual-core MCU
   - Measure task jitter and timing compliance

3. **I2C Drivers**
   - Implement MPU6050 driver using Pico I2C API
   - Add temperature sensor support (onboard or TMP102)
   - Validate sensor readout (non-zero, plausible values)

4. **Control Loop Validation**
   - Test attitude control loop with real sensor feedback
   - Measure response to simulated disturbances
   - Verify attitude stabilization (goal: settle within 30 sec)

5. **Hardware Testing**
   - Test on physical Pico 2W or simulator (QEMU/Wokwi)
   - Validate GPIO/I2C pinout configuration
   - Measure power consumption (baseline for Phase 3)

---

## Deliverables

### Code Changes
- [ ] `CMakeLists.txt` — Updated to support Pico SDK (conditional build)
- [ ] `src/drivers/i2c/pico_i2c.c` — I2C master bus driver using Pico SDK
- [ ] `src/drivers/imu/mpu6050_pico.c` — MPU6050 implementation via I2C
- [ ] `src/drivers/temperature/temperature_pico.c` — Temperature sensor driver
- [ ] `config/pico_pins.h` — Pin definitions (I2C, UART, GPIO)
- [ ] `FreeRTOS_Kernel_import.cmake` — Real FreeRTOS integration (replace stub)
- [ ] Updated task implementations in `src/tasks/` — Remove stubs, call real drivers

### Documentation
- [ ] `docs/PHASE2_BUILD_GUIDE.md` — Step-by-step SDK setup and build
- [ ] `docs/PHASE2_TESTING.md` — Hardware testing and validation checklist
- [ ] `docs/PICO_PIN_MAPPING.md` — I2C, UART, power pin assignments
- [ ] Updated `docs/ARCHITECTURE.md` — Reflect real SDK integration

### Tests
- [ ] Integration test: I2C communication (MPU6050 readout)
- [ ] Integration test: FreeRTOS task scheduling (jitter measurement)
- [ ] Integration test: Full control loop (sensor → control → actuator)

### Hardware Validation
- [ ] Power baseline measurement
- [ ] Task timing profile (best/worst/average jitter)
- [ ] Sensor sanity checks (range, noise floor)

---

## Work Breakdown Structure (WBS)

### Task 2.1: Pico SDK Setup (Est. 3 days)
**Owner**: Build/Infra Lead

1. **Download and install Pico SDK**
   - Clone: `https://github.com/raspberrypi/pico-sdk.git`
   - Set `PICO_SDK_PATH` environment variable
   - Verify cross-compiler: `arm-none-eabi-gcc --version`

2. **Integrate SDK into CMake**
   - Add `pico_sdk_import.cmake` to project
   - Update root `CMakeLists.txt` to conditionally include SDK
   - Test build with and without SDK (flag: `-DPICO_SDK=ON`)

3. **Create minimal Pico app**
   - Blink LED example to verify build chain
   - Flash to RP2040 and confirm execution

**Acceptance Criteria**:
- [ ] Pico SDK builds successfully
- [ ] LED blink demo runs on hardware
- [ ] Dual-core RP2040 boot verified

---

### Task 2.2: FreeRTOS Real Kernel Integration (Est. 3 days)
**Owner**: RTOS/Core Team

1. **Integrate FreeRTOS official kernel**
   - Replace stub `include/FreeRTOS.h` with real headers from SDK
   - Update `config/FreeRTOSConfig.h` with RP2040-specific settings:
     - Heap size: 32 KB (configurable)
     - Tick rate: 1000 Hz
     - Max priorities: 5
     - Dual-core: Core 0 = OS, Core 1 = (reserved for future)

2. **Port task layer to real FreeRTOS**
   - Update task signatures in `src/tasks/*`
   - Replace fake `vTaskDelay()` with real delays
   - Verify task creation and scheduling

3. **Test FreeRTOS on hardware**
   - Measure task jitter (target: <10 ms for 20 Hz control)
   - Verify no task starvation
   - Check stack usage per task

**Acceptance Criteria**:
- [ ] Real FreeRTOS compiles and runs
- [ ] All 4 tasks visible in scheduler
- [ ] Task jitter <10 ms (measured via `xTaskGetTickCount()`)

---

### Task 2.3: I2C Driver Implementation (Est. 5 days)
**Owner**: Drivers Team

#### 2.3.1: Pico I2C Master (Est. 2 days)
```markdown
File: src/drivers/i2c/pico_i2c.c

API:
- i2c_master_init(uint8_t sda_pin, uint8_t scl_pin, uint32_t speed)
- i2c_write(uint8_t addr, const uint8_t *data, size_t len)
- i2c_read(uint8_t addr, uint8_t *data, size_t len)
- i2c_write_then_read(uint8_t addr, const uint8_t *tx, size_t tx_len, uint8_t *rx, size_t rx_len)

Implementation:
- Use Pico SDK i2c_* functions (non-blocking or polling)
- Handle bus errors gracefully (retry, timeout)
- Support standard (100 kHz) and fast (400 kHz) modes
```

#### 2.3.2: MPU6050 Driver (Est. 2 days)
```markdown
File: src/drivers/imu/mpu6050_pico.c

Spec: Invensense MPU6050 (6-DOF accelerometer + gyroscope)

Init Sequence:
1. I2C write: Power management register (wake up, clock source)
2. Configure accelerometer range (±8g)
3. Configure gyroscope range (±500°/s)
4. Set sample rate divider (to 10 Hz or as needed)

Read Function:
- Poll status register for data ready
- Read 6 * 2 bytes (accel X/Y/Z, gyro X/Y/Z, 2 bytes each)
- Convert raw counts to physical units (m/s², rad/s)
- Validate range and NaN checks

Testing:
- Verification: Enable sensor on benchtop, shake, observe values change
- Acceptance: Non-zero values on motion, stable at rest (noise floor <10 LSB)
```

#### 2.3.3: Temperature Sensor (Est. 1 day)
```markdown
File: src/drivers/temperature/temperature_pico.c

Options:
A) Onboard RP2040 temperature sensor (ADC4, no external hardware)
   - Simpler, no I2C needed
   - Single-ended read via ADC, convert to °C

B) TMP102 (I2C, if external sensor available)
   - More accurate, easier calibration
   - I2C slave address: 0x48 (default)

Initial: Implement Option A (onboard ADC)
Future: Add B if hardware available
```

**Acceptance Criteria**:
- [ ] I2C master initializes and communicates with slave
- [ ] MPU6050 returns non-zero accel/gyro values on motion
- [ ] Temperature reads within expected range (10–40°C at room temp)
- [ ] No I2C timeouts or bus errors in normal operation

---

### Task 2.4: System Integration (Est. 3 days)
**Owner**: Integration Lead

1. **Update main() and task layer**
   - Call `mpu6050_init()` and `temperature_init()` in boot
   - Sensor task calls real driver functions
   - Update `system_state_t` with real data (not zeros)

2. **Control loop integration**
   - Attitude control task reads real sensor data
   - Computes real torque commands
   - Commands passed to actuator models (still simulated in Phase 2)

3. **Validation suite**
   - Unit test: I2C read/write (mock device or loopback)
   - Integration test: Control loop (sensor → control → actuator) end-to-end
   - Timing test: Task jitter, deadline adherence
   - Functional test: Attitude stabilization on hardware

**Acceptance Criteria**:
- [ ] All tasks run, communicate via system_state
- [ ] Control loop closes with real sensor feedback
- [ ] Attitude error reduces over time (validation in simulation mode)

---

### Task 2.5: Hardware Testing & Validation (Est. 4 days)
**Owner**: Test/Validation Lead

1. **Setup & Flashing**
   - Prepare Pico 2W board
   - Flash firmware via USB (UF2) or picotool
   - Verify boot messages via UART (debug console)

2. **Sensor Validation**
   - Verify I2C communication (oscilloscope or logic analyzer)
   - Read IMU sensor values (should be non-zero when shaken)
   - Confirm temperature readout is in plausible range

3. **Timing Characterization**
   - Measure task jitter via FreeRTOS hooks or GPIO toggling
   - Verify 10 Hz, 20 Hz, 1 Hz task rates
   - Document best/worst/average case timing

4. **Control Loop Validation**
   - Run open-loop control (command fixed torque)
   - Log attitude, rates, sensor data to UART
   - Verify control system responds correctly
   - If closed-loop possible: test stabilization

5. **Power & Thermal**
   - Measure baseline current draw
   - Monitor temperature over 1 hour run
   - Log power budget for Phase 3 planning

**Acceptance Criteria**:
- [ ] Firmware boots and initializes FreeRTOS
- [ ] All 4 tasks execute at target rates (jitter <10 ms)
- [ ] Sensor data is valid and varying
- [ ] Control loop executes without crashes

---

## Testing Strategy

### Unit Tests
```bash
# Continue Phase 1 tests (no changes to APIs)
ctest --test-dir build --output-on-failure
# Expected: 3/3 passing
```

### Integration Tests (New in Phase 2)

#### ITest 2.1: I2C Bus Communication
```c
// File: tests/integration/test_i2c_mpu6050.c
// Test: Verify I2C write/read cycle on MPU6050
// Setup: Pico board with MPU6050 on I2C0 (GPIO4=SDA, GPIO5=SCL)
// Expected: Read chip ID (0x68) from register 0x75
```

#### ITest 2.2: Task Scheduling
```c
// File: tests/integration/test_freertos_scheduling.c
// Test: Measure task jitter over 1 minute
// Expected: All tasks meet deadline; jitter < 10 ms
```

#### ITest 2.3: Control Loop (Hardware)
```c
// File: tests/integration/test_control_loop_hardware.c
// Test: Run full control loop with real sensors
// Expected: Attitude error converges; no crashes
```

---

## Risk Mitigation

| Risk | Impact | Mitigation |
|------|--------|-----------|
| Pico SDK fetch fails (network) | Blocks build | Use cached SDK or offline sources |
| I2C bus contention | Timing issues | Implement proper timeouts, error handling |
| FreeRTOS dual-core complexity | Hard to debug | Keep Core 1 unused initially; test single-core first |
| Sensor hardware not available | Can't validate | Use simulator (QEMU/Wokwi) or skip Phase 2.4 |
| Task jitter exceeds 10 ms | Control instability | Profile and optimize hot paths; consider OS tick rate |
| Flash/RAM exhaustion | Firmware doesn't fit | Profile memory; strip non-essential features if needed |

---

## Success Criteria

**Phase 2 is COMPLETE when**:

1. ✅ Pico SDK integrated; firmware compiles and runs on RP2040
2. ✅ Real FreeRTOS kernel active; 4 tasks execute with <10 ms jitter
3. ✅ MPU6050 driver reads sensor data (non-zero, varying values)
4. ✅ Temperature sensor functional
5. ✅ Control loop closes with real sensor feedback (attitude error decreases)
6. ✅ All tests passing (unit + integration)
7. ✅ Documentation: build guide, pin mapping, testing checklist complete
8. ✅ Hardware validated on Pico 2W or simulator

---

## Dependencies & Constraints

### External Dependencies
- Raspberry Pi Pico SDK (official GitHub repo)
- FreeRTOS kernel (included in SDK or standalone)
- arm-none-eabi toolchain (GCC)
- CMake 3.13+

### Hardware Requirements
- Raspberry Pi Pico 2W (or Pico 1 as fallback)
- Micro-USB cable for flashing/power
- MPU6050 breakout board (optional for Phase 2.4; Phase 2.3 via simulation)
- Logic analyzer or oscilloscope (optional; for I2C debugging)

### Constraints
- No dynamic memory allocation in flight code (static only)
- Task timing budget: sensor <50 ms, control <25 ms, telemetry <500 ms, health <1000 ms
- RAM budget: <60 KB (Pico has 264 KB; comfortable margin)
- Flash budget: <200 KB (Pico has 2 MB; comfortable margin)
- Dual-core: Core 0 reserved for OS; Core 1 future expansion

---

## Communication & Handoffs

### Status Check-ins
- **Weekly**: Brief sync on blockers, progress on WBS tasks
- **After Each Task**: PR review + merge to `dev` before proceeding

### Branch Management
- Feature branch: `feature/pico-sdk-integration`
- Create sub-branches if needed: `feature/pico-i2c-driver`, `feature/mpu6050-driver`
- Merge back to `dev` after review; eventually to `main` for release

### Documentation
- Update `docs/NEXT_STEPS.md` weekly with progress
- Add comments to code: rationale for design decisions
- Record lessons learned in `post-phase2-review.md`

---

## Timeline & Milestones

| Milestone | Target Date | Acceptance Criteria |
|-----------|-------------|-------------------|
| **M2.1**: Pico SDK Setup | Feb 18 | SDK builds, blink demo works |
| **M2.2**: FreeRTOS Real | Feb 22 | 4 tasks scheduling, jitter <10 ms |
| **M2.3**: I2C + Sensors | Mar 1 | MPU6050 + temp readable |
| **M2.4**: Control Loop | Mar 5 | End-to-end loop closes |
| **M2.5**: Hardware Valid | Mar 8 | Firmware validated on Pico 2W |
| **Phase 2 Complete** | Early March | All criteria met, PR merged to `dev` |

---

## Approval & Sign-Off

**Phase 2 Plan Owner**: OBC Development Lead  
**Approved By**: Project Manager / Tech Lead  
**Date Approved**: 2026-02-15  
**Next Review**: Weekly sync or as needed

---

**Last Updated**: 2026-02-15  
**Plan Version**: 1.0  
**Status**: Ready for implementation
