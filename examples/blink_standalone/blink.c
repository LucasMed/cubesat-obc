/**
 * blink.c — Full OBC subsystem test for Pico 2W
 *
 * Replicates obc_main.c exactly:
 *   • Same init sequence (system_state, comm, fault, eps, i2c, sensors)
 *   • Same FreeRTOS tasks at same priorities
 *   • LED blink task + extra heartbeat task for live UART confirmation
 *
 * If this runs but obc_main.c doesn’t, the problem is in obc_main.c itself.
 * If this also crashes, we’ll narrow down by commenting out sections.
 */

#include "FreeRTOS.h"
#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"
#include "task.h"

#include <stdio.h>

// OBC subsystem headers (same as obc_main.c)
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

/* ── LED blink task (same as obc_main.c) ──────────────────────────────── */
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

/* ── Heartbeat task (confirms scheduler is alive via UART) ────────────── */
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

/* ── main ──────────────────────────────────────────────────────────────── */
int main(void)
{
  stdio_init_all();

  printf("\r\n[BOOT] OBC full-subsystem test started\r\n");
  fflush(stdout);

  /* Wait for USB CDC host (non-blocking on UART) */
  for (int i = 0; i < 50 && !stdio_usb_connected(); i++)
    sleep_ms(100);

  printf("\r\n===================================\r\n");
  printf("  Pico 2W — Full OBC subsystem test\r\n");
  printf("===================================\r\n\r\n");
  fflush(stdout);

  /* CYW43 init */
  if (cyw43_arch_init())
  {
    printf("[WARN] cyw43_arch_init failed — LED disabled\r\n");
    fflush(stdout);
  }

  /* OBC subsystem init — mirrors obc_main.c */
  printf("Initializing system state...\r\n");
  fflush(stdout);
  system_state_init();

  printf("Initializing communications...\r\n");
  fflush(stdout);
  comm_init();

  printf("Initializing fault manager...\r\n");
  fflush(stdout);
  fault_manager_init();

  printf("Initializing EPS monitor...\r\n");
  fflush(stdout);
  eps_monitor_init();

  printf("Initializing I2C bus...\r\n");
  fflush(stdout);
  i2c_bus_init(I2C_SDA_PIN, I2C_SCL_PIN, 400000);

  printf("Initializing sensors...\r\n");
  fflush(stdout);
  int imu_ok = mpu6050_init();
  int temp_ok = temperature_init();
  system_state_set_available(imu_ok == 0, temp_ok == 0);
  printf("  IMU: %s  Temp: %s\r\n", imu_ok == 0 ? "OK" : "not found",
         temp_ok == 0 ? "OK" : "not found");

  /* FreeRTOS tasks — same as obc_main.c */
  printf("Creating FreeRTOS tasks...\r\n");
  fflush(stdout);
  xTaskCreate(vLedBlinkTask, "LEDBlink", 256, NULL, tskIDLE_PRIORITY + 2, NULL);
  xTaskCreate(vHeartbeatTask, "Heartbeat", 512, NULL, tskIDLE_PRIORITY + 1, NULL);
  xTaskCreate(vSensorReadTask, "SensorRead", 512, NULL, tskIDLE_PRIORITY + 4, NULL);
  xTaskCreate(vAttitudeControlTask, "AttitudeCtrl", 512, NULL, tskIDLE_PRIORITY + 4, NULL);
  xTaskCreate(vTelemetryTask, "Telemetry", 512, NULL, tskIDLE_PRIORITY + 3, NULL);
  xTaskCreate(vCommandTask, "Command", 1024, NULL, tskIDLE_PRIORITY + 3, NULL);
  xTaskCreate(vHealthMonitorTask, "HealthMonitor", 512, NULL, tskIDLE_PRIORITY + 2, NULL);

  printf("Starting FreeRTOS scheduler...\r\n");
  fflush(stdout);
  sleep_ms(100);

  vTaskStartScheduler();

  printf("[ERROR] Scheduler exited!\r\n");
  for (;;)
  {
  }
  return 0;
}

/* ── LED blink task ─────────────────────────────────────────────────────────
 * Same timing and poll pattern as vLedBlinkTask in obc_main.c
 */
static void vLedBlinkTask(void *pvParameters)
{
  (void)pvParameters;
  for (;;)
  {
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
    vTaskDelay(pdMS_TO_TICKS(200));
    cyw43_arch_poll(); /* required with pico_cyw43_arch_none */

    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
    vTaskDelay(pdMS_TO_TICKS(800));
    cyw43_arch_poll();
  }
}

/* ── Heartbeat / UART telemetry task ─────────────────────────────────────── */
static void vHeartbeatTask(void *pvParameters)
{
  (void)pvParameters;
  uint32_t tick = 0;
  for (;;)
  {
    printf("[TICK %lu] STATUS:mode=NOMINAL,heap=%lu\r\n", (unsigned long)tick++,
           (unsigned long)xPortGetFreeHeapSize());
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

/* ── main ────────────────────────────────────────────────────────────────── */
int main(void)
{
  stdio_init_all();

  /* Early UART marker — visible on GP0 TX at 115200 even before USB connects */
  printf("\r\n[BOOT] blink_pico2w (FreeRTOS) started\r\n");
  fflush(stdout);

  /* Wait up to 5 s for USB CDC host before continuing (non-blocking on UART) */
  for (int i = 0; i < 50 && !stdio_usb_connected(); i++)
    sleep_ms(100);

  printf("\r\n====================================\r\n");
  printf("  Pico 2W / RP2350 — FreeRTOS test\r\n");
  printf("====================================\r\n\r\n");
  fflush(stdout);

  /* Initialise CYW43 — same call as obc_main.c */
  if (cyw43_arch_init())
  {
    printf("[WARN] cyw43_arch_init failed — LED disabled\r\n");
    fflush(stdout);
    /* Non-fatal: continue without LED, same policy as OBC firmware */
  }

  /* Create tasks — same priorities and stack sizes as obc_main.c */
  xTaskCreate(vLedBlinkTask, "LEDBlink", 256, NULL, tskIDLE_PRIORITY + 2, NULL);
  xTaskCreate(vHeartbeatTask, "Heartbeat", 512, NULL, tskIDLE_PRIORITY + 1, NULL);

  printf("[OK] Starting FreeRTOS scheduler...\r\n");
  fflush(stdout);
  sleep_ms(100); /* let serial buffers flush before scheduler takes over */

  vTaskStartScheduler();

  /* Should never reach here */
  printf("[ERROR] Scheduler exited!\r\n");
  for (;;)
  {
  }
  return 0;
}
