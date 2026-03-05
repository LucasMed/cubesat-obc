# ADCS-DES-001 — Attitude Determination and Control System Design

| Field | Value |
|-------|-------|
| Document ID | ADCS-DES-001 |
| Version | 0.3 |
| Status | Draft |
| Subsystem | ADCS |
| Date | 2026-03-05 |
| Related docs | ICD-OBC-001, SPEC-2-ADCS v1.1, PHASE4_PLAN.md, PHASE5_PLAN.md, PHASE6_PLAN.md |

---

## 1. Purpose

This document describes the design of the Attitude Determination and Control System (ADCS)
implemented in the CubeSat OBC flight software.

The ADCS provides:

- spacecraft attitude estimation (6-state EKF)
- angular-rate detumbling (B×L cross-product control)
- three-axis coarse attitude control (LQR / PID)
- momentum management (magnetorquer B×L law)

The implementation targets embedded execution on the OBC (RP2350 MCU) running FreeRTOS.
All algorithms are hardware-independent and fully host-testable via the unit test suite.

---

## 2. ADCS Architecture

The ADCS software decomposes into four vertical layers:

```
Sensors
  │  raw IMU + mag data
  ▼
SensorReadTask  (10 Hz)
  │  validated snap.state via data_layer
  ▼
State Estimator — EKF (6-state)
  │  snap.state.attitude[3], snap.state.rates[3], snap.state.imu_ekf_valid
  ▼
AttitudeControlTask  (10 Hz) — controller dispatch
  │
  ├─ FM_BOOT / FM_SAFE ► return immediately — no actuator output
  ├─ FM_DETUMBLE ►►►►►►► momentum_dump_step()  →  magnetorquer (B-dot)
  ├─ FM_NOMINAL + EKF OK  ► LQR (mode-scheduled gains)  →  attitude_dynamics
  ├─ FM_NOMINAL + no EKF  ► PID fallback  →  attitude_dynamics
  └─ FM_DIAGNOSTIC  ►►►►►►►► PID  →  attitude_dynamics
```

**Component-to-source mapping:**

| Component | Implementation |
|-----------|---------------|
| Sensor interface | `src/drivers/imu/mpu6050.c`, `src/drivers/mag/hmc5883l.c` |
| State estimator | `src/control/ekf.c` (`ekf_t`, `ekf_predict`, `ekf_update`, `ekf_update_mag`) |
| Dynamics model | `src/dynamics/attitude_dynamics.c` (`attitude_dynamics_step` — RK2) |
| LQR controller | `src/control/lqr.c` + `src/control/lqr_schedule.c` |
| PID fallback | `src/control/pid_controller.c` (`pid_ctrl_t`) |
| Detumble / dump | `src/services/adcs/momentum_dump.c` (`momentum_dump_step`) |
| Actuator driver | `src/actuators/magnetorquer.c` (`magnetorquer_set_moment`) |
| Task — estimation | `src/tasks/sensor_read_task.c` (`vSensorReadTask`) |
| Task — control | `src/tasks/attitude_control_task.c` (`vAttitudeControlTask`) |

---

## 3. Reference Frames

### 3.1 Body Frame (B)

The spacecraft body frame is defined as:

```
+X → spacecraft forward (ram direction / velocity vector)
+Y → spacecraft right
+Z → spacecraft nadir (towards Earth)
```

The frame is **right-handed**: Z = X × Y. All sensor measurements, actuator
commands, and control torques are expressed in this frame.

### 3.2 Inertial Reference Frame (I)

An Earth-centred inertial frame is approximated for attitude propagation and
controller reference. Used by the EKF prediction step and LQR setpoint.

### 3.3 Magnetic Field Reference

The local geomagnetic field vector **B** is expressed in the body frame as
measured by the HMC5883L magnetometer. Tilt compensation (roll/pitch rotation
of **B** onto the horizontal plane) is applied inside `ekf_update_mag()` before
computing the yaw measurement.

Magnetic declination is configurable via:

```c
#define OBC_MAG_DECLINATION_RAD  0.0f   // default: equatorial / simulation
```

For a real mission this should be set to the IGRF value at the launch site
(e.g. Buenos Aires ≈ −0.0524 rad, Cape Canaveral ≈ −0.1134 rad).

---

## 4. Sensors

### 4.1 Summary

| Sensor | Quantity measured | Units | Driver |
|--------|------------------|-------|--------|
| MPU-6050 gyroscope | angular velocity **ω** | rad/s | `src/drivers/imu/mpu6050.c` |
| MPU-6050 accelerometer | specific force **a** | m/s² | `src/drivers/imu/mpu6050.c` |
| HMC5883L magnetometer | magnetic field **B** | µT | `src/drivers/mag/hmc5883l.c` |

