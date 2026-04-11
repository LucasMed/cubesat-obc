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
#include "bh1750.h"
#include "config.h"
#include "data_layer.h"
#include "ds3231.h"
#include "drivers/imu/mpu6050.h"
#include "drivers/mag/hmc5883l.h"
#include "drivers/temperature.h"
#include "ekf.h"
#include "sht31.h"
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

/** Time step matching the 10 Hz task rate [s]. */
#define SENSOR_DT_S 0.1f

/* EKF instance — initialised once on first step. */
static ekf_t s_ekf;
static bool s_ekf_initialised = false;

/* Magnetometer init flag. */
static bool s_mag_initialised = false;

// Core logic for sensor reading (independent of FreeRTOS task loop)
void vSensorReadTask_Step(void)
{
  dl_snapshot_t snap;
  data_layer_read(&snap);

  /* Read MPU6050 only if it was detected during boot */
  if (snap.state.imu_available)
  {
    float accel[3];
    float gyro_deg[3];
    if (mpu6050_read_raw(accel, gyro_deg) == 0)
    {
      /* Convert gyroscope output from deg/s to rad/s (SPEC-2-DLA §2.4) */
      float gyro_rad[3] = {gyro_deg[0] * DEG_TO_RAD, gyro_deg[1] * DEG_TO_RAD,
                           gyro_deg[2] * DEG_TO_RAD};

      /* Write raw gyro rates so downstream tasks always have current rates. */
      data_layer_write_imu(snap.state.attitude, gyro_rad);

      /* --- EKF sensor fusion ------------------------------------------ */
      if (!s_ekf_initialised)
      {
        ekf_init(&s_ekf);
        s_ekf_initialised = true;
      }

      ekf_predict(&s_ekf, gyro_rad, SENSOR_DT_S);
      ekf_update(&s_ekf, accel);

      /* Extract EKF outputs and publish to DLA. */
      float ekf_q[4] = {0.0f, 0.0f, 0.0f, 0.0f};
      float ekf_bias[3] = {0.0f, 0.0f, 0.0f};
      float ekf_cov[7] = {0.0f};

      ekf_get_quaternion(&s_ekf, ekf_q);
      ekf_get_bias(&s_ekf, ekf_bias);

      /* Diagonal covariance elements (q0..q3, bx..bz). */
      for (int i = 0; i < 7; i++)
      {
        ekf_cov[i] = s_ekf.P[i][i];
      }
      data_layer_write_ekf(ekf_q, ekf_bias, ekf_cov);
    }
  }

  /* Read temperature only if sensor was detected */
  if (snap.state.temp_available)
  {
    float temp = temperature_read();
    data_layer_write_temp(temp);

    /* Read SHT31 temperature and humidity for higher accuracy */
    float sht31_temp = 0.0f;
    float sht31_humidity = 0.0f;
    if (sht31_read(&sht31_temp, &sht31_humidity))
    {
      /* SHT31 is more accurate, use it if available */
      data_layer_write_temp(sht31_temp);

      /* Write humidity to data layer (valid if >= 0) */
      data_layer_write_humidity(sht31_humidity);

#ifdef PICO_BUILD
      /* Debug: print SHT31 readings */
      if (sht31_humidity >= 0.0f)
      {
        printf("[sht31] temp=%.1fC  humidity=%.1f%%\n", (double)sht31_temp, (double)sht31_humidity);
      }
      else
      {
        printf("[sht31] temp=%.1fC  humidity=N/A\n", (double)sht31_temp);
      }
#endif
    }
  }

  /* Read BH1750 light sensor */
  if (snap.state.lux_available)
  {
    float lux = -1.0f;
    if (bh1750_read(&lux))
    {
      data_layer_write_lux(lux);
    }
  }

  /* Read DS3231 RTC - once per second (every 10 cycles at 10 Hz) */
  if (snap.state.rtc_available)
  {
    static uint8_t rtc_read_counter = 0;
    if (++rtc_read_counter >= 10)  /* 10 Hz task → 1 Hz RTC read */
    {
      rtc_read_counter = 0;
      uint16_t year;
      uint8_t month, day, hour, minute, second;
      if (ds3231_read_time(&year, &month, &day, &hour, &minute, &second))
      {
        uint32_t ts = ds3231_to_epoch(year, month, day, hour, minute, second);
        data_layer_write_rtc(ts);
      }
    }
  }

  /* Read magnetometer only if sensor was detected during boot */
  if (snap.state.mag_available)
  {
    if (!s_mag_initialised)
    {
      if (hmc5883l_init() == 0)
      {
        s_mag_initialised = true;
      }
    }
    if (s_mag_initialised)
    {
      float mag_uT[3];
      if (hmc5883l_read(mag_uT) == 0)
      {
        data_layer_write_mag(mag_uT);

        /* Yaw correction: fuse mag into EKF when filter is ready. */
        if (s_ekf_initialised)
        {
          ekf_update_mag(&s_ekf, mag_uT, OBC_MAG_DECLINATION_RAD);
        }
      }
    }
  }
}

// Sensor read task: reads IMU at 10 Hz
void vSensorReadTask(void *pvParameters)
{
  (void)pvParameters;
  /* cppcheck-suppress unreadVariable -- updated each cycle by vTaskDelayUntil */
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
  fflush(stdout);

  /* ── Check if any sensor is available; if not, exit this task ────────── */
  {
    dl_snapshot_t snap;
    data_layer_read(&snap);
    if (!snap.state.imu_available && !snap.state.temp_available && !snap.state.mag_available &&
        !snap.state.lux_available && !snap.state.rtc_available)
    {
      printf("[sensor_read_task] No sensors connected — task suspended\n");
      fflush(stdout);
      vTaskSuspend(NULL); /* park forever — no CPU wasted */
      /* unreachable unless explicitly resumed */
    }
    printf("[sensor_read_task] Sensors: IMU=%s  Temp=%s  Mag=%s  Light=%s  RTC=%s\n",
           snap.state.imu_available ? "yes" : "no", snap.state.temp_available ? "yes" : "no",
           snap.state.mag_available ? "yes" : "no", snap.state.lux_available ? "yes" : "no",
           snap.state.rtc_available ? "yes" : "no");
    fflush(stdout);
  }

  while (1)
  {
    /* cppcheck-suppress unreadVariable -- macro writes back updated wake time */
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
      printf("[sensor_read_task] Timing (100 samples): min=%llu, max=%llu, avg=%llu us\n",
             (unsigned long long)min_int, (unsigned long long)max_int,
             (unsigned long long)(sum_int / 100));
      min_int = 0xFFFFFFFFFFFFFFFF;
      max_int = 0;
      sum_int = 0;
      samples = 1;  // Keep last_wake for next measurement
    }
#endif

    vSensorReadTask_Step();
  }
}
