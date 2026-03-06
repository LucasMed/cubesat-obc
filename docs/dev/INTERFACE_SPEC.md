# Interface Specification

## Overview

This document specifies the public API contracts for all OBC subsystems. Each interface defines input/output types, preconditions, postconditions, and error handling.

---

## 1. PID Controller Interface

**File**: `include/pid.h`, `src/control/pid_controller.c`

### Purpose
Proportional-Integral-Derivative controller for single-axis rate stabilization.

### Type Definition
```c
typedef struct {
    float kp;              // Proportional gain
    float ki;              // Integral gain
    float kd;              // Derivative gain
    float sat_limit;       // Output saturation (torque limit, N·m)
    float integral_max;    // Anti-windup integral limit
    float integral_term;   // Internal state: accumulated error
    float error_prev;      // Internal state: previous error
} pid_controller_t;
```

### Functions

#### `pid_init()`
```c
void pid_init(pid_controller_t *pid, float kp, float ki, float kd, float sat_limit);
```
- **Purpose**: Initialize PID instance with gains
- **Pre-conditions**: 
  - `pid` is non-NULL and points to valid memory
  - `kp, ki, kd >= 0` (gains are non-negative)
  - `sat_limit > 0` (saturation limit positive)
- **Post-conditions**:
  - PID structure populated; internal states reset to 0
  - Ready for first `pid_update()` call
- **Return**: None
- **Side Effects**: Initializes integral and error history

#### `pid_update()`
```c
float pid_update(pid_controller_t *pid, float error, float dt);
```
- **Purpose**: Compute control output for given error and time step
- **Parameters**:
  - `error`: Current error (setpoint - measurement) [rad/s for rate control]
  - `dt`: Time step since last call [seconds]
- **Pre-conditions**:
  - `pid` initialized via `pid_init()`
  - `dt > 0` (positive time step)
  - error is finite (not NaN/inf)
- **Post-conditions**:
  - Internal integral and error states updated
  - Output clipped to `[-sat_limit, +sat_limit]` range
- **Return**: Control output [N·m for torque]
- **Side Effects**: Updates `pid->integral_term` and `pid->error_prev`

#### `pid_reset()`
```c
void pid_reset(pid_controller_t *pid);
```
- **Purpose**: Clear integral accumulator and error history
- **Use Case**: Mode transition (e.g., idle → active control)
- **Post-conditions**: Integral state and error history zeroed

---

## 2. Attitude Control Interface

**File**: `include/attitude_control.h`, `src/control/attitude_control.c`

### Purpose
Higher-level attitude controller mapping attitude error to rate commands fed to PID loops.

### Type Definition
```c
typedef struct {
    float roll_cmd, pitch_cmd, yaw_cmd;      // Desired attitude [rad] (Euler)
    pid_controller_t pid_roll;
    pid_controller_t pid_pitch;
    pid_controller_t pid_yaw;
    float max_rate_cmd;                      // Rate limit [rad/s]
} attitude_controller_t;
```

### Functions

####`attitude_control_init()`
```c
void attitude_control_init(attitude_controller_t *ctrl, float kp, float ki, float kd, float sat_limit);
```
- **Purpose**: Initialize attitude controller with default gains
- **Pre-conditions**: `ctrl` non-NULL
- **Post-conditions**: 3 internal PID loops initialized with provided gains
- **Return**: None

#### `attitude_control_update()`
```c
void attitude_control_update(attitude_controller_t *ctrl,
                              float roll_current, float pitch_current, float yaw_current,
                              float dt,
                              float *torque_roll, float *torque_pitch, float *torque_yaw);
```
- **Purpose**: Compute torque commands from attitude error
- **Parameters**:
  - `roll/pitch/yaw_current`: Current Euler angles [rad]
  - `dt`: Time step [s]
  - `torque_*`: Output torque pointers (filled by function)
- **Pre-conditions**:
  - `ctrl` initialized; all output pointers non-NULL
  - Current angles are finite
  - Desired commands (`ctrl->roll_cmd` etc.) set by caller