### 4.2 Gyroscope

Measured quantity:

$$\boldsymbol{\omega} = [\omega_x, \omega_y, \omega_z]^T \quad [\text{rad/s}]$$

The gyroscope measures body angular rate. A slowly-varying sensor bias **b**
is estimated by the EKF and removed from the measurement before integration.

### 4.3 Accelerometer

Measured quantity:

$$\mathbf{a} = \mathbf{g}_B + \mathbf{a}_{linear} \quad [\text{m/s}^2]$$

At quasi-static attitude the dominant component is the gravity projection onto
the body frame. Used to derive:

$$\text{roll} = \text{atan2}(a_y, a_z), \quad
  \text{pitch} = \text{atan2}(-a_x, \sqrt{a_y^2 + a_z^2})$$

The EKF update is skipped when `|a| < ε` to avoid degenerate measurements.

### 4.4 Magnetometer

Measured quantity:

$$\mathbf{B} = [B_x, B_y, B_z]^T \quad [\mu\text{T}]$$

Used for:

- yaw estimation via tilt-compensated measurement inside `ekf_update_mag()`
- detumble and momentum dump actuator law

The horizontal field components after tilt compensation are:

$$B_{h,x} = B_x \cos p + B_y \sin r \sin p + B_z \cos r \sin p$$
$$B_{h,y} = B_y \cos r - B_z \sin r$$
$$\psi_\text{meas} = \text{atan2}(-B_{h,y},\ B_{h,x}) + \delta_\text{declination}$$

The update is skipped if the horizontal field magnitude is below a small epsilon.

> **HMC5883L field range note:** The HMC5883L full-scale range is ±1.3–8.1 Gauss
> (configurable). The expected geomagnetic field in LEO (500–700 km SSO) is
> **25–60 µT** (0.25–0.60 Gauss). The sensor must be configured for the lowest
> gain setting to avoid saturation, and should be calibrated for hard/soft-iron
> offsets before flight.

---

## 5. Attitude Representation

The flight software represents attitude using ZYX intrinsic Euler angles:

$$\mathbf{x}_{att} = [\phi,\ \theta,\ \psi]^T \quad [\text{rad}]$$

| Symbol | Name |
|--------|------|
| φ | roll |
| θ | pitch |
| ψ | yaw |

This representation was selected because:

- computationally lightweight for the RP2350 at 10 Hz rates
- sufficient accuracy for coarse pointing in Phase 1
- direct compatibility with accelerometer-derived roll/pitch and magnetometer yaw

**Known limitation:** Euler angles exhibit a kinematic singularity at pitch = ±90°
(gimbal lock). This is acceptable for the Phase 1 CubeSat attitude envelope.

**Future migration:** A quaternion library (`src/control/quaternion.c`) is already
implemented and unit-tested (T-QAT-01..05). The EKF state vector can be migrated
to quaternion representation in Phase 7+ without changes to the actuator or FMM layers.

---

## 6. State Estimation — Extended Kalman Filter

### 6.1 State Vector

$$\mathbf{x} = [\phi,\ \theta,\ \psi,\ b_x,\ b_y,\ b_z]^T \quad [\text{rad},\ \text{rad/s}]$$

| Index | Symbol | Description |
|-------|--------|-------------|
| 0 | φ | roll angle [rad] |
| 1 | θ | pitch angle [rad] |
| 2 | ψ | yaw angle [rad] |
| 3 | b_x | gyro bias x-axis [rad/s] |
| 4 | b_y | gyro bias y-axis [rad/s] |
| 5 | b_z | gyro bias z-axis [rad/s] |

Defined in `include/ekf.h`:

```c
#define EKF_N 6   // state: [roll, pitch, yaw, bx, by, bz]
#define EKF_M 2   // accelerometer measurement dimension: [roll_accel, pitch_accel]
```

> **Note on measurement dimensions:** The EKF implements two independent update steps
> with separate measurement vectors. These are NOT applied simultaneously:
> - `EKF_M_ACC = 2` — accelerometer update (roll + pitch), H ∈ ℝ²ˣ⁶
> - `EKF_M_MAG = 1` — magnetometer yaw update, H ∈ ℝ¹ˣ⁶
>
> The header constant `EKF_M = 2` covers the accelerometer case only.
> `ekf_update_mag()` uses a scalar (1×1) innovation internally.

### 6.2 Process Model (Continuous)

