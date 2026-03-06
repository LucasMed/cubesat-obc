# Test Plans

**Document ID**: TST-001  
**Version**: 3.0  
**Last Updated**: 2026-03-08  
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
- **Covers**: FR-2 (Attitude Determination), FR-3 (Attitude Dynamics)
- **Test IDs**: T-DYN-01..05
- **Cases**:
  - ✅ T-DYN-01: Zero-torque — attitude remains constant
  - ✅ T-DYN-02: Constant-torque — angular acceleration correct
  - ✅ T-DYN-03: Numerical stability — no NaN/inf over 100 iterations
  - ✅ T-DYN-04: RK2 accuracy — attitude error < 0.1 mrad vs analytic over 1 s
  - ✅ T-DYN-05: RK2 vs Euler — midpoint method improves accuracy by ≥10×
- **Result**: PASS 5/5

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
- **Covers**: DLA write path, unit conversion, EKF sensor fusion (PR-14)
- **Test IDs**: T-SDM-01..07, T-SRF-08..10
- **Cases**:
  - ✅ Task calls `data_layer_write_imu()` with converted gyro (rad/s)
  - ✅ Gyro deg/s → rad/s conversion: factor `π/180` verified numerically
  - ✅ `data_layer_write_temp()` populated from HAL
  - ✅ Task skips write when `imu_available == false`
  - ✅ Task skips write when `temp_available == false`
  - ✅ No direct access to `system_state_t` (compile-time isolation)
  - ✅ Driver failure (`mpu6050_read_raw` returns -1) leaves `imu_valid` false
  - ✅ T-SRF-08: `imu_ekf_valid` set to `true` after successful IMU read
  - ✅ T-SRF-09: EKF attitude, bias, and uncertainty stored in DLA are finite
  - ✅ T-SRF-10: `imu_ekf_valid` stays `false` when `mpu6050_read_raw` fails
- **Result**: PASS 10/10

### 4.6 test_attitude_control_task
- **File**: `tests/unit/test_attitude_control_task.c`
- **Covers**: FM guard, imu_valid guard, DLA read path, LQR/PID dispatch (PR-15)
- **Test IDs**: T-SDM-04..05, T-ACT-09..11
- **Cases**:
  - ✅ Task runs (computes torques) in FM_NOMINAL
  - ✅ Task runs in FM_DIAGNOSTIC
  - ✅ Task skips (returns immediately) in FM_SAFE
  - ✅ Task skips in FM_DETUMBLE
  - ✅ Task skips in FM_BOOT
  - ✅ Task skips when `imu_valid == false` (even in NOMINAL)
  - ✅ Task reads `attitude` and `rates` from DLA snapshot
  - ✅ `attitude_dynamics_step` receives torque from `attitude_ctrl_update`
  - ✅ T-ACT-09: FM_NOMINAL + `imu_ekf_valid` → `lqr_compute` called; `attitude_ctrl_update` NOT called
  - ✅ T-ACT-10: FM_NOMINAL + `imu_ekf_valid == false` → PID fallback; `attitude_ctrl_update` called
  - ✅ T-ACT-11: FM_DIAGNOSTIC + `imu_ekf_valid` → PID called (LQR excluded from DIAGNOSTIC mode)
- **Result**: PASS 11/11

### 4.7 test_telemetry (PR-9)
- **File**: `tests/unit/test_telemetry.c`
- **Covers**: Telemetry Task DLA migration, FM guard, energy state encoding
- **Test IDs**: T-TLM-01..06
- **Cases**:
  - ✅ T-TLM-01: Full ADCS fields sent in FM_NOMINAL (`csp_sendto` called, all fields populated)
  - ✅ T-TLM-02: HK-only in FM_SAFE — attitude/rates zeroed, temperature retained
  - ✅ T-TLM-03: `csp_sendto` called in all 5 flight modes (BOOT, SAFE, NOMINAL, DETUMBLE, DIAGNOSTIC)
  - ✅ T-TLM-04: Energy state encoded in `flags` bits[3:2] for all 4 energy levels
  - ✅ T-TLM-05: `imu_valid` / `temp_valid` reflected in `flags` bits 0–1
  - ✅ T-TLM-06: No `csp_sendto` when `csp_buffer_get` returns NULL
- **Result**: PASS 6/6

### 4.8 test_health_monitor_task (PR-10)
- **File**: `tests/unit/test_health_monitor_task.c`
- **Covers**: Health Monitor tick wiring
- **Test IDs**: T-HM-01..03
- **Cases**:
  - ✅ T-HM-01: `fault_manager_tick()` called exactly once per `vHealthMonitorTask_Step()`
  - ✅ T-HM-02: `eps_monitor_tick()` called exactly once per `vHealthMonitorTask_Step()`
  - ✅ T-HM-03: N successive Step calls → exactly N calls to each tick
- **Result**: PASS 3/3

---

## 5. Phase 4 — Advanced Control Tests (✅ Complete)

### 5.1 test_ekf (EKF Attitude Estimator)
- **File**: `tests/unit/test_ekf.c`
- **Covers**: FR-2 (Attitude Determination — EKF enhanced)
- **Test IDs**: T-EKF-01..06
- **Cases**:
  - ✅ T-EKF-01: `ekf_init()` produces valid zero-state with positive-definite P
  - ✅ T-EKF-02: `ekf_predict()` propagates attitude using bias-corrected gyro
  - ✅ T-EKF-03: `ekf_predict()` grows covariance P monotonically (no update)
  - ✅ T-EKF-04: `ekf_update()` reduces roll/pitch uncertainty vs accelerometer
  - ✅ T-EKF-05: Bias estimation converges: injected constant bias reduces over 50 ticks
  - ✅ T-EKF-06: Degenerate accelerometer input (near-zero vector) does not corrupt state
