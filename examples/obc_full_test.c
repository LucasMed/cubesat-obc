/**
 * obc_full_test.c — Full OBC subsystem test for Pico 2W
 *
 * Replicates obc_main.c exactly:
 *   • Same init sequence (system_state, comm, fault, eps, i2c, sensors)
 *   • Same FreeRTOS tasks at same priorities
 *   • LED blink task + heartbeat task for live UART confirmation
 *
 * Build with the normal pipeline:
 *   bash scripts/pico_ci.sh pico-build
 *
 * The resulting binary is:
 *   build_pico_ci/examples/obc_full_test.uf2
 *
 * UART output (GP0 TX, 115200) will show each init step.
 * If the system hangs at a specific step, that's the failing subsystem.
 */

#include "FreeRTOS.h"
#include "hardware/watchdog.h"
#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"
#include "task.h"

#include <stdio.h>

/* OBC subsystem headers — same as obc_main.c */
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

/* ── LED blink task (identical to obc_main.c) ─────────────────────────────── */
static void vLedBlinkTask(void *pvParameters)
{
  (void)pvParameters;
  for (;;)
  {
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
    vTaskDelay(pdMS_TO_TICKS(200));
    cyw43_arch_poll();
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
    vTaskDelay(pdMS_TO_TICKS(800));
    cyw43_arch_poll();
  }
}

/* ── Heartbeat task — confirms scheduler running and prints free heap ──────── */
static void vHeartbeatTask(void *pvParameters)
{
  (void)pvParameters;
  uint32_t tick = 0;
  for (;;)
  {
    printf("[HB %lu] heap=%lu\r\n", (unsigned long)tick++, (unsigned long)xPortGetFreeHeapSize());
    vTaskDelay(pdMS_TO_TICKS(2000));
  }
}

/* ── Startup task — all FreeRTOS init runs here, AFTER the scheduler ────── */
/**
 * On the RP2350 SMP FreeRTOS port the kernel spinlocks are not ready until
 * vTaskStartScheduler() completes.  Any call to xQueueCreateStatic (inside
 * csp_init, etc.) before the scheduler start causes a spin-lock deadlock.
 * Solution: do all subsystem init inside a highest-priority startup task.
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

  printf("  i2c_bus_init...\r\n");
  fflush(stdout);
  i2c_bus_init(I2C_SDA_PIN, I2C_SCL_PIN, 400000);

  printf("  sensors...\r\n");
  fflush(stdout);
  int imu_ok = mpu6050_init();
  int temp_ok = temperature_init();
  system_state_set_available(imu_ok == 0, temp_ok == 0);
  printf("  IMU: %s  Temp: %s\r\n", imu_ok == 0 ? "OK" : "not found",
         temp_ok == 0 ? "OK" : "not found");
  fflush(stdout);

  printf("  creating tasks...\r\n");
  fflush(stdout);
  /* configMAX_PRIORITIES=5 → valid range 0-4. tskIDLE_PRIORITY+N where N>=5 triggers configASSERT
   * hang. */
  xTaskCreate(vHeartbeatTask, "Heartbeat", 512, NULL, configMAX_PRIORITIES - 1, NULL);      /* 4 */
  xTaskCreate(vSensorReadTask, "SensorRead", 512, NULL, tskIDLE_PRIORITY + 3, NULL);        /* 3 */
  xTaskCreate(vAttitudeControlTask, "AttitudeCtrl", 512, NULL, tskIDLE_PRIORITY + 3, NULL); /* 3 */
  xTaskCreate(vTelemetryTask, "Telemetry", 512, NULL, tskIDLE_PRIORITY + 2, NULL);          /* 2 */
  xTaskCreate(vCommandTask, "Command", 1024, NULL, tskIDLE_PRIORITY + 2, NULL);             /* 2 */
  xTaskCreate(vHealthMonitorTask, "HealthMonitor", 512, NULL, tskIDLE_PRIORITY + 1, NULL);  /* 1 */
  xTaskCreate(vLedBlinkTask, "LEDBlink", 256, NULL, tskIDLE_PRIORITY + 1, NULL);            /* 1 */

  printf("[STARTUP] done\r\n");
  fflush(stdout);
  vTaskDelete(NULL);
}

/* ── main ──────────────────────────────────────────────────────────────────── */
int main(void)
{
  stdio_init_all();

  /* Wait up to 3 s for USB CDC host (UART works immediately) */
  for (int i = 0; i < 30 && !stdio_usb_connected(); i++)
    sleep_ms(100);

  printf("\r\n===================================\r\n");
  printf("  Pico 2W -- Full OBC subsystem test\r\n");
  printf("===================================\r\n\r\n");
  fflush(stdout);

  if (cyw43_arch_init())
  {
    printf("[WARN] cyw43_arch_init failed -- LED disabled\r\n");
    fflush(stdout);
  }

  /* Create ONE startup task — everything else happens inside it after the
   * scheduler starts and SMP spinlocks are fully initialised.           */
  xTaskCreate(vStartupTask, "Startup", 2048, NULL, configMAX_PRIORITIES - 1, NULL);

  printf("[BOOT] starting scheduler...\r\n");
  fflush(stdout);

  vTaskStartScheduler();

  printf("[ERROR] Scheduler exited!\r\n");
  for (;;)
  {
  }
  return 0;
}