$$\dot{\boldsymbol{\phi}} = \boldsymbol{\omega} - \mathbf{b}$$
$$\dot{\mathbf{b}} = \mathbf{0} \quad \text{(random-walk, driven by process noise Q)}$$

where **ω** = measured gyro rate, **b** = estimated gyro bias.

### 6.3 EKF Equations (Discrete)

**Prediction (time update):**

$$\mathbf{x}_{k+1} = f(\mathbf{x}_k,\ \boldsymbol{\omega}_k) \quad \text{(RK2 integration, see §6.4)}$$

$$P_{k+1} = F_k\,P_k\,F_k^T + Q$$

where $F_k$ is the Jacobian of $f$ with respect to **x** (computed analytically
as a first-order approximation of the continuous process model).

**Measurement update (accelerometer):**

$$\mathbf{y}_{acc} = \mathbf{z}_{acc} - h_{acc}(\mathbf{x})$$
$$S_{acc} = H_{acc}\,P\,H_{acc}^T + R_{acc}$$
$$K_{acc} = P\,H_{acc}^T\,S_{acc}^{-1}$$
$$\mathbf{x} \leftarrow \mathbf{x} + K_{acc}\,\mathbf{y}_{acc}$$
$$P \leftarrow (I - K_{acc}\,H_{acc})\,P$$

**Measurement update (magnetometer yaw):**

$$y_{mag} = z_{mag} - \psi \quad \text{(innovation, wrapped to } [-\pi, +\pi]\text{)}$$
$$S_{mag} = H_{mag}\,P\,H_{mag}^T + r_{mag}$$
$$K_{mag} = P\,H_{mag}^T / S_{mag}$$
$$\mathbf{x} \leftarrow \mathbf{x} + K_{mag}\,y_{mag}$$
$$P \leftarrow (I - K_{mag}\,H_{mag})\,P$$

Both updates leave **b** (gyro bias) partially corrected via off-diagonal
terms in P that couple attitude to bias.

### 6.4 RK2 Midpoint Integration (Prediction Step)

The prediction step propagates the state using a 2nd-order Runge-Kutta
(midpoint) integrator, removing the O(dt²) truncation error of forward Euler:

$$\mathbf{k}_1 = f(\mathbf{x},\ \mathbf{u})$$
$$\mathbf{k}_2 = f\!\left(\mathbf{x} + \tfrac{dt}{2}\,\mathbf{k}_1,\ \mathbf{u}\right)$$
$$\mathbf{x}_{n+1} = \mathbf{x}_n + dt\,\mathbf{k}_2$$

The same RK2 scheme is used in `attitude_dynamics_step()` for rigid-body propagation
in simulation:

```c
// attitude_dynamics.c — midpoint RK2
rates_mid[i] = rates[i] + 0.5f * dt * (torque[i] / inertia[i]);
attitude[i]  = attitude[i] + dt * rates_mid[i];
rates[i]     = rates[i] + dt * (torque[i] / inertia[i]);
```

### 6.4 Noise Parameters (Default)

| Parameter | Value | Description |
|-----------|-------|-------------|
| Q_att | 1×10⁻⁴ rad²/step | Attitude process noise (gyro ~0.01 rad/s @ 10 Hz) |
| Q_bias | 1×10⁻⁶ (rad/s)²/step | Bias process noise (slow random-walk) |
| R_att | 1×10⁻² rad² | Accelerometer measurement noise (~0.06 rad σ) |
| r_mag | configurable | Magnetometer yaw measurement noise [rad²] |
| P₀_att | 1.0 rad² | Initial attitude covariance (large uncertainty) |
| P₀_bias | 1×10⁻² (rad/s)² | Initial bias covariance |

### 6.5 Accelerometer Update (Roll / Pitch)

Corrects roll and pitch using gravity vector direction.

Measurement function:

$$\mathbf{z}_{acc} = \begin{bmatrix} \phi_{accel} \\ \theta_{accel} \end{bmatrix}
= \begin{bmatrix} \text{atan2}(a_y, a_z) \\ \text{atan2}(-a_x, \sqrt{a_y^2 + a_z^2}) \end{bmatrix}$$

Measurement matrix H (2×6):

$$H_{acc} = \begin{bmatrix}
1 & 0 & 0 & 0 & 0 & 0 \\
0 & 1 & 0 & 0 & 0 & 0
\end{bmatrix}$$

### 6.6 Magnetometer Update (Yaw)

Corrects yaw using tilt-compensated atan2 of the horizontal field projection.
Innovation is wrapped to [−π, +π] before the Kalman update.

Measurement function:

$$z_{mag} = \psi_\text{meas} = \text{atan2}(-B_{h,y},\ B_{h,x}) + \delta$$

Measurement matrix H (1×6):

$$H_{mag} = [0\ \ 0\ \ 1\ \ 0\ \ 0\ \ 0]$$

**API:**

```c
void ekf_predict(ekf_t *ekf, const float gyro[3], float dt);
void ekf_update(ekf_t *ekf, const float accel[3]);        // roll + pitch
void ekf_update_mag(ekf_t *ekf, const float mag_uT[3], float declination_rad);
void ekf_get_attitude(const ekf_t *ekf, float att[3]);
void ekf_get_bias(const ekf_t *ekf, float bias[3]);
```

---

## 7. Control System

### 7.1 Controller Dispatch

`vAttitudeControlTask_Step()` selects the active controller based on flight mode
and EKF validity:

| Flight mode | EKF valid | Active controller | Output |
|-------------|-----------|------------------|--------|
| FM_DETUMBLE | — | `momentum_dump_step()` | magnetorquer dipole |
| FM_NOMINAL | yes | LQR + mode-scheduled gains | torque → `attitude_dynamics_step()` |
| FM_NOMINAL | no | PID fallback | torque → `attitude_dynamics_step()` |
| FM_DIAGNOSTIC | — | PID | torque → `attitude_dynamics_step()` |
| FM_BOOT / FM_SAFE | — | — *(no output)* | — |

### 7.2 LQR Controller

**Control law:**

$$\mathbf{u} = -K\,\mathbf{x}_e$$

**Error state (6×1):**

$$\mathbf{x}_e = [\phi_e,\ \theta_e,\ \psi_e,\ \dot\phi_e,\ \dot\theta_e,\ \dot\psi_e]^T
\quad [\text{rad},\ \text{rad/s}]$$

> **Rate error convention:** The attitude setpoint is nadir-pointing (zero attitude).
> The *rate* setpoint is also zero — the target is a stationary spacecraft. Therefore:
> `rate_err[i] = 0 − snap.state.rates[i]` = negative of the measured body rate.
> In the code: `lqr_compute(&g_lqr, att_err, snap.state.rates, torque)` passes the
> raw gyro rates directly; the LQR gain matrix K has already absorbed the sign.

**Control output (3×1):**

$$\mathbf{u} = [\tau_\phi,\ \tau_\theta,\ \tau_\psi]^T \quad [\text{N}\cdot\text{m}]$$

**Spacecraft inertia tensor:**

$$I = \text{diag}(0.01,\ 0.01,\ 0.005) \quad [\text{kg}\cdot\text{m}^2]$$

**LQR design parameters:**

| Parameter | Value |
|-----------|-------|
| Q | diag(100, 100, 100, 1, 1, 1) |
| R | I₃ |
| ωn (nominal) | 10 rad/s per axis |
| ζ | 1 (critically damped) |
| Settling time | ≈ 0.4 s from 30° initial error |

**Default gain matrix K (3×6):**

```
K = [ 1.00   0      0      0.20   0      0    ]
    [ 0      1.00   0      0      0.20   0    ]
    [ 0      0      0.50   0      0      0.10 ]
```

### 7.3 Mode-Scheduled LQR Gains

The gain matrix is selected at run-time by `lqr_schedule_apply()`:

| Flight mode | ωn [rad/s] | ζ | k_att (roll/pitch) | k_rate (roll/pitch) | k_att (yaw) | k_rate (yaw) |
|-------------|-----------|---|--------------------|--------------------|-----------|----|
| FM_NOMINAL | 10 | 1 | 1.00 | 0.20 | 0.50 | 0.10 |
| FM_DETUMBLE | 30 | 1 | 9.00 | 0.60 | 4.50 | 0.30 |
| *(all others)* | 10 | 1 | *FM_NOMINAL fallback* | | | |

### 7.4 PID Fallback Controller

Used in FM_NOMINAL before EKF convergence, and in FM_DIAGNOSTIC.

**Control law:**

$$u = K_p\,e + K_i \int e\,dt + K_d\,\dot{e}$$

**Default parameters** (from `include/config.h`):

| Constant | Value |
|---------|-------|
| `DEFAULT_KP` | 0.5 |
| `DEFAULT_KI` | 0.01 |
| `DEFAULT_KD` | 0.1 |

**API:**