- **Result**: PASS 6/6

### 5.2 test_lqr (LQR Full-State Controller)
- **File**: `tests/unit/test_lqr.c`
- **Covers**: FR-4 (Attitude Control — LQR)
- **Test IDs**: T-LQR-01..07
- **Cases**:
  - ✅ T-LQR-01: `lqr_init()` sets default gains without NaN/inf
  - ✅ T-LQR-02: Zero attitude error + zero rates → zero torque command
  - ✅ T-LQR-03: Non-zero attitude error → non-zero torque in correct sign
  - ✅ T-LQR-04: Non-zero rate error → damping torque (sign check)
  - ✅ T-LQR-05: `lqr_set_gains()` overrides defaults; new torque matches manual K·x computation
  - ✅ T-LQR-06: Output linearity — doubling attitude error doubles torque (within 1e-5)
  - ✅ T-LQR-07: Closed-loop stability — coupled RK2 step + LQR converges attitude to zero
- **Result**: PASS 7/7

---

## 6. Validation Tests (Phase 5 — Pending)

### 6.1 Power Budget Validation
- **Covers**: NFR-4
- **Expected**: Average power <2 W
- **Status**: ⏳ Pending hardware measurement

### 6.2 End-to-End EKF Hardware Validation
- **Covers**: FR-2 (enhanced)
- **Expected**: RMS attitude error <5° over 10 min run on real hardware
- **Status**: ⏳ Pending Phase 5 hardware integration

---

## 7. Test ID Traceability Summary

| Test ID | Description | File | Status |
|---------|-------------|------|--------|
| T-FMM-01..11 | Flight Mode FSM transitions | `test_fmm.c` | ✅ 11/11 |
| T-FMS-02..04 | Fault reporting, CRITICAL→SAFE trigger, anti-cascade | `test_fault_manager.c` | ✅ 12/12 |
| T-EPS-03..05 | EPS Schmidt-trigger, energy state transitions, SAFE trigger | `test_eps_monitor.c` | ✅ 12/12 |
| T-LOG-01..03 | Logger ring buffer, Class-A protection, `log_read_recent` | `test_logger.c` | ✅ 12/12 |
| T-SDM-01..07 | DLA write from sensor task, unit conversion, driver failure | `test_sensor_read_task.c` | ✅ 10/10 |
| T-SRF-08..10 | EKF valid flag, finite outputs, flag on driver failure | `test_sensor_read_task.c` | ✅ 10/10 |
| T-SDM-04..05 | DLA read from control task, FM + imu guards | `test_attitude_control_task.c` | ✅ 11/11 |
| T-ACT-09..11 | LQR/PID dispatch (NOMINAL+EKF, fallback, diagnostic) | `test_attitude_control_task.c` | ✅ 11/11 |
| T-TLM-01..06 | Telemetry DLA read, FM guard, energy flags, null buffer | `test_telemetry.c` | ✅ 6/6 |
| T-HM-01..03 | Health monitor tick wiring (`fault_manager_tick`, `eps_monitor_tick`) | `test_health_monitor_task.c` | ✅ 3/3 |
| T-DYN-01..05 | RK2 dynamics integrator accuracy vs Euler baseline | `test_dynamics.c` | ✅ 5/5 |
| T-EKF-01..06 | EKF init, predict, update, bias convergence, degenerate input | `test_ekf.c` | ✅ 6/6 |
| T-LQR-01..07 | LQR init, zero/non-zero error, gains override, stability | `test_lqr.c` | ✅ 7/7 |
| T-WDT-01..05 | Watchdog init, kick, enable, HAL stub, health-monitor wiring | `test_watchdog.c` | ✅ 5/5 |
| T-MDT-01..05 | Momentum dump threshold, FM guard, B-dot duty cycle, DLA write | `test_momentum_dump.c` | ✅ 5/5 |
| T-MAG-01..04 | HMC5883L init, read, unit conversion, DLA mag fields | `test_hmc5883l.c` | ✅ 4/4 |
| T-EKFM-01..06 | EKF mag update: no-crash, degenerate guard, correction, wrap, cov, convergence | `test_ekf_mag.c` | ✅ 6/6 |

---

## 8. Test Execution

### Running Unit Tests
```bash
cd /workspace/build
cmake .. && cmake --build .
ctest --output-on-failure --verbose
```

### Running Specific Tests
```bash
ctest --test-dir /workspace/build -R test_ekf --output-on-failure
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

## 9. Test Coverage Summary

| Metric | Phase 3 | Spec-Alignment (PRs 1–10) | Phase 4 (PRs 11–15) | Phase 5 (PRs 16–19) | Target |
|--------|---------|---------------------------|----------------------|----------------------|--------|
| Line Coverage | 64% | ~80% | ~85% (estimated) | ~88% (estimated) | >85% |
| Test targets | 6 | 17 | 19 | **23** | ≥19 |
| Tests passing | 6/6 | 17/17 | 19/19 | **23/23** | 23/23 |
| New test IDs | — | T-FMM, T-FMS, T-EPS, T-LOG, T-SDM, T-TLM, T-HM | T-DYN, T-EKF, T-LQR, T-SRF, T-ACT | **T-WDT, T-MDT, T-MAG, T-EKFM** | — |

*Note: Phase 4 added two new test executables (`test_ekf`, `test_lqr`) and expanded `test_sensor_read_task` (7→10) and `test_attitude_control_task` (8→11). Missing coverage remains restricted to FreeRTOS `while(1)` task loops and `#ifdef PICO_BUILD` hardware branches not reachable in host builds.*