- **Post-conditions**:
  - Output torques computed, clipped to limits
  - Rate commands respect `max_rate_cmd`
- **Return**: None via pointers
- **Algorithm**: 
  1. Compute error: `error = cmd - current`
  2. Normalize to [-π, π]
  3. Compute rate_cmd: `rate_cmd = kp_att * error` (simplified proportional mapping)
  4. Feed to PID: `torque = pid_update(rate_cmd - meas_rate, dt)`

---

## 3. Attitude Dynamics Interface

**File**: `include/attitude_dynamics.h`, `src/dynamics/attitude_dynamics.c`

### Purpose
Integrate rigid-body attitude dynamics given applied torques.

### Type Definition
```c
typedef struct {
    float euler[3];          // Roll, pitch, yaw [rad]
    float angular_rates[3];  // p, q, r [rad/s]
    float inertia[3];        // Ixx, Iyy, Izz [kg·m²]
} attitude_state_t;
```

### Functions

#### `attitude_dynamics_init()`
```c
void attitude_dynamics_init(attitude_state_t *state, float ixx, float iyy, float izz);
```
- **Purpose**: Initialize attitude state with inertia tensor
- **Pre-conditions**: Inertia values positive
- **Post-conditions**: State reset to origin (zero attitude, rates, angles)

#### `attitude_dynamics_step()`
```c
void attitude_dynamics_step(attitude_state_t *state,
                             float torque_roll, float torque_pitch, float torque_yaw,
                             float dt);
```
- **Purpose**: Integrate attitude forward by `dt` under applied torques
- **Parameters**:
  - `torque_*`: Applied torques [N·m]
  - `dt`: Integration time step [s]
- **Pre-conditions**:
  - `state` initialized; torques finite; `dt > 0`
  - No singularities (Euler angles not at gimbal lock, though code handles it)
- **Post-conditions**:
  - `state->euler` and `state->angular_rates` updated
  - Uses Euler forward integration: `x(t+dt) = x(t) + dx/dt * dt`
- **Return**: None
- **Notes**:
  - Simple Euler method (O(dt) accuracy); upgrade to RK2/RK4 in Phase 4 if needed
  - Assumed small angles for simplicity; full quaternion can replace if needed

---

## 4. Sensor Driver Interfaces

### 4.1 IMU Driver (MPU6050)

**File**: `include/imu.h`, `src/drivers/imu/mpu6050.c`

#### Type Definition
```c
typedef struct {
    float accel[3];        // Acceleration [m/s²] (x, y, z)
    float gyro[3];         // Angular velocity [rad/s] (p, q, r)
    float temp;            // Temperature [°C]
} imu_data_t;
```

#### `mpu6050_init()`
```c
int mpu6050_init(uint8_t i2c_addr, uint32_t i2c_speed);
```
- **Purpose**: Initialize MPU6050 over I2C
- **Parameters**:
  - `i2c_addr`: I2C 7-bit address (default 0x68)
  - `i2c_speed`: I2C bus speed [Hz] (typically 400kHz)
- **Return**: 0 on success, -1 on error
- **Pre-conditions**: I2C master initialized at SDA/SCL pins
- **Post-conditions**: Sensor configured; ready for `mpu6050_read()`

#### `mpu6050_read()`
```c
int mpu6050_read(imu_data_t *data);
```
- **Purpose**: Read sensor data into structure
- **Parameters**: `data` — pointer to output structure
- **Return**: 0 on success, -1 on I2C error
- **Pre-conditions**: `mpu6050_init()` called; `data` non-NULL
- **Post-conditions**: `data` populated with latest accel, gyro, temp readings
- **Timing**: Typically 5–10 ms (I2C transfer + conversions)

---

### 4.2 Temperature Sensor

**File**: `include/temperature.h`, `src/drivers/temperature.c` (stub)

#### `temperature_init()`
```c
int temperature_init(void);
```
- **Purpose**: Initialize temperature sensor (TMP102 or onboard RP2040)
- **Return**: 0 on success

