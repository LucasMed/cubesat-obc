/**
 * @file sensor_read_task.c
 * @brief Sensor read task — migrated to Data Layer Abstraction (PR-7).
 *
 * All shared state is now accessed exclusively through data_layer.h.
 * The legacy system_state.h API is no longer used here.
 *
 * Unit conversions applied on every tick:
 *   - Gyroscope output (deg/s) → rad/s  (multiply by π/180)
 *   - Accelerometer output (G) stored as-is in att_rad[]; a dedicated
 *     attitude-estimator task will replace this in a later PR.
 *
 * Spec ref: SPEC-2-DLA v1.6 §2.4
 */

#include "sensor_read_task.h"

#include "FreeRTOS.h"
#include "config.h"
#include "data_layer.h"
#include "drivers/imu/mpu6050.h"
#include "drivers/temperature.h"
#include "task.h"

#include <math.h>
#include <stdio.h>
#ifdef PICO_BUILD
  #include "pico/time.h"
#endif

#ifndef M_PI
  #define M_PI 3.14159265358979323846f
#endif

#define DEG_TO_RAD (float)(M_PI / 180.0)

// Core logic for sensor reading (independent of FreeRTOS task loop)
void vSensorReadTask_Step(void)
{
  dl_snapshot_t snap;
  data_layer_read(&snap);

  /* Read MPU6050 only if it was detected during boot */
  if (snap.state.imu_available)
  {
    float accel[3], gyro_deg[3];
    if (mpu6050_read_raw(accel, gyro_deg) == 0)
    {
      /* Convert gyroscope output from deg/s to rad/s (SPEC-2-DLA §2.4) */
      float gyro_rad[3] = {gyro_deg[0] * DEG_TO_RAD, gyro_deg[1] * DEG_TO_RAD,
                           gyro_deg[2] * DEG_TO_RAD};
      data_layer_write_imu(accel, gyro_rad);
    }
  }

  /* Read temperature only if sensor was detected */
  if (snap.state.temp_available)
  {
    float temp = temperature_read();
    data_layer_write_temp(temp);
  }
}

// Sensor read task: reads IMU at 10 Hz
void vSensorReadTask(void *pvParameters)
{
  (void)pvParameters;
  TickType_t xLastWakeTime = xTaskGetTickCount();
  const TickType_t xFrequency = pdMS_TO_TICKS(100);  // 10 Hz

#ifdef PICO_BUILD
  uint64_t last_wake = time_us_64();
  uint64_t min_int = 0xFFFFFFFFFFFFFFFF;
  uint64_t max_int = 0;
  uint64_t sum_int = 0;
  uint32_t samples = 0;
#endif

  printf("[sensor_read_task] Started\n");

  while (1)
  {
    vTaskDelayUntil(&xLastWakeTime, xFrequency);

#ifdef PICO_BUILD
    uint64_t now = time_us_64();
    uint64_t interval = now - last_wake;
    last_wake = now;

    if (samples > 0)
    {  // Skip first sample to stabilize
      if (interval < min_int)
        min_int = interval;
      if (interval > max_int)
        max_int = interval;
      sum_int += interval;
    }
    samples++;

    if (samples >= 101)
    {  // 100 measured intervals
      printf("[sensor_read_task] Timing (100 samples): min=%llu, max=%llu, avg=%llu us\n", min_int,
             max_int, sum_int / 100);
      min_int = 0xFFFFFFFFFFFFFFFF;
      max_int = 0;
      sum_int = 0;
      samples = 1;  // Keep last_wake for next measurement
    }
#endif

    vSensorReadTask_Step();
  }
}