```c
void lqr_init(lqr_t *lqr);
void lqr_set_gains(lqr_t *lqr, const float K[LQR_M][LQR_N]);
void lqr_compute(lqr_t *lqr, const float att_err[3],
                 const float rate_err[3], float torque_cmd[3]);
void lqr_schedule_apply(lqr_t *lqr, flight_mode_t mode);
void pid_init(pid_ctrl_t *p, float kp, float ki, float kd);
float pid_update(pid_ctrl_t *p, float error, float dt);
```

### 7.5 Torque Saturation

The LQR and PID controllers compute unclamped torque commands. Before applying to
the actuator model, the output is saturated to the physical limit of the magnetorquers:

$$|\tau_{cmd,i}| \leq \tau_{max}$$

Saturation is logged as `FAULT_CTRL_OUTPUT_SATURATED` (0x0201). This is a
non-critical fault: control continues with clipped output.

```c
// Pseudo-code — saturation applied before attitude_dynamics_step()
for (int i = 0; i < 3; i++)
    torque[i] = fmaxf(-TORQUE_MAX, fminf(TORQUE_MAX, torque[i]));
```

The physical `TORQUE_MAX` is derived from the magnetorquer control authority
(see §7.6 below).

### 7.6 Control Authority Budget

Verification that the magnetorquer can produce sufficient torque for control.

**Magnetorquer maximum dipole:**

$$m_{max} \approx 0.2 \ \text{A·m}^2 \quad \text{(DRV8833 driver + 50-turn coil estimate)}$$

**Reference geomagnetic field (LEO 500–700 km SSO):**

$$|\mathbf{B}| \approx 25\text{–}60 \ \mu\text{T}$$

**Maximum available torque per axis:**

$$\tau_{max} = m_{max} \times |\mathbf{B}|_{mean}
             = 0.2 \times 40 \times 10^{-6}
             \approx 8 \times 10^{-6} \ \text{N·m}$$

**Verification against 1U inertia:**

$$\alpha_{max} = \frac{\tau_{max}}{I_{xx}} = \frac{8 \times 10^{-6}}{0.01}
              = 8 \times 10^{-4} \ \text{rad/s}^2$$

For a 30° (0.52 rad) initial attitude error with zero angular rate:

$$t_{settle} \approx \sqrt{\frac{2 \times 0.52}{8 \times 10^{-4}}} \approx 36 \ \text{s}
\quad \text{(upper bound, open-loop)}$$

The LQR closed-loop response (ωn = 10 rad/s, ζ = 1) achieves ≈ 0.4 s settling
for small errors. For large initial errors the torque-saturation bound above
gives the practical limit. **Conclusion: the magnetorquer authority is sufficient
for Phase 1 coarse pointing.**

---

## 8. Detumbling — B-dot Control Law

After deployment, the spacecraft may carry angular momentum from the separation
event. The FM_DETUMBLE mode uses the **B-dot law** to damp body angular rates
via the magnetorquers.

> **B-dot vs B×L — terminology clarification:**
> 
> | Law | Formula | Purpose |
> |-----|---------|--------|
> | **B-dot** (detumble) | **m** = −k · d**B**/dt | Damp body angular rates after deployment |
> | **B×L** (momentum dump) | **m** = −k (B̂ × **L**_rw) | Bleed stored momentum from reaction wheels |
> 
> The current Phase 1 implementation uses `momentum_dump_step()` to produce a
> **B-dot-equivalent** command: with no reaction wheels, `L_rw = Iω ≈ ω` (phase
> 1 approximation), so the cross-product law operates on the body rate directly
> and behaves as a B-dot derivative controller. True B×L momentum dumping
> (Phase 2+) will use the measured reaction-wheel angular momentum vector.

### 8.1 Control Law (Phase 1 — B-dot Approximation)

$$\mathbf{m}_{cmd} = -k_{dump}\,\left(\hat{\mathbf{B}} \times \boldsymbol{\omega}\right)$$

This is equivalent to a B-dot derivative law when **B** rotates with the
spacecraft body (dB/dt ≈ ω × B in body frame).

| Symbol | Description | Units |
|--------|-------------|-------|
| **m**_cmd | magnetic dipole command | A·m² |
| **B̂** | unit vector of local magnetic field | — |
| **L**_rw / **ω** | angular momentum / rate vector | kg·m²/s or rad/s |
| k_dump | scalar control gain | A·m²·s / (kg·m²) |

**Default gain** (`config.h`):

```c
#define DETUMBLE_K_DUMP  0.01f   // [A·m²·s / (kg·m²)]
```

This produces a magnetorquer torque:

$$\boldsymbol{\tau} = \mathbf{m}_{cmd} \times \mathbf{B}$$

which opposes the angular momentum, driving the spacecraft toward zero angular rate.

### 8.2 Phase 1 Notes

