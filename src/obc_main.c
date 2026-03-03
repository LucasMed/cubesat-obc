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
#endif

int main(void)
{
#ifdef PICO_BUILD
  stdio_init_all();

  /* Early UART marker — visible at 115200 on GP0/TX even before USB connects */
  printf("\r\n[BOOT] CubeSat OBC firmware started\r\n");
  fflush(stdout);

  printf("\n=====================================\n");
  printf("  CubeSat OBC - Pico 2W Firmware\n");
  printf("  FreeRTOS Real Kernel\n");
  printf("=====================================\n\n");

  if (cyw43_arch_init())
  {
    /* CYW43 init failed — LED will not work but firmware continues.
     * This is non-fatal: the OBC can operate without the status LED. */
    printf("[WARN] CYW43 init failed — LED disabled\r\n");
    fflush(stdout);
    /* Do NOT return here: returning from main() in embedded is undefined.
     * Continue — the OBC subsystems do not require CYW43. */
  }

  /* Wait briefly for USB CDC host to connect (non-blocking: use UART if no USB) */
  for (int i = 0; i < 20; i++)
  {
    printf(".");
    sleep_ms(100);
  }
  printf("\nStartup delay finished.\r\n");
  fflush(stdout);
#else
  printf("=== CubeSat OBC Firmware (Host Simulation) ===\n");
#endif

  // Initialize System State
  printf("Initializing system state...\n");
  fflush(stdout);
  system_state_init();

  // Initialize Communications
  comm_init();

  // Initialize Fault Manager and EPS Monitor
  printf("Initializing fault manager...\n");
  fflush(stdout);
  fault_manager_init();

  printf("Initializing EPS monitor...\n");
  fflush(stdout);
  eps_monitor_init();

#ifdef PICO_BUILD
  // Initialize I2C Bus
  printf("Initializing I2C bus...\n");
  fflush(stdout);
  i2c_bus_init(I2C_SDA_PIN, I2C_SCL_PIN, 400000);
#endif

  // Initialize Sensors
  printf("Initializing sensors...\n");
  fflush(stdout);
  int imu_res = mpu6050_init();
  int temp_res = temperature_init();

  system_state_set_available(imu_res == 0, temp_res == 0);

  printf("Creating FreeRTOS tasks...\n");
  fflush(stdout);

#ifdef PICO_BUILD
  // LED blink task (diagnostic on hardware)
  xTaskCreate(vLedBlinkTask, "LEDBlink", 256, NULL, tskIDLE_PRIORITY + 2, NULL);
#endif

  // OBC Functional Tasks
  xTaskCreate(vSensorReadTask, "SensorRead", 512, NULL, tskIDLE_PRIORITY + 4, NULL);
  xTaskCreate(vAttitudeControlTask, "AttitudeControl", 512, NULL, tskIDLE_PRIORITY + 4, NULL);
  xTaskCreate(vTelemetryTask, "Telemetry", 512, NULL, tskIDLE_PRIORITY + 3, NULL);
  xTaskCreate(vCommandTask, "Command", 1024, NULL, tskIDLE_PRIORITY + 3, NULL);
  xTaskCreate(vHealthMonitorTask, "HealthMonitor", 512, NULL, tskIDLE_PRIORITY + 2, NULL);

  printf("Starting FreeRTOS scheduler...\n");
  fflush(stdout);
#ifdef PICO_BUILD
  sleep_ms(100);  // Small pause to let serial buffers clear
#endif

  vTaskStartScheduler();

  // Should never reach here
  printf("ERROR: FreeRTOS scheduler exited!\n");
  while (1)
  {
    ;
  }

  return 0;
}

#ifdef PICO_BUILD
// FreeRTOS hooks moved to freertos_hooks.c
#endif
