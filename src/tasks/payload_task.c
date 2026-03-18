/**
 * @file payload_task.c
 * @brief Payload FreeRTOS task implementation.
 */

#include "payload_task.h"

#include "camera_driver.h"
#include "data_layer.h"
#include "flight_mode.h"
#include "payload_manager.h"
#include "radiation_driver.h"
#include "rm3100.h"
#include "storage_manager.h"

#include <stdio.h>

#define PAYLOAD_PERIOD_MS 100
void payload_task_init(void)
{
#define RADIATION_PERIOD_DIV 10 /* 1Hz (10 * 100ms) */
  payload_manager_init();
  xTaskCreate(vPayloadTask, "PayloadTask", 1024, NULL, 2, NULL);
}

static uint32_t s_counter = 0;
static TickType_t s_last_wake_time = 0;

void payload_task_reset(void)
{
  s_counter = 0;
  s_last_wake_time = 0;
}

void vPayloadTask_Step(void)
{
  uint32_t notify_value = 0;

  /* Wait for notification or timeout (periodic sampling) */
  if (xTaskNotifyWait(0x00, 0xFFFFFFFF, &notify_value, 0) == pdTRUE)
  {
    if (notify_value & PAYLOAD_NOTIFY_CAPTURE_IMAGE)
    {
      printf("[PayloadTask] Capture command received\n");
      if (camera_init())
      {
        if (camera_capture(5000))
        {
          uint32_t len = camera_get_fifo_length();
          if (len > 0 && len < 200000) /* Safety limit 200KB */
          {
            /* Static buffer for image to avoid heap fragmentation */
            static uint8_t s_img_buffer[128 * 1024]; /* 128KB max */
            size_t read_len = (len > sizeof(s_img_buffer)) ? sizeof(s_img_buffer) : len;

            if (camera_read_fifo_burst(s_img_buffer, read_len))
            {
              char filename[32];
              snprintf(filename, sizeof(filename), "/IMAGES/img_%lu.jpg", (unsigned long)s_counter);
              storage_write_image(filename, s_img_buffer, (uint32_t)read_len);
              printf("[PayloadTask] Image saved: %s (%zu bytes)\n", filename, read_len);
            }
          }
        }
      }
    }
  }

  /* Periodic Sampling: only active in FM_PAYLOAD */
  if (fmm_get_mode() == FM_PAYLOAD)
  {
    if (!payload_manager_get_status().rail_enabled)
    {
      payload_manager_enable(true);
    }

    /* 10Hz: Magnetometer */
    rm3100_vector_t mag_vec;
    if (rm3100_read_vector(&mag_vec, 10))
    {
      /* Store in DLA for telemetry */
      float field[3] = {mag_vec.x_nT, mag_vec.y_nT, mag_vec.z_nT};
      data_layer_write_mag(field);
    }

    /* 1Hz: Radiation */
    if ((s_counter % RADIATION_PERIOD_DIV) == 0)
    {
      float dose = radiation_driver_read_dose();
      data_layer_write_radiation(dose);

      /* Update payload status (img count placeholder) */
      payload_status_t ps = payload_manager_get_status();
      data_layer_write_payload_status(ps.rail_enabled, ps.image_count);

      /* Log to SD card */
      char log_entry[64];
      snprintf(log_entry, sizeof(log_entry), "MAG: %.1f,%.1f,%.1f RAD: %.2f\n", mag_vec.x_nT,
               mag_vec.y_nT, mag_vec.z_nT, dose);
      storage_append_log("/LOGS/science.dat", (uint8_t *)log_entry, strlen(log_entry));
    }
  }
  else
  {
    /* Ensure payload is OFF if not in PAYLOAD mode */
    if (payload_manager_get_status().rail_enabled)
    {
      payload_manager_enable(false);
    }
  }

  s_counter++;
}

void vPayloadTask(void *pvParameters)
{
  (void)pvParameters;
  s_last_wake_time = xTaskGetTickCount();

  printf("[PayloadTask] Started\n");

  while (1)
  {
    vPayloadTask_Step();
    vTaskDelayUntil(&s_last_wake_time, pdMS_TO_TICKS(PAYLOAD_PERIOD_MS));
  }
}