In Phase 1 (magnetorquers only, no reaction wheels), `momentum_dump_step()` is
called with `L_rw = snap.state.rates` (body angular rate vector). The direction
of ω and I·ω are identical; only the magnitude scale changes, which is absorbed
into `k_dump`.

The magnetic field input **B** is currently zeroed in the control task as a safe
placeholder pending integration of the magnetometer into the Data Layer (TODO PR-18):

```c
float B[3] = {0.0f, 0.0f, 0.0f}; /* TODO PR-18: read DLA mag_field */
momentum_dump_step(&g_mdump, B, snap.state.rates, dipole);
```

When B = **0** the `momentum_dump_step()` function safely outputs a zero dipole (guard
against degenerate field). No spurious torque is applied.

### 8.3 Convergence Estimate

Expected detumble performance for a typical CubeSat deployment:

| Parameter | Value |
|-----------|-------|
| Initial angular rate | ~10 °/s (typical separation event) |
| Target angular rate | < 0.5 °/s (FMM transition threshold) |
| Geomagnetic field | ~40 µT @ 500 km SSO |
| Magnetorquer dipole | ~0.2 A·m² per axis |
| Expected detumble time | **~1–2 orbits** (~90–180 minutes) |

The convergence time is inversely proportional to `k_dump × |B|²`. Increasing
`DETUMBLE_K_DUMP` above 0.01 will reduce convergence time at the cost of higher
magnetorquer duty cycle.

### 8.4 Transition out of DETUMBLE

FM_DETUMBLE → FM_NOMINAL transition is requested by the FMM once the measured
angular rate falls below the detumble threshold or a ground command is received.

---

## 9. Momentum Management

The `momentum_dump` service is also the mechanism for managing angular momentum
accumulated in the reaction wheels (Phase 2+). The same B×L law applies; the only
difference is that `L_rw` is the true reaction-wheel angular momentum vector once
the RW driver is implemented.

The helper `momentum_dump_needed()` evaluates when a dump is required:

```c
bool momentum_dump_needed(const float L_rw[3], float threshold);
// Returns true when |L_rw| > threshold.
```

---

## 10. Actuators

### 10.1 Phase 1 — Magnetorquers (Active)

| Actuator | Quantity | Driver | Function |
|---------|---------|--------|----------|
| DRV8833 magnetorquer ×3 | 3-axis magnetic dipole | `src/actuators/magnetorquer.c` | detumble + coarse attitude control |

**Commanded magnetic dipole moment:**

$$\mathbf{m} = [m_x,\ m_y,\ m_z]^T \quad [\text{A·m}^2]$$

**Generated torque on the spacecraft:**

$$\boldsymbol{\tau} = \mathbf{m} \times \mathbf{B}$$

where **B** is the local geomagnetic field vector [T]. The ADCS control law
computes **m** and passes it directly to the actuator driver, closing the
estimator → controller → actuator loop:

```c
// Control loop closure: controller → actuator
float dipole[3];                              // [A·m²], computed by control law
magnetorquer_set_moment(&g_mtq, dipole[0], dipole[1], dipole[2]);
```

**API:**

```c
void magnetorquer_init(magnetorquer_t *mq);
void magnetorquer_set_moment(magnetorquer_t *mq, float mx, float my, float mz);
```

Commands are in A·m². The DRV8833 driver translates to PWM duty cycle on GPIO14/15/16.

### 10.2 Phase 2 — Reaction Wheels (Planned)

Three TB6612FNG-driven reaction wheels (GPIO6/7/10, PWM) are included in the
hardware design. The software stub (`src/actuators/reaction_wheel.c`) exists but
is not yet active in the control loop. The LQR and dynamics model already size for
them (I = diag(0.01, 0.01, 0.005) kg·m²).

When reaction wheels are integrated, the LQR torque output will be allocated to
both the reaction wheels (primary) and magnetorquers (momentum management only).

---

## 11. Software Integration

### 11.1 FreeRTOS Tasks

| Task function | File | Rate | FreeRTOS period |
|--------------|------|------|----------------|
| `vSensorReadTask` | `src/tasks/sensor_read_task.c` | **10 Hz** | `pdMS_TO_TICKS(100)` |
| `vAttitudeControlTask` | `src/tasks/attitude_control_task.c` | **10 Hz** | `pdMS_TO_TICKS(100)` |

Both tasks use `vTaskDelayUntil()` for deterministic wakeup.

### 11.2 Data Layer Interface

