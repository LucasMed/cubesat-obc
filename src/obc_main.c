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
#include "bh1750.h"
#include "boot_meta.h"
#include "comm_init.h"
#include "command_task.h"
#include "config.h"
#include "data_layer.h"
#include "deploy_monitor.h"
#include "drivers/i2c_interface.h"
#include "drivers/imu/mpu6050.h"
#include "drivers/mag/hmc5883l.h"
#include "ds3231.h"
#include "eps.h"
#include "fault_manager.h"
#include "flight_mode.h"
#include "gps_driver.h"
#include "health_monitor_task.h"
#include "ina219.h"
#include "payload_task.h"
#include "post.h"
#include "sensor_read_task.h"
#include "sht31.h"
#include "sun_sensor.h"
#include "system_state.h"
#include "telemetry_task.h"
#include "w25q64.h"
#include "wcet_profiler.h"

#ifdef PICO_BUILD
  #include "hardware/watchdog.h"
  #include "mpu_init.h"
  #include "pico/cyw43_arch.h"
  #include "pico/stdlib.h"
  #include "watchdog_hal.h"

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
    printf("[HB %lu] heap=%lu min_ever=%lu tick=%lu\r\n", (unsigned long)tick,
           (unsigned long)xPortGetFreeHeapSize(), (unsigned long)xPortGetMinimumEverFreeHeapSize(),
           (unsigned long)xTaskGetTickCount());
    printf("  HWM Heartbeat=%lu (used=%lu)\r\n", (unsigned long)uxTaskGetStackHighWaterMark(NULL),
           (unsigned long)(2048u - uxTaskGetStackHighWaterMark(NULL)));

    /* Debug: send periodic message to HC-12 to verify TX is working */
    // uart_puts(uart1, "[CMD] PING OK\r\n");  // REMOVED: was blocking command reception

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
  /* Activate MPU now — scheduler is running, SMP spinlocks and stacks are stable.
   * (configENABLE_MPU=0 means FreeRTOS leaves MPU management to us.) */
  mpu_init();
  printf("  MPU enabled: flash=RO, SRAM=RW, peri=priv\r\n");
  fflush(stdout);

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
  int mag_res = hmc5883l_init();
  bool sht31_res = sht31_init(SHT31_ADDR_DEFAULT);

  /* Start SHT31 in periodic mode (non-blocking reads) */
  if (sht31_res)
  {
    sht31_res = sht31_start_periodic(10);  // 10 Hz for continuous monitoring
    if (sht31_res)
    {
      printf("sht31: Periodic mode started at 10Hz\r\n");
    }
  }
  bool bh1750_res = bh1750_init(BH1750_ADDR_DEFAULT);
  bool ds3231_res = ds3231_init();
  bool ina219_res = ina219_init();
  bool ina219_solar_res = ina219_solar_init();
  bool sun_sensor_res = sun_sensor_init();

  /* DEBUG: Print battery voltage after init */
  ina219_data_t pwr_data = {0};
  if (ina219_res && ina219_read_power(&pwr_data))
  {
    printf("  Battery: %d mV (I=%d mA)\r\n", pwr_data.bus_voltage_mv,
           (int)(pwr_data.current_ua / 1000));
  }

  /* DEBUG: Print solar panel voltage after init */
  ina219_data_t solar_data = {0};
  if (ina219_solar_res && ina219_solar_read_power(&solar_data))
  {
    printf("  Solar panel: %d mV (I=%d mA, P=%d mW)\r\n", solar_data.bus_voltage_mv,
           (int)(solar_data.current_ua / 1000), (int)(solar_data.power_uw / 1000));
  }
  fflush(stdout);

  system_state_set_available(imu_res == 0, sht31_res);
  data_layer_set_mag_avail(mag_res == 0);
  data_layer_set_lux_avail(bh1750_res);
  data_layer_set_rtc_avail(ds3231_res);
  data_layer_set_power_avail(ina219_res);
  data_layer_set_solar_avail(ina219_solar_res);
  data_layer_set_sun_avail(sun_sensor_res);
  printf("  IMU: %s  Temp: %s  Mag: %s  SHT31: %s  BH1750: %s  RTC: %s  PWR: %s  SOLAR: %s  "
         "Sun: %s\r\n",
         imu_res == 0 ? "OK" : "not found", sht31_res ? "OK" : "not found",
         mag_res == 0 ? "OK" : "not found", sht31_res ? "OK" : "not found",
         bh1750_res ? "OK" : "not found", ds3231_res ? "OK" : "not found",
         ina219_res ? "OK" : "not found", ina219_solar_res ? "OK" : "not found",
         sun_sensor_res ? "OK" : "not found");
  fflush(stdout);

  printf("  gps_init...\r\n");
  fflush(stdout);
  bool gps_ok = gps_init();
  printf("  GPS: %s\r\n", gps_ok ? "OK" : "not found");
  fflush(stdout);

#ifdef PICO_BUILD
  printf("  flash_init...\r\n");
  fflush(stdout);
  int flash_res = w25q64_init();
  printf("  Flash: %s\r\n", flash_res == 0 ? "OK" : "not found");
  fflush(stdout);
#endif

  /* --- POST: Power-On Self-Test --- */
  {
    post_record_t post_rec = {0};
    post_run(&post_rec);
    if (post_is_critical_fail(&post_rec))
    {
      printf("[STARTUP] POST CRITICAL FAIL — forcing SAFE mode\r\n");
      fflush(stdout);
      fmm_force_safe();
    }
    else
    {
      printf("[STARTUP] POST OK (boot=%lu reason=%s)\r\n", (unsigned long)post_rec.boot_count,
             post_boot_reason_name(post_rec.boot_reason));
      fflush(stdout);
    }
  }
  /* --- END POST --- */

  printf("  creating tasks...\r\n");
  fflush(stdout);

