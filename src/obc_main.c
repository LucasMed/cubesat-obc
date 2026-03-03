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
    vTaskDelay(pdMS_TO_TICKS(200));
    /* Poll CYW43 so the SPI command is processed (required with arch_none) */
    cyw43_arch_poll();
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
    vTaskDelay(pdMS_TO_TICKS(800));
    cyw43_arch_poll();
  }
}

/* Heartbeat — proof of life every 2 s with explicit fflush so USB CDC
 * delivers the output promptly (without fflush the buffer sits for up to
 * PICO_STDIO_USB_STDOUT_TIMEOUT_US = 500 ms before the host sees it). */
static void vHeartbeatTask(void *pvParameters)
{
  (void)pvParameters;
  uint32_t tick = 0;
  for (;;)
  {
    printf("[HB %lu] heap=%lu\r\n", (unsigned long)tick++, (unsigned long)xPortGetFreeHeapSize());
    fflush(stdout);
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

  printf("  creating tasks...\r\n");
  fflush(stdout);
#ifdef PICO_BUILD
  xTaskCreate(vLedBlinkTask, "LEDBlink", 256, NULL, tskIDLE_PRIORITY + 1, NULL);
  xTaskCreate(vHeartbeatTask, "Heartbeat", 512, NULL, configMAX_PRIORITIES - 1, NULL);
#endif
  xTaskCreate(vSensorReadTask, "SensorRead", 512, NULL, tskIDLE_PRIORITY + 3, NULL);
  xTaskCreate(vAttitudeControlTask, "AttitudeCtrl", 512, NULL, tskIDLE_PRIORITY + 3, NULL);
  xTaskCreate(vTelemetryTask, "Telemetry", 512, NULL, tskIDLE_PRIORITY + 2, NULL);
  xTaskCreate(vCommandTask, "Command", 1024, NULL, tskIDLE_PRIORITY + 2, NULL);
  xTaskCreate(vHealthMonitorTask, "HealthMonitor", 512, NULL, tskIDLE_PRIORITY + 1, NULL);

  printf("[STARTUP] done — deleting startup task\r\n");
  fflush(stdout);
  vTaskDelete(NULL);
}

int main(void)
{
#ifdef PICO_BUILD
  stdio_init_all();

  printf("\r\n[BOOT] CubeSat OBC firmware started\r\n");
  fflush(stdout);
  /* Wait up to 3 s for USB CDC host to enumerate (UART works immediately). */
  for (int i = 0; i < 30 && !stdio_usb_connected(); i++)
    sleep_ms(100);
  if (cyw43_arch_init())
  {
    printf("[WARN] CYW43 init failed — LED disabled\r\n");
    fflush(stdout);
  }
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
