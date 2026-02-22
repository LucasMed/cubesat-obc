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

## 2. Integration Tests (Phase 2 — ✅ Complete)

### 2.1 ITest: I2C Communication (MPU6050)
- **File**: `tests/integration/test_i2c_mpu6050.c`
- **Covers**: FR-1
- **Setup**: Pico 2W with MPU6050 on I2C0 (GPIO4=SDA, GPIO5=SCL)
- **Expected**: Read chip ID (0x68) from register 0x75
- **Status**: ✅ PASS

### 2.2 ITest: FreeRTOS Task Scheduling
- **File**: `tests/integration/test_freertos_scheduling.c`
- **Covers**: NFR-1, NFR-2
- **Expected**: All tasks meet deadline; jitter <10 ms over 1 minute
- **Status**: ✅ PASS (Resolved flash.c blocker and FPU incompatibilities)

### 2.3 ITest: Control Loop (Hardware)
- **File**: `tests/integration/test_control_loop_hardware.c`
- **Covers**: FR-1 → FR-3 → FR-5 (sensor → control → actuator)
- **Expected**: Attitude error converges; stabilization within 30 s
- **Status**: ✅ PASS

---

## 3. Communication Tests (Phase 3 — ✅ Complete)

### 3.1 Unit: Telemetry Task
- **File**: `tests/unit/test_telemetry.c`
- **Covers**: CSP Telemetry Packing
- **Expected**: `vTelemetryTask_Step` correctly packs `system_state_t` into `csp_telemetry_packet_t` and calls `csp_sendto`.
- **Status**: ✅ PASS

### 3.2 Unit: Command Task
- **File**: `tests/unit/test_command.c`
- **Covers**: CSP Command Parsing
- **Expected**: Successfully parse `CMD_ECHO`, `CMD_REBOOT`, and unknown commands without crashing.
- **Status**: ✅ PASS

### 3.3 Unit/Integration: CSP Initialization
- **File**: `tests/unit/test_comm_init.c`
- **Covers**: `comm_init.c`
- **Expected**: Successfully initialize `libcsp` mock over UART KISS interface and verify routing.
- **Status**: ✅ PASS

### 3.4 System: End-to-End Ground Station Link
- **Covers**: FR-7
- **Expected**: Valid CSP telemetry received at external node (1 Hz), remote commands executed.
- **Status**: ⏳ Pending


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

### Test Coverage (Using gcov/gcovr)
```bash
# To generate line coverage reports (Host build required)
mkdir -p build_host && cd build_host
cmake -DPICO_ENABLED=OFF ..
make -j$(nproc)
make test
gcovr -r ../src .
```

---

## 6. Test Coverage Summary

| Metric | Current (%) | Target (%) |
|--------|-------------|------------|
| Line Coverage | **64%** (179/279) | > 80% |

*Note: The test suite was heavily expanded during Phase 3 to cover `telemetry_task.c` (73%), `command_task.c` (58%), and `comm_init.c` (100%). The missing coverage is exclusively restricted to FreeRTOS infinite loop wrappers (`while(1)`) and hardware-specific `#ifdef PICO_BUILD` branches that cannot be executed during host testing.*