#ifdef PICO_BUILD
  printf("  wcet_profiler_init...\r\n");
  fflush(stdout);
  (void)wcet_profiler_init();
#endif

#ifdef PICO_BUILD
  /* Task handles — Pico only; HWM printed in ALIVE loop. */
  static TaskHandle_t h_sensor = NULL, h_ctrl = NULL, h_telem = NULL;
  static TaskHandle_t h_cmd = NULL, h_health = NULL, h_gps = NULL;
  static TaskHandle_t h_led = NULL, h_hb = NULL;
  #define HPTR(h) (&(h))
#else
  #define HPTR(h) (NULL)
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
  CHK(xTaskCreate(vSensorReadTask, "SensorRead", 2048, NULL, tskIDLE_PRIORITY + 4, HPTR(h_sensor)),
      "SensorRead");
  CHK(xTaskCreate(vAttitudeControlTask, "AttitudeCtrl", 2048, NULL, tskIDLE_PRIORITY + 3,
                  HPTR(h_ctrl)),
      "AttitudeCtrl");
  CHK(xTaskCreate(vTelemetryTask, "Telemetry", 2048, NULL, tskIDLE_PRIORITY + 2, HPTR(h_telem)),
      "Telemetry");
  CHK(xTaskCreate(vCommandTask, "Command", 2048, NULL, tskIDLE_PRIORITY + 2, HPTR(h_cmd)),
      "Command");
  CHK(xTaskCreate(vHealthMonitorTask, "HealthMon", 2048, NULL, tskIDLE_PRIORITY + 1,
                  HPTR(h_health)),
      "HealthMon");

  if (gps_ok)
  {
    extern void gps_task(void *pvParameters);
    CHK(xTaskCreate(gps_task, "GpsTask", 2048, NULL, tskIDLE_PRIORITY + 2, HPTR(h_gps)), "GpsTask");
  }

  CHK(xTaskCreate(vDeployMonitorTask, "DeployMon", 2048, NULL, tskIDLE_PRIORITY + 2, NULL),
      "DeployMon");

  printf("  payload_task_init...\r\n");
  fflush(stdout);
  payload_task_init();

#ifdef PICO_BUILD
  /* Link Health Monitor handle to Fault Manager for ISR-safe FDIR signaling */
  if (h_health != NULL)
  {
    fault_manager_set_hm_task_handle(h_health);
  }
#endif
#ifdef PICO_BUILD
  CHK(xTaskCreate(vLedBlinkTask, "LEDBlink", 2048, NULL, tskIDLE_PRIORITY + 2, &h_led), "LEDBlink");
  /* Heartbeat at LOW priority — it's just diagnostic, must not preempt Startup. */
  CHK(xTaskCreate(vHeartbeatTask, "Heartbeat", 2048, NULL, tskIDLE_PRIORITY + 1, &h_hb),
      "Heartbeat");
#endif

#undef HPTR

#undef CHK

  /* ── Confirm boot to bootloader (fsw_confirmed) ── */
  /* This writes to internal flash — only on real hardware. */
  boot_meta_set_fsw_confirmed();

  printf("[STARTUP] done — heap=%lu\r\n", (unsigned long)xPortGetFreeHeapSize());
  fflush(stdout);

  /* CDR-SW-05 (OI-7): StartupTask lifecycle.
   *
   * Instead of vTaskDelete(NULL) — which may have issues on the SMP kernel
   * with 1 core — lower our priority and turn this task into a slow alive
   * heartbeat.  A regular task is safer than the SMP task-deletion code path
   * (see Pico SDK FreeRTOS SMP port errata).  The 5 s interval and minimal
   * printf overhead make the CPU impact negligible (< 0.1 ‰).
   *
   * If a future dual-core SMP baseline is adopted, this task can be eliminated
   * and replaced with a dedicated Heartbeat task at priority +1.              */
  vTaskPrioritySet(NULL, tskIDLE_PRIORITY + 1);
  for (;;)
  {
    printf("[ALIVE] heap=%lu min_ever=%lu tick=%lu\r\n", (unsigned long)xPortGetFreeHeapSize(),
           (unsigned long)xPortGetMinimumEverFreeHeapSize(), (unsigned long)xTaskGetTickCount());
#ifdef PICO_BUILD
    /* HWM = remaining free words (high is good). used = 2048 - HWM. */
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
    printf("  HWM Payload     =%4lu  GpsTask     =%4lu\r\n",
           (unsigned long)uxTaskGetStackHighWaterMark(xTaskGetHandle("PayloadTask")),
           (unsigned long)uxTaskGetStackHighWaterMark(h_gps));
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

  #define BUILD_TIMESTAMP __DATE__ " " __TIME__

  printf("\r\n[BOOT] CubeSat OBC firmware started\r\n");
  printf("[BOOT] Build: %s\r\n", BUILD_TIMESTAMP);
  fflush(stdout);
#else
  printf("=== CubeSat OBC Firmware (Host Simulation) ===\n");
#endif

  /* Create ONE startup task — all subsystem init happens inside it after
   * the scheduler starts and SMP spinlocks are fully initialised.        */
  xTaskCreate(vStartupTask, "Startup", 2048, NULL, configMAX_PRIORITIES - 1, NULL);

  /* Enable hardware watchdog before starting any tasks.
   * Timeout: 8 000 ms — allows ~1.5 missed health-monitor ticks (5 s each)
   * before a forced reset.  Configured BEFORE vTaskStartScheduler so the
   * watchdog counts from boot, not from the first health-monitor feed.
   * pause_on_debug=false inside the HAL so single-core RP2350 WDT works. */
  watchdog_hal_init(8000);

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