All inter-task state is exchanged exclusively via the Data Layer (`data_layer.h`).
The sensor task writes to `dl_snapshot_t`; the control task reads via
`data_layer_read(&snap)`. No direct global variable sharing.

Relevant fields on `dl_snapshot_t`:

| Field | Type | Description |
|-------|------|-------------|
| `snap.state.attitude[3]` | float[3] | EKF attitude estimate [rad] |
| `snap.state.rates[3]` | float[3] | Body angular rates [rad/s] |
| `snap.state.imu_valid` | bool | true = IMU data available this step |
| `snap.state.imu_ekf_valid` | bool | true = EKF has converged |
| `snap.state.imu_available` | bool | true = IMU physically present |
| `snap.mode` | flight_mode_t | Current FMM state |

### 11.3 IMU Availability Guard

Before entering the control loop, `vAttitudeControlTask` polls the data layer
every 5 s until `imu_available == true`. This prevents spurious zero-attitude
control commands during sensor initialisation.

---

## 12. Flight Mode Interaction

The ADCS behaviour is fully governed by the FMM state machine:

| Mode | Value | ADCS behaviour |
|------|-------|---------------|
| FM_BOOT | 0 | No ADCS output. Hardware initialising. |
| FM_SAFE | 1 | No ADCS output. Actuators disabled. EKF may run (telemetry only). |
| FM_DETUMBLE | 2 | B-dot detumble via magnetorquers. LQR/PID suppressed. |
| FM_NOMINAL | 3 | Full 3-axis control. LQR (if EKF valid) or PID fallback. |
| FM_DIAGNOSTIC | 4 | PID only. Used for ground-commanded tuning and testing. |

---

## 13. Fault Detection and Response

ADCS faults reported to the Fault Manager (`src/services/fault/fault_manager.c`):

| Fault ID | Hex | Condition | Response |
|---------|-----|-----------|---------|
| FAULT_EST_GYRO_TIMEOUT | 0x0101 | IMU gyro read timeout | Force FM_SAFE |
| FAULT_EST_MAG_TIMEOUT | 0x0102 | Magnetometer read timeout | Degraded: yaw estimation disabled |
| FAULT_EST_DIVERGENCE | 0x0103 | EKF state diverged (see reset conditions below) | Reset estimator; PID fallback |
| FAULT_CTRL_OUTPUT_SATURATED | 0x0201 | Torque output clipped | Log warning; continue |
| FAULT_CTRL_DEADLINE_MISS | 0x0203 | Control loop deadline missed | Log; increment miss counter |
| FAULT_SENS_IMU_I2C_ERROR | 0x0401 | IMU I2C bus error | Force FM_SAFE |
| FAULT_ACT_MTQ_FAULT | 0x0301 | Magnetorquer driver fault | Force FM_SAFE |

### 13.1 EKF Reset Trigger Conditions

The `FAULT_EST_DIVERGENCE` fault (0x0103) is raised and the estimator is reset when
any of the following conditions are detected:

| Condition | Threshold | Action |
|-----------|-----------|--------|
| Attitude state out of bounds | \|attitude[i]\| > π rad | Re-initialize `x` to zero; preserve bias estimate |
| Covariance trace blow-up | tr(**P**) > P_trace_max | Re-initialize **P** to P₀ |
| Gyro read timeout | no new data in > 2 control cycles | Raise FAULT_EST_GYRO_TIMEOUT; enter PID fallback |

After reset, `imu_ekf_valid` is cleared in the Data Layer. The control task
automatically falls back to PID until the EKF re-converges (typically < 5 s for
roll/pitch, < 30 s for yaw depending on magnetic field observability).

---

## 14. SAFE MODE Behaviour

When the system is in FM_SAFE:

- `vAttitudeControlTask_Step()` returns immediately without actuator output
- Magnetorquers commanded to zero moment
- `vSensorReadTask` continues reading sensors (telemetry integrity)
- EKF prediction/update continues (attitude data available for ground monitoring)
- FMM accepts transition to FM_DETUMBLE or FM_NOMINAL upon ground command

---

## 15. Verification

All ADCS components have associated unit tests runnable on the host build:

```bash
cd build && ctest --output-on-failure -R "ekf|lqr|dynamics|momentum|pid"
```

### 15.1 EKF Tests (`tests/unit/test_ekf.c`)

| Test ID | Description | Pass Criterion |
|---------|-------------|---------------|
| T-EKF-01 | Initialization | State zero, P diagonal positive definite |
| T-EKF-02 | Predict-only propagation | State propagates from gyro; bias unchanged |
| T-EKF-03 | Convergence (roll/pitch) | Error < ±2° in 5 s from 10° initial |
| T-EKF-04 | Bias estimation | Roll/pitch bias converges within 10 s |
| T-EKF-05 | Degenerate accelerometer | Update skipped when \|a\| ≈ 0 |
| T-EKF-06 | Covariance symmetry | P stays symmetric after 50 steps |

