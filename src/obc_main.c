/**
 * @file obc_main.c
 * @brief CubeSat OBC Main Entry Point
 *
 * Supports both host simulation and Pico 2W hardware builds.
 */

#include "FreeRTOS.h"
#include "task.h"

#include <stdio.h>

// Project headers
#include "attitude_control_task.h"
#include "comm_init.h"
#include "command_task.h"
#include "config.h"
#include "drivers/i2c_interface.h"
#include "drivers/imu/mpu6050.h"
#include "drivers/temperature.h"
#include "eps.h"
#include "fault_manager.h"
#include "health_monitor_task.h"
#include "sensor_read_task.h"
#include "system_state.h"
#include "telemetry_task.h"

#ifdef PICO_BUILD
  #include "hardware/watchdog.h"
  #include "pico/cyw43_arch.h"
  #include "pico/stdlib.h"

// LED Blink Helper (for diagnostics on hardware)
void vLedBlinkTask(void *pvParameters)
{
  (void)pvParameters;
  for (;;)
  {
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
    cyw43_arch_poll(); /* process SPI command NOW, before yielding */
    vTaskDelay(pdMS_TO_TICKS(200));
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
    cyw43_arch_poll();
    vTaskDelay(pdMS_TO_TICKS(800));
  }
}

/* Heartbeat — proof of life every 2 s.
 * No fflush: pico USB CDC ring buffer is drained by the USB IRQ.
 * An explicit fflush while the buffer is full can deadlock the stdio mutex. */
static void vHeartbeatTask(void *pvParameters)
{
  (void)pvParameters;
  uint32_t tick = 0;
  for (;;)
  {
    printf("[HB %lu] heap=%lu tick=%lu\r\n", (unsigned long)tick,
           (unsigned long)xPortGetFreeHeapSize(), (unsigned long)xTaskGetTickCount());
    printf("  HWM Heartbeat=%lu\r\n", (unsigned long)uxTaskGetStackHighWaterMark(NULL));
    fflush(stdout); /* guarantee output even if pico short-circuit misbehaves */
    tick++;
    vTaskDelay(pdMS_TO_TICKS(2000));
  }
}
#endif

/**
 * vStartupTask — runs at highest priority after vTaskStartScheduler().
 *
 * All FreeRTOS sync-object creation (queues, semaphores, mutexes inside
 * csp_init, etc.) MUST happen here, not in main().  On the RP2350 SMP
 * FreeRTOS port the kernel spinlocks are not initialised until the
 * scheduler starts, so calling xQueueCreateStatic (or anything that
 * calls taskENTER_CRITICAL) from main() before vTaskStartScheduler()
 * causes a deadlock.
 */
