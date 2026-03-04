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

/* ── Task enable/disable flags for incremental debugging ────────────────────
 * Set to 0 to skip creating a task. Start with all 0, confirm LED + HB work,
 * then enable one at a time and reflash until the blocker is found.          */
#define ENABLE_COMM_INIT 1 /* CSPRouter task (pri 3) */
#define ENABLE_LED_BLINK 1 /* CYW43 SPI LED toggle   */
#define ENABLE_TASK_SENSOR_READ 1
#define ENABLE_TASK_ATTITUDE_CTRL 1
#define ENABLE_TASK_TELEMETRY 1
#define ENABLE_TASK_COMMAND 1
#define ENABLE_TASK_HEALTH_MON 1

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

/* ── LED blink task ───────────────────────────────────────────────────────────── */
static void vLedBlinkTask(void *pvParameters)
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

/* ── Heartbeat task — confirms scheduler running and prints free heap / HWMs ─ */
static TaskHandle_t h_sensor = NULL;
static TaskHandle_t h_attitude = NULL;
static TaskHandle_t h_telemetry = NULL;
static TaskHandle_t h_command = NULL;
static TaskHandle_t h_health = NULL;

static void vHeartbeatTask(void *pvParameters)
{
  (void)pvParameters;
  uint32_t tick = 0;
  for (;;)
  {
    /* Ultra-minimal heartbeat — no fflush, no HWM.
     * Pico SDK printf (LIB_PICO_PRINTF_PICO) already calls stdio_flush()
     * internally.  Calling fflush(stdout) from FreeRTOS tasks may conflict
     * with the USB low-priority IRQ and crash. */
    printf("[HB %lu] heap=%lu\r\n", (unsigned long)tick, (unsigned long)xPortGetFreeHeapSize());
    tick++;
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

#if ENABLE_COMM_INIT
  printf("  comm_init...\r\n");
  fflush(stdout);
  comm_init();
#else
  printf("  [SKIP] comm_init (disabled)\r\n");
  fflush(stdout);
#endif

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

/* Check every xTaskCreate — a silent pdFAIL here causes mysterious hangs. */
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

  /* Create lower-priority tasks first so that Heartbeat (highest pri) cannot
   * preempt the startup task before all tasks exist.
   * 2048 words (8 KB) per task: newlib printf with floats + EKF + CSP uses >4 KB. */
#if ENABLE_LED_BLINK
  CHK(xTaskCreate(vLedBlinkTask, "LEDBlink", 2048, NULL, tskIDLE_PRIORITY + 1, NULL), "LEDBlink");
#else
  printf("  [SKIP]  LEDBlink (disabled)\r\n");
  fflush(stdout);
#endif
  // CHK(xTaskCreate(vHeartbeatTask, "Heartbeat", 2048, NULL, configMAX_PRIORITIES - 2, NULL),
  // "Heartbeat");
  CHK(xTaskCreate(vHeartbeatTask, "Heartbeat", 2048, NULL, tskIDLE_PRIORITY + 1, NULL),
      "Heartbeat");

#if ENABLE_TASK_SENSOR_READ
  CHK(xTaskCreate(vSensorReadTask, "SensorRead", 2048, NULL, tskIDLE_PRIORITY + 3, &h_sensor),
      "SensorRead");
#else
  printf("  [SKIP]  SensorRead (disabled)\r\n");
  fflush(stdout);
#endif
#if ENABLE_TASK_ATTITUDE_CTRL
  CHK(xTaskCreate(vAttitudeControlTask, "AttitudeCtrl", 2048, NULL, tskIDLE_PRIORITY + 3,
                  &h_attitude),
      "AttitudeCtrl");
#else
  printf("  [SKIP]  AttitudeCtrl (disabled)\r\n");
  fflush(stdout);
#endif
#if ENABLE_TASK_TELEMETRY
  CHK(xTaskCreate(vTelemetryTask, "Telemetry", 2048, NULL, tskIDLE_PRIORITY + 2, &h_telemetry),
      "Telemetry");
#else
  printf("  [SKIP]  Telemetry (disabled)\r\n");
  fflush(stdout);
#endif
#if ENABLE_TASK_COMMAND
  CHK(xTaskCreate(vCommandTask, "Command", 2048, NULL, tskIDLE_PRIORITY + 2, &h_command),
      "Command");
#else
  printf("  [SKIP]  Command (disabled)\r\n");
  fflush(stdout);
#endif
#if ENABLE_TASK_HEALTH_MON
  CHK(xTaskCreate(vHealthMonitorTask, "HealthMon", 2048, NULL, tskIDLE_PRIORITY + 1, &h_health),
      "HealthMon");
#else
  printf("  [SKIP]  HealthMon (disabled)\r\n");
  fflush(stdout);
#endif

#undef CHK

  printf("[STARTUP] done — heap=%lu\r\n", (unsigned long)xPortGetFreeHeapSize());
  fflush(stdout);

  /* TEST 4: Stay at MAX priority (4). Call vTaskDelay → context switch
   * to Timer Svc (pri 3), then Idle (pri 0), then back to us.
   * If this crashes: PendSV / idle-task / SMP port is broken.
   * If ALIVE 0 prints but not ALIVE 1: crash is in Timer/Idle task.      */
  /* NO vTaskPrioritySet — remain at configMAX_PRIORITIES-1 = 4 */
  uint32_t alive_tick = 0;
  for (;;)
  {
    printf("[ALIVE %lu] tick=%lu\r\n", (unsigned long)alive_tick,
           (unsigned long)xTaskGetTickCount());
    alive_tick++;
    vTaskDelay(pdMS_TO_TICKS(2000));
  }
}

/* ── main ──────────────────────────────────────────────────────────────────── */
int main(void)
{
  stdio_init_all();
  setvbuf(stdout, NULL, _IONBF, 0); /* fully unbuffered — every write goes to USB immediately */
  /* Wait up to 3 s for USB CDC host (UART works immediately) */
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

  printf("\r\n===================================\r\n");
  printf("  Pico 2W -- Full OBC subsystem test\r\n");
  printf("===================================\r\n\r\n");
  fflush(stdout);

#if 1
  if (cyw43_arch_init())
  {
    printf("[WARN] cyw43_arch_init failed -- LED disabled\r\n");
    fflush(stdout);
  }
#endif

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