#### `temperature_read()`
```c
float temperature_read(void);
```
- **Purpose**: Read current temperature
- **Return**: Temperature [°C]

---

## 5. Actuator Interfaces

### 5.1 Reaction Wheel

**File**: `include/reaction_wheel.h`, `src/actuators/reaction_wheel.c`

#### Type Definition
```c
typedef struct {
    float momentum[3];     // Angular momentum [N·m·s] for each axis
    float max_torque;      // Max applicable torque [N·m]
    float max_momentum;    // Momentum saturation limit [N·m·s]
} reaction_wheel_t;
```

#### `rw_init()`
```c
void rw_init(reaction_wheel_t *rw, float max_torque, float max_momentum);
```
- **Purpose**: Initialize reaction wheel model
- **Pre-conditions**: Limits positive
- **Post-conditions**: Momentum reset to zero; ready for commands

#### `rw_set_torque()`
```c
int rw_set_torque(reaction_wheel_t *rw, float torque[3], float dt);
```
- **Purpose**: Apply torque command and update momentum
- **Parameters**:
  - `torque[3]`: Torque per axis [N·m]; clipped to `max_torque`
  - `dt`: Time step [s]; momentum updates as `dh = torque * dt`
- **Return**: 0 on success, 1 if momentum saturated, -1 on error
- **Post-conditions**:
  - `rw->momentum` updated
  - Saturated momentum clamped to `[-max_momentum, +max_momentum]`
- **Notes**: Return value 1 signals to ground station: de-sat maneuver needed

#### `rw_get_momentum()`
```c
void rw_get_momentum(reaction_wheel_t *rw, float momentum[3]);
```
- **Purpose**: Query current angular momentum
- **Return**: Current momentum per axis

---

### 5.2 Magnetorquer

**File**: `include/magnetorquer.h`, `src/actuators/magnetorquer.c`

#### Type Definition
```c
typedef struct {
    float dipole_moment[3]; // Magnetic dipole [A·m²] per axis
    float max_dipole;       // Max dipole strength [A·m²]
} magnetorquer_t;
```

#### `mag_init()`
```c
void mag_init(magnetorquer_t *mag, float max_dipole);
```
- **Purpose**: Initialize magnetorquer
- **Pre-conditions**: `max_dipole > 0`
- **Post-conditions**: Dipole moment zeroed

#### `mag_set_dipole()`
```c
int mag_set_dipole(magnetorquer_t *mag, float dipole[3]);
```
- **Purpose**: Set magnetic dipole moment commands
- **Parameters**: `dipole[3]` — desired dipole per axis [A·m²]
- **Return**: 0 on success, -1 on over-range
- **Post-conditions**:
  - Commands clipped to `[-max_dipole, +max_dipole]`
  - Stored for next actuator pulse

---

## 6. System State Interface

**File**: `include/system_state.h`, `src/core/system_state.c`

### Purpose
Centralized shared state accessed by FreeRTOS tasks with mutex protection.

### Type Definition
```c
typedef struct {
    // Attitude & rates
    float euler[3];              // Roll, pitch, yaw [rad]
    float angular_rates[3];      // p, q, r [rad/s]
    
    // Sensor data
    imu_data_t imu;              // Latest IMU reading
    float battery_voltage;       // [V]
    float temperature;           // [°C]
    
    // Control commands
    float desired_euler[3];      // Commanded attitude [rad]
    
    // Actuator states
    reaction_wheel_t rw;
    magnetorquer_t mag;
    float control_torque[3];     // Latest torque commands [N·m]
    
    // Diagnostics
    uint32_t loop_counter;       // Total control loop iterations
    uint8_t flags;               // Status flags (RW saturated, thermal warning, etc.)
} system_state_t;
```

### Functions

#### `get_system_state()`
```c
const system_state_t *get_system_state(void);
```
- **Purpose**: Get current system state pointer (read-only)
- **Return**: Pointer to global state structure
- **Thread-Safety**: Uses internal critical section; safe for concurrent reads