static void vStartupTask(void *pvParameters)
{
  (void)pvParameters;

  printf("\r\n[STARTUP] Subsystem init begin\r\n");
  fflush(stdout);

#ifdef PICO_BUILD
  printf("  cyw43_arch_init...\r\n");
  fflush(stdout);
  if (cyw43_arch_init())
  {
    printf("  [WARN] cyw43_arch_init failed -- LED disabled\r\n");
    fflush(stdout);
  }
#endif

  printf("  system_state_init...\r\n");
  fflush(stdout);
  system_state_init();

  printf("  comm_init...\r\n");
  fflush(stdout);
  comm_init();

  printf("  fault_manager_init...\r\n");
  fflush(stdout);
  fault_manager_init();

  printf("  eps_monitor_init...\r\n");
  fflush(stdout);
  eps_monitor_init();

#ifdef PICO_BUILD
  printf("  i2c_bus_init...\r\n");
  fflush(stdout);
  i2c_bus_init(I2C_SDA_PIN, I2C_SCL_PIN, 400000);
#endif

  printf("  sensors...\r\n");
  fflush(stdout);
  int imu_res = mpu6050_init();
  int temp_res = temperature_init();
  system_state_set_available(imu_res == 0, temp_res == 0);
  printf("  IMU: %s  Temp: %s\r\n", imu_res == 0 ? "OK" : "not found",
         temp_res == 0 ? "OK" : "not found");
  fflush(stdout);

  printf("  creating tasks...\r\n");
  fflush(stdout);

  static TaskHandle_t h_sensor = NULL, h_ctrl = NULL, h_telem = NULL;
  static TaskHandle_t h_cmd = NULL, h_health = NULL;
#ifdef PICO_BUILD
  static TaskHandle_t h_led = NULL, h_hb = NULL;
#endif

#define CHK(ret, name)                                                                             \
  do                                                                                               \
  {                                                                                                \
    if ((ret) != pdPASS)                                                                           \
    {                                                                                              \
      printf("  [ERROR] xTaskCreate FAILED: " name "\r\n");                                        \
      fflush(stdout);                                                                              \
    }                                                                                              \
    else                                                                                           \
    {                                                                                              \
      printf("  [OK]    created: " name "\r\n");                                                   \
      fflush(stdout);                                                                              \
    }                                                                                              \
  } while (0)

  /* Create lower-priority tasks first; Heartbeat (highest pri) goes last so
   * it cannot preempt the startup task before all other tasks exist.
   * 2048 words (8 KB) per task: newlib printf with floats + EKF + CSP uses >4 KB. */
  CHK(xTaskCreate(vSensorReadTask, "SensorRead", 2048, NULL, tskIDLE_PRIORITY + 4, &h_sensor),
      "SensorRead");
  CHK(xTaskCreate(vAttitudeControlTask, "AttitudeCtrl", 2048, NULL, tskIDLE_PRIORITY + 3, &h_ctrl),
      "AttitudeCtrl");
  CHK(xTaskCreate(vTelemetryTask, "Telemetry", 2048, NULL, tskIDLE_PRIORITY + 2, &h_telem),
      "Telemetry");
  CHK(xTaskCreate(vCommandTask, "Command", 2048, NULL, tskIDLE_PRIORITY + 2, &h_cmd), "Command");
  CHK(xTaskCreate(vHealthMonitorTask, "HealthMon", 2048, NULL, tskIDLE_PRIORITY + 1, &h_health),
      "HealthMon");
#ifdef PICO_BUILD
  CHK(xTaskCreate(vLedBlinkTask, "LEDBlink", 2048, NULL, tskIDLE_PRIORITY + 1, &h_led), "LEDBlink");
  /* Heartbeat at LOW priority — it's just diagnostic, must not preempt Startup. */
  CHK(xTaskCreate(vHeartbeatTask, "Heartbeat", 2048, NULL, tskIDLE_PRIORITY + 1, &h_hb),
      "Heartbeat");
#endif

#undef CHK

  printf("[STARTUP] done — heap=%lu\r\n", (unsigned long)xPortGetFreeHeapSize());
  fflush(stdout);

  /* Instead of vTaskDelete(NULL) — which may have issues on the SMP kernel
   * with 1 core — lower our priority and turn this task into a slow alive
   * heartbeat.  This avoids the SMP task-deletion code path entirely.      */
  vTaskPrioritySet(NULL, tskIDLE_PRIORITY + 1);
  for (;;)
  {
    printf("[ALIVE] heap=%lu tick=%lu\r\n", (unsigned long)xPortGetFreeHeapSize(),
           (unsigned long)xTaskGetTickCount());
#ifdef PICO_BUILD
    /* Stack HWM in words (lower = more stack used). Target: ≥ 20%% free = ≥ 410 words. */
    printf("  HWM SensorRead  =%4lu  AttitudeCtrl=%4lu\r\n",
           (unsigned long)uxTaskGetStackHighWaterMark(h_sensor),
           (unsigned long)uxTaskGetStackHighWaterMark(h_ctrl));
    printf("  HWM Telemetry   =%4lu  Command     =%4lu\r\n",
           (unsigned long)uxTaskGetStackHighWaterMark(h_telem),
           (unsigned long)uxTaskGetStackHighWaterMark(h_cmd));
    printf("  HWM HealthMon   =%4lu  LEDBlink    =%4lu  Heartbeat=%4lu\r\n",
           (unsigned long)uxTaskGetStackHighWaterMark(h_health),
           (unsigned long)uxTaskGetStackHighWaterMark(h_led),
           (unsigned long)uxTaskGetStackHighWaterMark(h_hb));
#endif
    fflush(stdout);
    vTaskDelay(pdMS_TO_TICKS(5000));
  }
}

int main(void)
{
#ifdef PICO_BUILD
  stdio_init_all();
  setvbuf(stdout, NULL, _IONBF, 0); /* fully unbuffered — every write goes to USB immediately */

  /* Wait up to 3 s for USB CDC host to enumerate (UART works immediately). */
  for (int i = 0; i < 30 && !stdio_usb_connected(); i++)
    sleep_ms(100);

  /* Check if we rebooted due to a stack overflow (watchdog scratch magic). */
  if (watchdog_hw->scratch[0] == 0xDEAD0001u)
  {
    char name[13] = {0};
    for (int i = 0; i < 3; i++)
    {
      uint32_t w = watchdog_hw->scratch[1 + i];
      name[i * 4 + 0] = (char)(w & 0xFF);
      name[i * 4 + 1] = (char)((w >> 8) & 0xFF);
      name[i * 4 + 2] = (char)((w >> 16) & 0xFF);
      name[i * 4 + 3] = (char)((w >> 24) & 0xFF);
    }
    name[12] = '\0';
    watchdog_hw->scratch[0] = 0; /* clear so we don't repeat */
    printf("\r\n*** REBOOTED: Stack overflow in task '%s' ***\r\n\r\n", name);
    fflush(stdout);
  }

  printf("\r\n[BOOT] CubeSat OBC firmware started\r\n");
  fflush(stdout);
#else
  printf("=== CubeSat OBC Firmware (Host Simulation) ===\n");
#endif

  /* Create ONE startup task — all subsystem init happens inside it after
   * the scheduler starts and SMP spinlocks are fully initialised.        */
  xTaskCreate(vStartupTask, "Startup", 2048, NULL, configMAX_PRIORITIES - 1, NULL);

  printf("[BOOT] starting scheduler...\r\n");
  fflush(stdout);

  vTaskStartScheduler();

  printf("[ERROR] scheduler exited!\r\n");
  for (;;)
  {
  }
  return 0;
}

#ifdef PICO_BUILD
// FreeRTOS hooks moved to freertos_hooks.c
#endif
