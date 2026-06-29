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
#include "wcet_profiler.h"

#include <stdio.h>
#include <string.h>

#if defined(PICO_BUILD)
  #include "pico/time.h"
#endif

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
        static uint8_t s_img_buffer[128 * 1024]; /* 128KB — QVGA JPEG */
        uint32_t read_len = sizeof(s_img_buffer);

        if (camera_capture(5000))
        {
          if (camera_read_fifo_burst(s_img_buffer, read_len))
          {
            /* Quick JPEG validation: check SOI+EOI markers */
            bool has_soi = (s_img_buffer[0] == 0xFF && s_img_buffer[1] == 0xD8);

            uint32_t eoi_offset = 0;
            for (uint32_t i = 2; i + 1 < read_len; i++)
            {
              if (s_img_buffer[i] == 0xFF && s_img_buffer[i + 1] == 0xD9)
              {
                eoi_offset = i;
                break;
              }
            }

            uint32_t img_size = (eoi_offset > 0) ? eoi_offset + 2 : read_len;
            printf("[PayloadTask] JPEG: SOI=%s EOI=%s size=%lu\n", has_soi ? "YES" : "NO",
                   eoi_offset > 0 ? "YES" : "NO", (unsigned long)img_size);

            char filename[32];
            snprintf(filename, sizeof(filename), "/IMAGES/img_%lu.jpg", (unsigned long)s_counter);
            storage_status_t st = storage_write_image(filename, s_img_buffer, img_size);
            if (st == STORAGE_OK)
            {
              printf("[PayloadTask] Image saved: %s (%lu bytes)\n", filename,
                     (unsigned long)img_size);
            }
            else
            {
              printf("[PayloadTask] ERROR: storage_write_image failed for %s (status=%d)\n",
                     filename, (int)st);
            }
          }
          else
          {
            printf("[PayloadTask] FIFO burst read FAILED\n");
          }
        }
        else
        {
          printf("[PayloadTask] camera_capture FAILED\n");
        }
      }
      else
      {
        printf("[PayloadTask] camera_init FAILED\n");
      }
    }
    else if (notify_value & PAYLOAD_NOTIFY_DUMP_IMAGE)
    {
      printf("[PayloadTask] IMGDUMP command received\n");
      if (camera_init())
      {
        static uint8_t dump_buf[128 * 1024];
        uint32_t read_len = sizeof(dump_buf);

        if (camera_capture(5000))
        {
          if (camera_read_fifo_burst(dump_buf, read_len))
          {
            /* Find actual JPEG size */
            uint32_t eoi_offset = 0;
            for (uint32_t i = 2; i + 1 < read_len; i++)
            {
              if (dump_buf[i] == 0xFF && dump_buf[i + 1] == 0xD9)
              {
                eoi_offset = i;
                break;
              }
            }
            uint32_t img_size = (eoi_offset > 0) ? eoi_offset + 2 : read_len;
            printf("[PayloadTask] IMGDUMP: JPEG size=%lu, dumping %lu bytes as hex\n",
                   (unsigned long)img_size, (unsigned long)img_size);

            /* Hex dump: 16 bytes per line, lowercase hex */
            for (uint32_t i = 0; i < img_size; i++)
            {
              if (i % 16 == 0)
              {
                printf("\n  ");
              }
              printf("%02x ", dump_buf[i]);
            }
            printf("\n[PayloadTask] IMGDUMP: end (%lu bytes)\n", (unsigned long)img_size);
          }
          else
          {
            printf("[PayloadTask] IMGDUMP: FIFO burst read FAILED\n");
          }
        }
        else
        {
          printf("[PayloadTask] IMGDUMP: camera_capture FAILED\n");
        }
      }
      else
      {
        printf("[PayloadTask] IMGDUMP: camera_init FAILED\n");
      }
    }
    else
    {
      printf("[PayloadTask] Unknown notify value: 0x%08lX\n", (unsigned long)notify_value);
    }

    /* Clear notification flags */
    (void)notify_value;
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
    wcet_task_begin(WCET_TASK_PAYLOAD);
    vPayloadTask_Step();
    wcet_task_end(WCET_TASK_PAYLOAD);
    vTaskDelayUntil(&s_last_wake_time, pdMS_TO_TICKS(PAYLOAD_PERIOD_MS));
  }
}
