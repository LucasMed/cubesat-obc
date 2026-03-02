#include "health_monitor_task.h"

#include "FreeRTOS.h"
#include "config.h"
#include "eps.h"
#include "fault_manager.h"
#include "task.h"
#include "watchdog_hal.h"

#include <stdio.h>

// Core logic for health monitoring (independent of FreeRTOS task loop)
void vHealthMonitorTask_Step(void)
{
  /* 1. Feed the hardware watchdog — must happen every health-monitor tick. */
  watchdog_hal_feed();

  /* 2. Periodic Fault Manager age / auto-clear pass (1 Hz) */
  fault_manager_tick();

  /* 3. EPS voltage classification, fault raise/clear, rail control */
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
