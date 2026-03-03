#include "health_monitor_task.h"

#include "FreeRTOS.h"
#include "config.h"
#include "eps.h"
#include "fault_ids.h"
#include "fault_manager.h"
#include "task.h"
#include "watchdog_hal.h"

#include <stdio.h>

// Core logic for health monitoring (independent of FreeRTOS task loop)
void vHealthMonitorTask_Step(void)
{
  /* 1. Feed the hardware watchdog — must happen every health-monitor tick. */
  watchdog_hal_feed();

  /* 2. Check if the previous reset was watchdog-induced.
   * If so raise a CRITICAL fault which immediately forces FM_SAFE via
   * fmm_force_safe() inside fault_report().
   * The check is intentionally done before fault_manager_tick() so the
   * CRITICAL entry is visible in the fault table on the same tick. */
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

// Health monitor task: monitors system every 5 seconds
void vHealthMonitorTask(void *pvParameters)
{
  (void)pvParameters;
  TickType_t xLastWakeTime = xTaskGetTickCount();
  const TickType_t xFrequency = pdMS_TO_TICKS(5000);  // 5 Hz (0.2 Hz logical)

  printf("[health_monitor_task] Started\n");

  while (1)
  {
    vHealthMonitorTask_Step();
    vTaskDelayUntil(&xLastWakeTime, xFrequency);
  }
}
