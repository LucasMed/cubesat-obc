# Coding Standards

**Document ID**: STD-001  
**Version**: 1.0  
**Last Updated**: 2026-02-20  
**Status**: Active  
**Based on**: MISRA C:2012 (subset), ECSS-Q-ST-80C

---

## 1. Naming Conventions

### 1.1 Functions
```c
// Pattern: module_action() or module_noun_verb()
pid_init();
pid_update();
attitude_control_update();
rw_set_torque();
mpu6050_read();
```

### 1.2 Types
```c
// Pattern: module_name_t (typedef structs)
typedef struct { ... } pid_controller_t;
typedef struct { ... } attitude_state_t;
typedef struct { ... } system_state_t;
```

### 1.3 Constants & Macros
```c
// Pattern: CONFIG_NAME or MODULE_NAME
#define CONFIG_FREERTOS_HEAP_SIZE  (32 * 1024)
#define PID_MAX_INTEGRAL          100.0f
#define SENSOR_READ_RATE_HZ       10
```

### 1.4 Variables
```c
// Local: snake_case
float angular_rate;
int loop_counter;

// Global (avoid): g_prefix
static system_state_t g_system_state;
```

---

## 2. Code Safety Rules

### 2.1 Memory Management
- ❌ **No `malloc()`/`free()` in flight code** — static allocation only
- ✅ Use stack-allocated locals or module-scope statics
- ✅ FreeRTOS heap via `pvPortMalloc()` for task stacks only (configured in `FreeRTOSConfig.h`)

### 2.2 Type Safety
- Use `stdint.h` types (`uint8_t`, `int32_t`, etc.) for hardware registers
- Use `float` for control math (RP2350 has FPU)
- Explicit casts when converting between integer and float

### 2.3 Error Handling
```c
// Return code convention:
//  0    = success
//  > 0  = warning (non-critical, e.g., actuator saturated)
//  < 0  = error (critical, operation failed)

int status = mpu6050_read(&data);
if (status < 0) {
    // Handle error: log, enter safe mode
}
```

### 2.4 Bounds Checking
- Check array indices before access
- Validate pointer arguments (non-NULL) at function entry
- Clamp output values to physical limits

---

## 3. Compilation Standards

### 3.1 Required Flags
```cmake
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
add_compile_options(-Wall -Wextra -pedantic)
```

### 3.2 Release Builds
```cmake
# Treat warnings as errors for releases
add_compile_options(-Werror)
```

### 3.3 Static Analysis
```bash
# Run cppcheck before merge
cppcheck --enable=all --suppress=missingInclude src/ include/
```

---

## 4. File Organization

### 4.1 Header Files
```c
// Every .h file MUST have include guards:
#ifndef MODULE_NAME_H
#define MODULE_NAME_H

#include <stdint.h>  // Standard types first

// Public API declarations only
void module_function(void);

#endif // MODULE_NAME_H
```

### 4.2 Source Files
```c
// Order of includes:
#include "module_name.h"     // Own header first
#include <string.h>          // Standard library
#include "other_module.h"    // Project headers
#include "FreeRTOS.h"        // RTOS headers last

// Static (private) functions declared at top
static void helper_function(void);

// Public functions below
void module_function(void) { ... }
```

### 4.3 Documentation
```c
/**
 * @brief Compute PID output for current error
 * @param pid     Pointer to initialized PID controller
 * @param error   Current error (setpoint - measurement) [rad/s]
 * @param dt      Time step since last call [s]
 * @return        Control output, clipped to saturation limits [N·m]
 * @pre           pid initialized via pid_init(), dt > 0
 * @post          Internal states updated
 */
float pid_update(pid_controller_t *pid, float error, float dt);
```

---

## 5. FreeRTOS Conventions

### 5.1 Task Functions
```c
// Task entry point: void task_name(void *pvParameters)
// Must contain infinite loop: for(;;) { ... vTaskDelay(...); }
// Never return from a task function
```

### 5.2 Priority Levels
| Level | Use |
|-------|-----|
| 4 | Critical/safety (reserved) |
| 3 | HIGH — Sensor, Control |
| 2 | MEDIUM — Telemetry |
| 1 | LOW — Health monitoring |
| 0 | IDLE (FreeRTOS internal) |

### 5.3 Shared Data
- Access `system_state_t` only through accessor functions
- Accessor functions use FreeRTOS critical sections or mutexes
- Keep critical sections <1 ms

---

## 6. Git Commit Messages

```
<type>(<scope>): <description>

Types: feat, fix, docs, chore, test, refactor
Scope: control, drivers, tasks, build, docs

Examples:
  feat(drivers): implement MPU6050 I2C driver
  fix(control): correct PID anti-windup saturation
  docs(standards): update coding conventions
  test(actuators): add magnetorquer boundary tests
```

---

## 7. Testing Requirements

- Every public API function must have at least one unit test
- Tests must pass with `ctest --output-on-failure`
- No test shall depend on external hardware (use stubs/mocks)
- Integration tests tagged separately from unit tests
