# Test Plans

**Document ID**: TST-001  
**Version**: 2.0  
**Last Updated**: 2026-03-01  
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

## 4. Spec-Alignment Tests (PRs 3–8 — ✅ Complete)

### 4.1 test_fmm (Flight Mode Manager)
- **File**: `tests/unit/test_flight_mode_manager.c`
- **Covers**: FMM FSM (BOOT / SAFE / DETUMBLE / NOMINAL / DIAGNOSTIC)
- **Test IDs**: T-FMM-01..11
- **Cases**:
  - ✅ Boot → SAFE valid transition
  - ✅ SAFE → NOMINAL allowed
  - ✅ NOMINAL → DETUMBLE allowed
  - ✅ NOMINAL → DIAGNOSTIC allowed
  - ✅ BOOT → NOMINAL rejected (invalid transition)
  - ✅ `fmm_force_safe()` from any state
  - ✅ Guard: no reentrancy from same state
  - ✅ 4 additional edge cases
- **Result**: PASS 11/11

### 4.2 test_fault_manager (Fault Manager)
- **File**: `tests/unit/test_fault_manager.c`
- **Covers**: T-FMS-02, T-FMS-03, T-FMS-04
- **Cases**:
  - ✅ `fault_report()` stores event in 32-slot table
  - ✅ `fault_is_active()` returns correct status
  - ✅ `fault_clear()` removes event
  - ✅ `fault_get_event(0)` returns false (ID=0 sentinel guard)
  - ✅ `fault_get_highest_level()` returns CRITICAL when any CRITICAL active
  - ✅ CRITICAL fault triggers `fmm_force_safe()` on `fault_manager_tick()`
  - ✅ WARNING faults auto-clear after 30 ticks
  - ✅ Anti-cascade: table stays consistent when full
  - ✅ 4 additional FSM edge cases
- **Result**: PASS 12/12

### 4.3 test_eps_monitor (EPS Monitor)
- **File**: `tests/unit/test_eps_monitor.c`
- **Covers**: T-EPS-03, T-EPS-04, T-EPS-05
- **Cases**:
  - ✅ `eps_monitor_tick()` reads HAL and populates snapshot
  - ✅ Downward voltage transition (NOMINAL→LOW→CRITICAL→EMERGENCY) immediate
  - ✅ Upward transition requires V > threshold + 0.1 V hysteresis
  - ✅ No flapping at exact threshold boundary
  - ✅ `eps_set_power()` does not cut OBC rail
  - ✅ fmm_force_safe() called when energy state reaches EMERGENCY
  - ✅ `data_layer_set_energy_state()` called on each state change
  - ✅ 5 additional edge / boundary cases
- **Result**: PASS 12/12

### 4.4 test_logger (Persistent Logger)
- **File**: `tests/unit/test_logger.c`
- **Covers**: T-LOG-01, T-LOG-02, T-LOG-03
- **Cases**:
  - ✅ `log_event()` stores entry in 320-slot ring
  - ✅ Ring wraps on overflow — oldest Class-C evicted first
  - ✅ Class-A (CRITICAL) entries never evicted when ring is full
  - ✅ `log_read_recent()` returns newest-first, skips cleared slots
  - ✅ `log_clear_info()` nullifies Class-C only
  - ✅ Event IDs from `log_event_ids.h` stored verbatim
  - ✅ `get_tick_ms()` host stub provides monotonic timestamps
  - ✅ 5 additional eviction / ordering edge cases
- **Result**: PASS 12/12

### 4.5 test_sensor_read_task
- **File**: `tests/unit/test_sensor_read_task.c`
- **Covers**: DLA write path, unit conversion
- **Cases**:
  - ✅ Task calls `data_layer_write_imu()` with converted gyro (rad/s)
  - ✅ Gyro deg/s → rad/s conversion: factor `π/180` verified numerically
  - ✅ `data_layer_write_temp()` populated from HAL
  - ✅ Task skips write when `imu_available == false`
  - ✅ Task skips write when `temp_available == false`
  - ✅ No direct access to `system_state_t` (compile-time isolation)
  - ✅ 1 additional coverage case
- **Result**: PASS 7/7

### 4.6 test_attitude_control_task
- **File**: `tests/unit/test_attitude_control_task.c`
- **Covers**: FM guard, imu_valid guard, DLA read path
- **Cases**:
  - ✅ Task runs (computes torques) in FM_NOMINAL
  - ✅ Task runs in FM_DIAGNOSTIC
  - ✅ Task skips (returns immediately) in FM_SAFE
  - ✅ Task skips in FM_DETUMBLE
  - ✅ Task skips in FM_BOOT
  - ✅ Task skips when `imu_valid == false` (even in NOMINAL)
  - ✅ Task reads `attitude` and `rates` from DLA snapshot
  - ✅ No direct access to `system_state_t`
- **Result**: PASS 8/8

---

## 5. Validation Tests (Phase 4+ — TBD)

### 5.1 Kalman Filter Attitude Estimation
- **Covers**: FR-2 (enhanced)
- **Expected**: RMS error <5° over 10 min simulation
- **Status**: ⏳ Pending Phase 4

### 5.2 Power Budget Validation
- **Covers**: NFR-4
- **Expected**: Average power <2 W
- **Status**: ⏳ Pending Phase 3 (requires hardware measurement)

---

## 6. Test ID Traceability Summary (Spec-Alignment)

| Test ID | Description | File | Status |
|---------|-------------|------|--------|
| T-FMM-01..11 | Flight Mode FSM transitions | `test_flight_mode_manager.c` | ✅ 11/11 |
| T-FMS-02..04 | Fault reporting, CRITICAL→SAFE trigger, anti-cascade | `test_fault_manager.c` | ✅ 12/12 |
| T-EPS-03..05 | EPS Schmidt-trigger, energy state transitions, SAFE trigger | `test_eps_monitor.c` | ✅ 12/12 |
| T-LOG-01..03 | Logger ring buffer, Class-A protection, `log_read_recent` | `test_logger.c` | ✅ 12/12 |
| T-SDM-01..03 (partial) | DLA write from sensor task, unit conversion | `test_sensor_read_task.c` | ✅ 7/7 |
| T-SDM-04..05 (partial) | DLA read from control task, FM + imu guards | `test_attitude_control_task.c` | ✅ 8/8 |

**Pending test IDs** (PRs 9–10):
- T-TLM-01..04 — `test_telemetry_task.c` (PR-9)
- T-FMS-01 — `tests/integration/test_fault_safe.c` (PR-10)
- T-EPS-02 / T-SAFE-01 — `tests/integration/test_safe_trigger.c` (PR-10)

---

## 7. Test Execution

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

## 8. Test Coverage Summary

| Metric | Phase 3 (%) | Current (Phase SA) | Target (%) |
|--------|-------------|---------------------|------------|
| Line Coverage | 64% (179/279) | ~75% (estimated, 16 test targets) | > 80% |
| Test targets | 6 | 16 | ≥ 18 (after PR-9/10) |
| Tests passing | 6/6 | 16/16 | 18/18 |

*Note: The test suite was heavily expanded during Phase 3 to cover `telemetry_task.c` (73%), `command_task.c` (58%), and `comm_init.c` (100%). The missing coverage is exclusively restricted to FreeRTOS infinite loop wrappers (`while(1)`) and hardware-specific `#ifdef PICO_BUILD` branches that cannot be executed during host testing.*
