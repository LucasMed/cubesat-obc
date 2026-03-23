# ADCS-SIM-001 — ADCS Simulation Design

| Field | Value |
|-------|-------|
| Document ID | ADCS-SIM-001 |
| Version | 1.0 |
| Status | Draft |
| Subsystem | ADCS Simulation |
| Date | 2026-03-21 |
| Related docs | ADCS-DES-001, FSW-SDD-001, SVVP-OBC-001 |

---

## 1. Purpose and Scope

### 1.1 Purpose

This document defines the simulation architecture, models, scenarios, and validation criteria for the Attitude Determination and Control System (ADCS) software verification. The simulation provides a high-fidelity software-in-the-loop (SIL) environment to validate ADCS algorithms before flight hardware integration.

### 1.2 Scope

The ADCS simulation encompasses:

- Quaternion-based attitude dynamics with Euler's equations for rigid-body dynamics
- Extended Kalman Filter (EKF) state estimation (6-state: quaternion + gyroscope bias)
- Linear Quadratic Regulator (LQR) attitude control
- Sensor models: IMU (accelerometer/gyroscope), magnetometer, GPS
- Actuator models: reaction wheels, magnetorquers
- Operational modes: Detumbling, Nominal Sun-Pointing, Safe Mode
- Fault injection and edge-case validation

The simulation excludes:

- Mechanical vibration coupling
- Thermal effects on sensor/actuator performance
- Radiation-induced single-event effects (handled separately in fault design)

---

## 2. Simulation Architecture

### 2.1 High-Level Structure

```
┌─────────────────────────────────────────────────────────────────────┐
│                     ADCS Simulation Environment                     │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│  ┌──────────────┐    ┌──────────────┐    ┌──────────────────────┐   │
│  │   Scenario   │───▶│  Simulation  │───▶│   Data Logging &     │   │
│  │   Manager    │    │   Engine     │    │   Visualization      │   │
│  └──────────────┘    └──────┬───────┘    └──────────────────────┘   │
│                             │                                       │
│        ┌────────────────────┼────────────────────┐                  │
│        ▼                    ▼                    ▼                  │
│  ┌───────────┐        ┌───────────┐        ┌───────────┐            │
│  │ Dynamics  │        │ Estimator │        │ Controller│            │
│  │   Model   │◀──────▶│   (EKF)   │◀──────▶│  (LQR)    │            │
│  └─────┬─────┘        └─────┬─────┘        └─────┬─────┘            │
│        │                    │                    │                  │
│        ▼                    ▼                    ▼                  │
│  ┌───────────┐        ┌───────────┐        ┌───────────┐            │
│  │  Sensor   │        │  Sensor   │        │  Actuator │            │
│  │   Noise   │        │   Bias    │        │   Models  │            │
│  └───────────┘        └───────────┘        └───────────┘            │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

### 2.2 Simulation Loop

```
while (t < t_end):
    1. scenario_update(t)          // Update external commands, faults
    2. dynamics_propagate(dt)      // Propagate attitude dynamics
    3. sensor_measurements()       // Generate corrupted sensor data
    4. ekf_predict(dt)             // EKF time update
    5. ekf_update(measurements)    // EKF measurement update
    6. controller_compute()        // Compute control actions
    7. actuator_command()          // Apply actuator commands
    8. log_state(t)                // Record state for analysis
