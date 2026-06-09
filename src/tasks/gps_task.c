// gps_task.c -- FreeRTOS task for GPS polling and data layer update
#include "FreeRTOS.h"
#include "data_layer.h"
#include "fault_ids.h"
#include "fault_manager.h"
#include "gps_driver.h"
#include "task.h"
#include "wcet_profiler.h"

#include <stdint.h>
#include <stdio.h>

#define GPS_TASK_PERIOD_MS 1000
#define GPS_TIMEOUT_TICKS 30  // 30 seconds no DATA = timeout fault

// Core GPS polling logic (independent of FreeRTOS task loop)
void vGpsTask_Step(uint32_t *no_data_count, uint32_t *prev_timestamp)
{
  wcet_task_begin(WCET_TASK_GPS);
  GpsFix_t *fix = gps_read_fix();

  if (fix != NULL)
  {
    // Check if we have NEW data (timestamp changed)
    // Note: timestamp is 0 if never updated, >0 if data received
    bool new_data = (fix->timestamp_ms != *prev_timestamp) && (fix->timestamp_ms > 0);

    if (new_data)
    {
      *prev_timestamp = fix->timestamp_ms;
    }

    if (fix->valid)
    {
      // Valid fix - reset counters
      data_layer_set_gps_fix(fix);
      if (*no_data_count > 0)
      {
        printf("[gps_task] GPS active after %lu polls no data\r\n", (unsigned long)*no_data_count);
        fault_clear(FAULT_GPS_TIMEOUT);
        *no_data_count = 0;
      }
    }
    else if (new_data)
    {
      // Data received but no fix yet - GPS is working, just no satellites
      // Don't count this as no-data timeout
      printf("[gps_task] GPS data OK, no fix yet (%lu sats)\r\n", (unsigned long)fix->satellites);
      *no_data_count = 0;  // Reset - we're receiving data
    }
    else
    {
      // No new data in this poll
      (*no_data_count)++;
    }
  }
  else
  {
    // NULL = no data in buffer at all
    (*no_data_count)++;
  }

  // Report fault only when truly no data for extended period
  if (*no_data_count >= GPS_TIMEOUT_TICKS)
  {
    fault_report(FAULT_GPS_TIMEOUT, FAULT_LEVEL_WARNING);
    printf("[gps_task] GPS no data timeout (%lu)\r\n", (unsigned long)*no_data_count);
  }

  wcet_task_end(WCET_TASK_GPS);
}

void gps_task(void *pvParameters)
{
  (void)pvParameters;
  TickType_t xLastWakeTime = xTaskGetTickCount();
  uint32_t no_data_count = 0;   // Track consecutive polls with no new data
  uint32_t prev_timestamp = 0;  // Track last fix timestamp

  while (1)
  {
    vGpsTask_Step(&no_data_count, &prev_timestamp);
    vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(GPS_TASK_PERIOD_MS));
  }
}