### 15.2 EKF Magnetometer Tests (`tests/unit/test_ekf_mag.c`)

| Test ID | Description | Pass Criterion |
|---------|-------------|---------------|
| T-EKFM-01 | No crash on cold-start | `ekf_update_mag()` completes on zero-state |
| T-EKFM-02 | Degenerate horizontal field | State and covariance unchanged |
| T-EKFM-03 | Large yaw error | Update moves yaw toward measured value |
| T-EKFM-04 | Innovation wrapping near ±π | Wrapping handled correctly |
| T-EKFM-05 | Covariance reduction | P[2][2] strictly decreases after update |
| T-EKFM-06 | Yaw convergence | Repeated predict+update_mag converges yaw |
| T-EKFM-07 | Declination offset | Non-zero declination shifts yaw measurement |

### 15.3 LQR Tests (`tests/unit/test_lqr.c`)

| Test ID | Description | Pass Criterion |
|---------|-------------|---------------|
| T-LQR-01 | Default gains | K matrix diagonal entries correct |
| T-LQR-02 | Zero input | Zero error → zero torque |
| T-LQR-03 | Sign convention | Positive error → negative (restoring) torque |
| T-LQR-04 | Axis isolation | Single-axis error affects only its output |
| T-LQR-05 | Closed-loop stability | 30° initial error settles to < 1° in 30 s |
| T-LQR-06 | Disturbance rejection | Steady-state error < 1° under 1×10⁻⁴ N·m |
| T-LQR-07 | Custom gains | `lqr_set_gains()` correctly replaces K |

### 15.4 Dynamics Tests (`tests/unit/test_dynamics.c`)

Validates the RK2 integrator and rigid-body propagation accuracy.

### 15.5 Momentum Dump Tests (`tests/unit/test_momentum_dump.c`)

| Test ID | Description | Pass Criterion |
|---------|-------------|---------------|
| T-MTM-01 | Initialization | Gain stored correctly |
| T-MTM-02 | Zero B field guard | Degenerate field → zero dipole |
| T-MTM-03 | Known B and L_rw | Dipole matches analytic cross-product |
| T-MTM-04 | Threshold check | `momentum_dump_needed()` correct above/below |
| T-MTM-05 | Gain scaling | Doubling k_dump doubles dipole magnitude |

---

## 16. Attitude Dynamics Model

The rigid-body attitude dynamics model (`src/dynamics/attitude_dynamics.c`) is used
in closed-loop simulation and hardware-in-loop testing.

**State:** x = [attitude[3], rates[3]] — Euler angles [rad] and body rates [rad/s]

**Equations of motion** (linearized, principal-axis assumption):

$$\dot{\theta}_i = \omega_i$$
$$\dot{\omega}_i = \frac{\tau_i}{I_i}$$

**Inertia tensor** (1U CubeSat estimate):

$$I = \text{diag}(0.01,\ 0.01,\ 0.005) \quad [\text{kg}\cdot\text{m}^2]$$

The symmetric inertia (Ixx = Iyy ≫ Izz) reflects the typical 1U form factor
where the Z axis (along the stacking axis) has lower mass distribution.

**Integration:** RK2 midpoint (see §6.3).

---

## 17. Future Improvements

| Item | Description | Target Phase |
|------|-------------|-------------|
| PR-18 | Wire magnetometer mag_field into Data Layer for B×L detumble | Phase 5 |
| Quaternion EKF | Migrate state to q = [w, x, y, z] using existing `quaternion.c` | Phase 7 |
| Reaction wheel support | Activate `reaction_wheel.c`; update momentum dump L_rw | Phase 2 |
| Sun sensor | Add coarse sun-vector measurement as EKF update | Phase 3+ |
| IGRF model | On-board magnetic field model for declination correction in orbit | Phase 5+ |
| MEKF | Multiplicative EKF for quaternion-safe covariance propagation | Phase 7+ |

---

*Document generated from FSW source audit — `config/pico_pins.h`, `include/ekf.h`,
`include/lqr.h`, `include/lqr_schedule.h`, `include/momentum_dump.h`, `include/config.h`,
`include/flight_mode.h`, `include/fault_ids.h`, `src/tasks/attitude_control_task.c`,
`src/tasks/sensor_read_task.c`, `src/dynamics/attitude_dynamics.c`.*
