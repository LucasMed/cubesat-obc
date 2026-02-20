#include "config.h"
#include "sensor_read_task.h"
#include <stdio.h>
#include "FreeRTOS.h"
#include "task.h"

#include "drivers/imu/mpu6050.h"
#include "drivers/temperature.h"
#include "system_state.h"
#ifdef PICO_BUILD
#include "pico/time.h"
#endif

// Core logic for sensor reading (independent of FreeRTOS task loop)
void vSensorReadTask_Step(void) {
    system_state_t state;
    system_state_get(&state);

    // Read MPU6050 only if it was detected during boot
    if (state.imu_available) {
        float accel[3], gyro[3];
        if (mpu6050_read_raw(accel, gyro) == 0) {
            system_state_set_imu(accel, gyro);
        }
    }

    // Read Temperature only if detected
    if (state.temp_available) {
        float temp = temperature_read();
        system_state_set_temp(temp);
    }
}

// Sensor read task: reads IMU at 10 Hz
void vSensorReadTask(void *pvParameters) {
    (void)pvParameters;
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(100); // 10 Hz

#ifdef PICO_BUILD
    uint64_t last_wake = time_us_64();
    uint64_t min_int = 0xFFFFFFFFFFFFFFFF;
    uint64_t max_int = 0;
    uint64_t sum_int = 0;
    uint32_t samples = 0;
#endif

    printf("[sensor_read_task] Started\n");

    while (1) {
        vTaskDelayUntil(&xLastWakeTime, xFrequency);

#ifdef PICO_BUILD
        uint64_t now = time_us_64();
        uint64_t interval = now - last_wake;
        last_wake = now;

        if (samples > 0) { // Skip first sample to stabilize
            if (interval < min_int) min_int = interval;
            if (interval > max_int) max_int = interval;
            sum_int += interval;
        }
        samples++;

        if (samples >= 101) { // 100 measured intervals
            printf("[sensor_read_task] Timing (100 samples): min=%llu, max=%llu, avg=%llu us\n",
                   min_int, max_int, sum_int / 100);
            min_int = 0xFFFFFFFFFFFFFFFF;
            max_int = 0;
            sum_int = 0;
            samples = 1; // Keep last_wake for next measurement
        }
#endif

        vSensorReadTask_Step();
    }
}
