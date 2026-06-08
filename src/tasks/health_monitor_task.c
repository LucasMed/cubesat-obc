#include "health_monitor_task.h"

#include "FreeRTOS.h"
#include "config.h"
#include "eps.h"
#include "fault_ids.h"
#include "fault_manager.h"
#include "flight_mode.h"
#include "task.h"
#include "watchdog_hal.h"
#include "wcet_profiler.h"

#include <stdio.h>

// Core logic for health monitoring (independent of FreeRTOS task loop)
void vHealthMonitorTask_Step(void)
{
  /* 1. Feed the hardware watchdog — must happen every health-monitor tick. */
  watchdog_hal_feed();

  /* 2. Check if the previous reset was watchdog-induced.
   * If so raise a CRITICAL fault which immediately triggers the notification
   * mechanism to transition to FM_SAFE.
   *
   * Clear the triggered flag after reading so subsequent ticks do not
   * re-raise the fault and keep forcing SAFE mode indefinitely. */
  if (watchdog_hal_triggered())
  {
    watchdog_hal_clear_triggered();
    fault_report(FAULT_WDT_KICK_MISSED, FAULT_LEVEL_CRITICAL);
  }

  /* 3. Periodic Fault Manager age / auto-clear pass (1 Hz) */
  fault_manager_tick();

  /* 4. EPS voltage classification, fault raise/clear, rail control */
  eps_monitor_tick();

  /* 5. Print WCET report every ~20 cycles (~100 s) — no-op on host builds */
  {
    static uint32_t s_report_count = 0;
    s_report_count++;
    if (s_report_count % 20 == 0)
    {
      wcet_profiler_print_report(NULL);
      wcet_profiler_reset();
    }
  }

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
      wcet_task_begin(WCET_TASK_HEALTH_MON);
      vHealthMonitorTask_Step();
      wcet_task_end(WCET_TASK_HEALTH_MON);
      xLastWakeTime = xNow;
    }

    /* Safety net: if a CRITICAL fault is active but we missed the notification,
     * force safe mode at the start of every periodic check.
     * This ensures EMERGENCY energy state ALWAYS triggers FM_SAFE. */
    if (fault_get_highest_level() >= FAULT_LEVEL_CRITICAL)
    {
      printf("[health_monitor_task] CRITICAL fault detected in safety net — forcing SAFE\n");
      fmm_force_safe();
    }
  }
}