```

### 2.3 Component Mapping

| Component | Source File | Description |
|-----------|-------------|-------------|
| Scenario Manager | `sim/scenario_manager.c` | Defines mission scenarios and timeline |
| Dynamics Engine | `sim/dynamics/attitude_dynamics.c` | Rigid-body kinematics and dynamics |
| EKF Estimator | `src/control/ekf.c` | (Shared with flight software) |
| LQR Controller | `src/control/lqr.c` | (Shared with flight software) |
| Sensor Models | `sim/models/sensors/*.c` | IMU, magnetometer, GPS simulation |
| Actuator Models | `sim/models/actuators/*.c` | Reaction wheel, magnetorquer simulation |
| Data Logger | `sim/utils/data_logger.c` | CSV/hDF5 output generation |

---

## 3. Simulation Models

### 3.1 Attitude Dynamics Model

#### 3.1.1 Quaternion Kinematics

The spacecraft attitude is represented by a unit quaternion **q** = [q₀, q₁, q₂, q₃] where q₀ is the scalar component. The quaternion kinematics equation is:

```
q̇ = 0.5 * Ω(q) * ω_body

where:
  Ω(q) = [ -q₁ -q₂ -q₃
           q₀ -q₃  q₂
           q₃  q₀ -q₁
          -q₂  q₁  q₀ ]
  ω_body = [ω_x, ω_y, ω_z]ᵀ (body-frame angular velocity)
```

The quaternion is normalized after each integration step to maintain unit magnitude.

#### 3.1.2 Euler's Equations (Rotational Dynamics)

```
I · ω̇_body + ω_body × (I · ω_body) = τ_external + τ_control

where:
  I = [Iₓₓ Iᵧᵧ Iᵤᵤ]ᵀ (inertia matrix, diagonal for symmetric CubeSat)
  τ_external = disturbance torques (solar pressure, residual magnetic dipole)
  τ_control = actuator-generated torques
```

#### 3.1.3 Disturbance Torques

| Source | Model | Typical Magnitude |
|--------|-------|-------------------|
| Solar pressure | A · P · Cr · ĉ | 10⁻⁶ to 10⁻⁵ N·m |
| Magnetic residual | μ × B | 10⁻⁸ to 10⁻⁷ N·m |
| Atmospheric drag | 0.5 · ρ · V² · C_d · A | 10⁻⁸ to 10⁻⁷ N·m (LEO) |

### 3.2 Extended Kalman Filter Model

#### 3.2.1 State Vector (6-State)

```
x = [q₀, q₁, q₂, q₃, bₓ, bᵧ, bᵤ]ᵀ

where:
  q = attitude quaternion (4 states, constrained ||q|| = 1)
  b = gyroscope bias (rad/s)
```

#### 3.2.2 State Transition Model

```
x_k = f(x_{k-1}, u_{k-1}) + w_k

f(x, u) = [ quaternion_propagation(q, ω - b)
           b ]  // bias treated as random walk
```

Process noise **w** models:

- Gyroscope random walk
- Unmodeled angular accelerations
- Quaternion integration errors

#### 3.2.3 Measurement Models

| Sensor | Measurement | Model |
|--------|-------------|-------|
| Gyroscope | ω_meas = ω_true + b + v_gyro | z_gyro = [ω] + b + v |
| Magnetometer | B_meas = R(q) · B_inertial + v_mag | z_mag = h(x) + v |
| Sun sensor | s_meas = R(q) · s_sun + v_sun | z_sun = h(x) + v |

#### 3.2.4 EKF Parameters

| Parameter | Value |
|-----------|-------|
| State dimension | 6 |
| Process noise (q) | 1×10⁻⁴ (quaternion) |
| Process noise (bias) | 1×10⁻⁶ rad/s/sqrt(s) |
| Gyro noise | 0.01 rad/s |
| Mag noise | 1.0 μT |
| Initial covariance | diag([0.1, 0.1, 0.1, 0.1, 1e-3, 1e-3, 1e-3]) |

### 3.3 LQR Controller Model

#### 3.3.1 Control Problem Formulation

The LQR minimizes the cost function:

```
J = ∫₀^∞ (xᵀ Q x + uᵀ R u) dt
```

subject to linearized dynamics:

```
ẋ = A x + B u
```

#### 3.3.2 Linearized Dynamics

The quaternion is linearized about the desired attitude q_d:

```
δθ = 2 · q_d^{-1} ⊗ q  // small angle rotation vector

Linearized: δθ̇ = ω_body
           ω̇ = I^{-1} (τ_control - ω × Iω)
```

#### 3.3.3 Gain Scheduling

| Mode | Q-diagonal | R-diagonal | Description |
|------|------------|------------|-------------|
| Detumble | [10,10,10, 0,0,0] | [1,1,1] | Aggressive rate damping |
| Nominal | [100,100,100, 10,10,10] | [0.1, 0.1, 0.1] | Precise pointing |
| Safe | [1,1,1, 0,0,0] | [10,10,10] | Conservative, minimal torque |

#### 3.3.4 LQR Parameters

| Parameter | Value |
|-----------|-------|
| Control horizon | Infinite (steady-state LQR) |
| Sample rate | 10 Hz |
| Integral action | None (error integration in outer loop) |

### 3.4 Sensor Models

#### 3.4.1 IMU Model (MPU6050)

```
gyro_meas = ω_true + bias_gyro + noise_gyro
accel_meas = a_true + bias_accel + noise_accel

where:
  noise_gyro ~ N(0, σ_gyro²), σ_gyro = 0.01 rad/s
  noise_accel ~ N(0, σ_accel²), σ_accel = 0.1 m/s²
  bias_gyro = random constant, initialized ±0.1 rad/s
  bias_accel = random constant, initialized ±0.5 m/s²
```

| Parameter | Typical Value | Units |
|-----------|---------------|-------|
| Gyro range | ±250 | deg/s |
| Accel range | ±2 | g |
| Sample rate | 100 | Hz |
| Gyro noise density | 0.005 | deg/s/√Hz |
| Accel noise density | 0.1 | mg/√Hz |

#### 3.4.2 Magnetometer Model (HMC5883L)

```
B_meas = R(q) · B_inertial + bias_mag + noise_mag

where:
  bias_mag = random constant, initialized ±5 μT
  noise_mag ~ N(0, σ_mag²), σ_mag = 1.0 μT
```

| Parameter | Typical Value | Units |
|-----------|---------------|-------|
| Range | ±1.3 | Gauss |
| Resolution | 0.92 | mG/LSB |
| Sample rate | 15 | Hz |
| Noise | 0.5 | mG RMS |

#### 3.4.3 GPS Model

```
position_meas = position_true + noise_position
velocity_meas = velocity_true + noise_velocity

where:
  noise_position ~ N(0, σ_pos²), σ_pos = 10 m (1σ)
  noise_velocity ~ N(0, σ_vel²), σ_vel = 1 m/s (1σ)
```

| Parameter | Typical Value | Units |
|-----------|---------------|-------|
| Position accuracy | 10 | m (1σ) |
| Velocity accuracy | 1 | m/s (1σ) |
| Update rate | 1 | Hz |
| Time accuracy | 100 | ns |

### 3.5 Actuator Models

#### 3.5.1 Reaction Wheel Model

```
ω_wheel_dot = (τ_command - τ_friction) / J_wheel
τ_output = k_t · ω_wheel  // back-EMF torque reduction
τ_friction = c_v · ω_wheel + c_s · sign(ω_wheel)

where:
  J_wheel = 1e-5  kg·m² (3U CubeSat reaction wheel)
  k_t = 1e-4     N·m/(rad/s) (torque constant)
  c_v = 1e-8     N·m/(rad/s) (viscous friction)
  c_s = 1e-7     N·m (static friction)
```

| Parameter | Value | Units |
|-----------|-------|-------|
| Max torque | 0.001 | N·m |
| Max speed | 6000 | RPM |
| Momentum capacity | 0.02 | N·m·s |
| Torque constant | 1×10⁻⁴ | N·m/(rad/s) |

#### 3.5.2 Magnetorquer Model

```
m_command = command_vector · M_max
B_body = R(q) · B_inertial
τ_output = m_command × B_body

where:
  M_max = [0.2, 0.2, 0.2] A·m² (per axis)
```

| Parameter | Value | Units |
|-----------|-------|-------|
| Magnetic moment | 0.2 | A·m² (per axis) |
| Coil area | 0.01 | m² |
| Turns | 100 | - |
| Power consumption | 0.5 | W (per axis) |

---

## 4. Simulation Scenarios

### 4.1 Detumbling Mode

**Objective:** Reduce initial angular momentum to near-zero.

**Initial Conditions:**

- Angular velocity: random, magnitude 5-15 deg/s per axis
- Attitude: random quaternion
- EKF state: initialized with high covariance

**Timeline:**

| Time (s) | Event |
|----------|-------|
| 0 | Simulation start, random initial state |
| 0-60 | Detumble control (B-dot law) |
| 60+ | Transition to nominal mode if \|ω\| < 0.1 deg/s |

**Success Criteria:**

| Criterion | Threshold |
|-----------|-----------|
| Final angular rate | < 0.1 deg/s (all axes) |
| Convergence time | < 60 s |
| Maximum actuator duty | < 100% |

### 4.2 Nominal Sun-Pointing Mode

**Objective:** Point +Z axis toward Sun (±X axis Sun-ward).

**Initial Conditions:**

- Angular velocity: < 1 deg/s
- Attitude: random within 30° of Sun-pointing
- EKF: valid estimate from previous mode

**Timeline:**

| Time (s) | Event |
|----------|-------|
| 0 | LQR controller engaged |
| 0-120 | Pointing acquisition |
| 120+ | Steady-state pointing |

**Reference Vectors:**

- Target Sun vector: [1, 0, 0] (body frame, +X toward Sun)
- Nadir vector: [0, 0, 1] (body frame, +Z toward Earth)

**Success Criteria:**

| Criterion | Threshold |
|-----------|-----------|
| Pointing error | < 5° (Sun axis) |
| Pointing error | < 10° (nadir axis) |
| Steady-state error | < 2° (after 60s) |
| Angular rate | < 0.5 deg/s |
| Control effort | < 50% actuator capacity |

### 4.3 Safe Mode

**Objective:** Maintain spacecraft survival attitude with minimal power and hardware.

**Trigger Conditions:**

- EKF failure (divergence or timeout)
- Actuator failure detection
- Ground command
- Power bus undervoltage

**Behavior:**

- Disable reaction wheels
- Enable magnetorquer detumble (B-dot)
- Reduce ADCS task rate to 1 Hz
- Log diagnostics at 1 Hz

**Success Criteria:**

| Criterion | Threshold |
|-----------|-----------|
| Angular rate | < 5 deg/s (survival) |
| Power consumption | < 1 W |
| Heartbeat | Maintained |
| Telemetry | Available |

### 4.4 Fault Scenarios

#### 4.4.1 Sensor Faults

| Fault | Injection | Expected Behavior |
|-------|-----------|-------------------|
| Gyro stuck | ω_meas = constant after t=30s | EKF divergence detection, fallback to PID |
| Mag dropout | B_meas = 0 after t=60s | EKF continues with gyro-only update |
| GPS loss | Position = last value after t=45s | No impact on attitude (non-essential) |

#### 4.4.2 Actuator Faults

| Fault | Injection | Expected Behavior |
|-------|-----------|-------------------|
| RW saturation | τ_command clamped at t=40s | Anti-windup activates, degrades pointing |
| Magnetorquer failure | m = 0 after t=50s | Transition to RW-only control |
| Dual failure | Both actuators fail at t=70s | Safe mode entry |

#### 4.4.3 EKF Faults

| Fault | Injection | Expected Behavior |
|-------|-----------|-------------------|
| Divergence | Process noise increased 100x at t=35s | Covariance explosion, fallback to PID |
| Measurement rejection | Innovation test fails for 5 consecutive cycles | Sensor excluded, mode maintains |

---

## 5. Validation Criteria

### 5.1 Functional Requirements

| ID | Requirement | Validation Method | Pass Criterion |
|----|-------------|-------------------|----------------|
| SIM-REQ-001 | Simulation accurately represents flight dynamics | Compare simulation against analytical solutions | Error < 1% on test cases |
| SIM-REQ-002 | EKF converges within 60s from random initial state | Monte Carlo (100 runs) | 95% success rate |
| SIM-REQ-003 | Detumble achieves rate < 0.1 deg/s in < 60s | Simulation runs | 100% pass |
| SIM-REQ-004 | Nominal mode achieves < 5° pointing error | Simulation runs | 95% pass |
| SIM-REQ-005 | Safe mode entry within 5s of fault trigger | Fault injection tests | 100% pass |

### 5.2 Performance Metrics

| Metric | Target | Measurement |
|--------|--------|-------------|
| Simulation speed | > 100x real-time | Wall-clock / simulation-time |
| State memory | < 10 MB | Peak heap usage |
| Determinism | < 1% timing variance | 100 runs, same input |

### 5.3 Verification Matrix

| Test Case | Detumble | Nominal | Safe Mode | Fault |
|-----------|----------|---------|-----------|-------|
| TC-SIM-001: Nominal operation | ✓ | ✓ | - | - |
| TC-SIM-002: Mode transitions | ✓ | ✓ | ✓ | - |
| TC-SIM-003: Gyro fault | - | - | - | ✓ |
| TC-SIM-004: Mag fault | - | ✓ | - | ✓ |
| TC-SIM-005: RW saturation | - | ✓ | - | ✓ |
| TC-SIM-006: Dual actuator failure | - | - | ✓ | ✓ |
| TC-SIM-007: EKF divergence | - | ✓ | - | ✓ |
| TC-SIM-008: Monte Carlo (100 runs) | ✓ | ✓ | - | - |

---

## 6. Simulation Parameters

### 6.1 Spacecraft Properties

| Parameter | Value | Units |
|-----------|-------|-------|
| Mass | 4.0 | kg |
| Inertia (Iₓₓ) | 0.02 | kg·m² |
| Inertia (Iᵧᵧ) | 0.02 | kg·m² |
| Inertia (Iᵤᵤ) | 0.01 | kg·m² |
| Center of mass offset | [0, 0, 0.01] | m |

### 6.2 Orbital Parameters (LEO Sun-Synchronous)

| Parameter | Value | Units |
|-----------|-------|-------|
| Altitude | 500 | km |
| Inclination | 97.5 | deg |
| Period | 94.5 | min |
| Eclipse fraction | 0.35 | - |

### 6.3 Magnetic Field Model

| Parameter | Value | Units |
|-----------|-------|-------|
| Model | IGRF-13 | - |
| Epoch | 2026-03-21 | - |
| Reference altitude | 500 | km |

### 6.4 Simulation Integration

| Parameter | Value | Units |
|-----------|-------|-------|
| Integration method | RK4 (dynamics) / Euler (EKF) | - |
| Fixed timestep | 0.01 | s |
| EKF update rate | 10 | Hz |
| Controller rate | 10 | Hz |
| Data logging rate | 10 | Hz |
| Total simulation time | 600 | s |

### 6.5 Monte Carlo Parameters

| Parameter | Value |
|-----------|-------|
| Number of runs | 100 |
| Initial attitude distribution | Uniform (SO(3)) |
| Initial rate distribution | Uniform [-15, 15] deg/s |
| Sensor bias distribution | Normal (σ = 1σ spec) |
| Random seed | Fixed for reproducibility |

---

## 7. References

| Document ID | Title |
|-------------|-------|
| ADCS-DES-001 | ADCS Design Document |
| FSW-SDD-001 | Flight Software Design Document |
| SVVP-OBC-001 | Software Verification and Validation Plan |
| ECSS-E-ST-40C | Software Engineering Standard |
| ECSS-E-ST-50C | Software Verification Standard |

---

*End of Document*