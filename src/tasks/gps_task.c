// gps_task.c -- FreeRTOS task for GPS polling and data layer update
#include "FreeRTOS.h"
#include "data_layer.h"
#include "gps_driver.h"
#include "task.h"

#include <stdio.h>

#define GPS_TASK_PERIOD_MS 1000

void gps_task(void *pvParameters)
{
  (void)pvParameters;
  TickType_t xLastWakeTime = xTaskGetTickCount();
  while (1)
  {
    GpsFix_t *fix = gps_read_fix();
    if (fix)
    {
      data_layer_set_gps_fix(fix);
    }
    vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(GPS_TASK_PERIOD_MS));
  }
}
