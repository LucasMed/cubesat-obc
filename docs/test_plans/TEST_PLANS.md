# Test Plans

**Document ID**: TST-001  
**Version**: 1.0  
**Last Updated**: 2026-02-20  
**Status**: Active

---

## 1. Unit Tests (Phase 1 — ✅ Complete)

### 1.1 test_pid
- **File**: `tests/unit/test_pid.c`
- **Covers**: FR-3 (Rate Control)
- **Cases**:
  - ✅ Anti-windup: integral term saturates correctly
  - ✅ Output saturation: command clipped to torque limits
  - ✅ Zero-error steady-state: near-zero output
  - ✅ Step response: error reduction over iterations
- **Result**: PASS (100%)

### 1.2 test_dynamics
- **File**: `tests/unit/test_dynamics.c`
- **Covers**: FR-2 (Attitude Determination), FR-4 (Attitude Control)
- **Cases**:
  - ✅ Zero-torque: attitude remains constant
  - ✅ Constant-torque: angular acceleration correct
  - ✅ Numerical stability: no NaN/inf over 100 iterations
  - ✅ Energy conservation: kinetic energy bounds reasonable
- **Result**: PASS (100%)

### 1.3 test_actuators
- **File**: `tests/unit/test_actuators.c`
- **Covers**: FR-5 (RW Actuation), FR-6 (Magnetorquer)
- **Cases**:
  - ✅ RW torque scaling: linear momentum change
  - ✅ RW saturation: max torque enforced
  - ✅ Magnetorquer dipole: correct magnetic moment
  - ✅ Momentum limits: accumulation capped
- **Result**: PASS (100%)

---

## 2. Integration Tests (Phase 2 — TBD)

### 2.1 ITest: I2C Communication (MPU6050)
- **File**: `tests/integration/test_i2c_mpu6050.c` (TBD)
- **Covers**: FR-1
- **Setup**: Pico 2W with MPU6050 on I2C0 (GPIO4=SDA, GPIO5=SCL)
- **Expected**: Read chip ID (0x68) from register 0x75
- **Status**: ⏳ Pending Task 2.3

### 2.2 ITest: FreeRTOS Task Scheduling
- **File**: `tests/integration/test_freertos_scheduling.c` (TBD)
- **Covers**: NFR-1, NFR-2
- **Expected**: All tasks meet deadline; jitter <10 ms over 1 minute
- **Status**: ⏳ Pending Task 2.2c (flash.c blocker resolution)

### 2.3 ITest: Control Loop (Hardware)
- **File**: `tests/integration/test_control_loop_hardware.c` (TBD)
- **Covers**: FR-1 → FR-3 → FR-5 (sensor → control → actuator)
- **Expected**: Attitude error converges; stabilization within 30 s
- **Status**: ⏳ Pending Task 2.4

---

## 3. System Tests (Phase 3 — TBD)

### 3.1 WiFi Telemetry Packet Test
- **Covers**: FR-7
- **Expected**: Packets received by ground station, 100% success at 1 Hz
- **Status**: ⏳ Pending Phase 3

### 3.2 Health Monitor Watchdog Test
- **Covers**: FR-8, SR-1, SR-2
- **Expected**: Safe mode entered within 5 s on task stall
- **Status**: ⏳ Pending Phase 5

---

## 4. Validation Tests (Phase 4+ — TBD)

### 4.1 Kalman Filter Attitude Estimation
- **Covers**: FR-2 (enhanced)
- **Expected**: RMS error <5° over 10 min simulation
- **Status**: ⏳ Pending Phase 4

### 4.2 Power Budget Validation
- **Covers**: NFR-4
- **Expected**: Average power <2 W
- **Status**: ⏳ Pending Phase 3 (requires hardware measurement)

---

## 5. Test Execution

### Running Unit Tests
```bash
cd /home/ljm/Dev/cubesat-obc/build
cmake .. && cmake --build .
ctest --output-on-failure --verbose
```

### Running Specific Tests
```bash
ctest --test-dir build -R test_pid --output-on-failure
```

### Static Analysis
```bash
scripts/static_analysis.sh
# or manually:
cppcheck --enable=all --suppress=missingInclude src/ include/
```

---

## 6. Test Coverage Summary

| Category | Total Tests | Passing | Pending | Blocked |
|----------|-------------|---------|---------|---------|
| Unit | 3 | 3 | 0 | 0 |
| Integration | 3 | 0 | 2 | 1 (flash.c) |
| System | 2 | 0 | 2 | 0 |
| Validation | 2 | 0 | 2 | 0 |
| **Total** | **10** | **3** | **6** | **1** |
