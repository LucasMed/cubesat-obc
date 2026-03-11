#include "health_monitor_task.h"

#include "FreeRTOS.h"
#include "config.h"
#include "eps.h"
#include "fault_ids.h"
#include "fault_manager.h"
#include "flight_mode.h"
#include "task.h"
#include "watchdog_hal.h"

#include <stdio.h>

// Core logic for health monitoring (independent of FreeRTOS task loop)
void vHealthMonitorTask_Step(void)
{
  /* 1. Feed the hardware watchdog — must happen every health-monitor tick. */
  watchdog_hal_feed();

  /* 2. Check if the previous reset was watchdog-induced.
   * If so raise a CRITICAL fault which immediately triggers the notification
   * mechanism to transition to FM_SAFE. */
  if (watchdog_hal_triggered())
  {
    fault_report(FAULT_WDT_KICK_MISSED, FAULT_LEVEL_CRITICAL);
  }

  /* 3. Periodic Fault Manager age / auto-clear pass (1 Hz) */
  fault_manager_tick();

  /* 4. EPS voltage classification, fault raise/clear, rail control */
  eps_monitor_tick();

  printf("[health_monitor_task] Health check\n");
}

// Health monitor task: monitors system every 5 seconds, but wakes for FDIR signals
void vHealthMonitorTask(void *pvParameters)
{
  (void)pvParameters;
  TickType_t xLastWakeTime = xTaskGetTickCount();
  const TickType_t xFrequency = pdMS_TO_TICKS(5000);

  printf("[health_monitor_task] Started\n");
  fflush(stdout);

  while (1)
  {
    uint32_t ulNotifiedValue = 0;
    TickType_t xNow = xTaskGetTickCount();
    TickType_t xTimeToWait;

    /* Calculate how much time remains until the next periodic health check */
    if (xNow < xLastWakeTime + xFrequency)
    {
      xTimeToWait = (xLastWakeTime + xFrequency) - xNow;
    }
    else
    {
      xTimeToWait = 0;
    }

    /* Wait for notifications (CRITICAL faults) or periodic timeout */
    if (xTaskNotifyWait(0, HM_NOTIFY_FAULT_CRITICAL, &ulNotifiedValue, xTimeToWait) == pdPASS)
    {
      if (ulNotifiedValue & HM_NOTIFY_FAULT_CRITICAL)
      {
        printf("[health_monitor_task] CRITICAL FAULT NOTIFICATION received\n");
        fmm_force_safe();
      }
    }

    /* Periodic step execution */
    xNow = xTaskGetTickCount();
    if (xNow >= xLastWakeTime + xFrequency)
    {
      vHealthMonitorTask_Step();
      xLastWakeTime = xNow;
    }
  }
}