#### `set_attitude()`
```c
void set_attitude(float roll, float pitch, float yaw);
```
- **Purpose**: Update attitude estimate
- **Pre-conditions**: Angles are finite and normalized to [-π, π]
- **Post-conditions**: `state.euler` updated under mutex

#### `set_imu_data()`
```c
void set_imu_data(const imu_data_t *data);
```
- **Purpose**: Update IMU readings (called by sensor task)
- **Thread-Safety**: Mutex protected

#### `get_desired_attitude()`
```c
void get_desired_attitude(float *roll_cmd, float *pitch_cmd, float *yaw_cmd);
```
- **Purpose**: Retrieve commanded attitude
- **Thread-Safety**: Mutex protected

---

## 7. FreeRTOS Task Interfaces

**File**: `include/task.h`, `src/tasks/*.c`

### Sensor Read Task
```c
void sensor_read_task(void *pvParameters);
```
- **Rate**: 10 Hz
- **Priority**: HIGH (3)
- **Input**: None (reads hardware I2C)
- **Output**: Updates `system_state.imu`, `system_state.temperature`, `system_state.battery_voltage`
- **Timing Budget**: <50 ms per cycle

### Attitude Control Task
```c
void attitude_control_task(void *pvParameters);
```
- **Rate**: 20 Hz
- **Priority**: HIGH (3)
- **Input**: Reads `system_state.euler`, `system_state.angular_rates`, `system_state.desired_euler`
- **Output**: Updates `system_state.control_torque`, actuates RW + magnetorquer
- **Timing Budget**: <25 ms per cycle

### Telemetry Task
```c
void telemetry_task(void *pvParameters);
```
- **Rate**: 1 Hz
- **Priority**: MEDIUM (2)
- **Input**: Reads entire `system_state`
- **Output**: Packets transmitted over WiFi/UART
- **Timing Budget**: <500 ms per cycle (lenient, low priority)

### Health Monitor Task
```c
void health_monitor_task(void *pvParameters);
```
- **Rate**: 0.2 Hz (every 5 seconds)
- **Priority**: LOW (1)
- **Input**: Reads `system_state` flags, voltages, temperatures
- **Output**: Updates `system_state.flags`; may trigger watchdog reset
- **Timing Budget**: <1000 ms per cycle

---

## 8. Error Handling Convention

All functions that can fail follow this convention:

- **Return Codes**:
  - `0` or `NULL`: Success (or success with no special return)
  - `> 0`: Non-critical warning (e.g., RW momentum saturated → return 1)
  - `< 0` or `NULL`: Critical error; operation failed

- **Example**:
  ```c
  int status = rw_set_torque(&rw, torque, dt);
  if (status < 0) {
      // Handle critical error: log and possibly trigger safe mode
      trigger_safe_mode();
  } else if (status == 1) {
      // Non-critical: momentum saturating → request de-sat maneuver
      request_desaturation();
  }
  ```

---

## 9. Thread Safety & Concurrency

- **Shared Data**: `system_state_t` accessed by 4 concurrent tasks
- **Protection**: Mutex-protected accessors (`get_system_state()`, `set_attitude()`, etc.)
- **Atomic Reads**: Individual fields (e.g., `state.euler[0]`) assumed atomic on 32-bit reads
- **No Blocking**: All I2C driver calls assume non-blocking (DMA or polling without task yield)
- **Critical Sections**: Minimized to <1 ms to avoid task preemption

---

## 10. Integration Checklist

Before merging feature branches:

- [ ] All functions documented with pre/post-conditions
- [ ] Return codes tested (success, warning, error paths)
- [ ] Thread-safety verified (no race conditions)
- [ ] Memory bounds checked (no buffer overflows)
- [ ] Unit tests written for each public API
- [ ] Example usage provided in docstring

---

**Last Updated**: 2026-02-15  
**Interface Version**: 1.0  
**Status**: Ready for Phase 2 implementation review
