/**
 * @file obc_main.c
 * @brief CubeSat OBC Main Entry Point
 * 
 * Supports both host simulation and Pico 2W hardware builds.
 */

#include <stdio.h>
#include "FreeRTOS.h"
#include "task.h"

// Project headers
#include "config.h"
#include "sensor_read_task.h"
#include "attitude_control_task.h"
#include "telemetry_task.h"
#include "health_monitor_task.h"

#ifdef PICO_BUILD
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"

// LED Blink Helper (for diagnostics on hardware)
void vLedBlinkTask(void *pvParameters) {
    for (;;) {
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
        vTaskDelay(pdMS_TO_TICKS(200));
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
        vTaskDelay(pdMS_TO_TICKS(800));
    }
}
#endif

int main(void) {
#ifdef PICO_BUILD
    stdio_init_all();
    
    printf("\n=====================================\n");
    printf("  CubeSat OBC - Pico 2W Firmware\n");
    printf("  FreeRTOS Real Kernel\n");
    printf("=====================================\n\n");
    
    if (cyw43_arch_init()) {
        printf("ERROR: CYW43 initialization failed!\n");
        return -1;
    }
#else
    printf("=== CubeSat OBC Firmware (Host Simulation) ===\n");
#endif

    printf("Creating FreeRTOS tasks...\n");

#ifdef PICO_BUILD
    // LED blink task (diagnostic on hardware)
    xTaskCreate(
        vLedBlinkTask,
        "LEDBlink",
        256,
        NULL,
        tskIDLE_PRIORITY + 2,
        NULL
    );
#endif

    // OBC Functional Tasks
    xTaskCreate(vSensorReadTask, "SensorRead", 512, NULL, tskIDLE_PRIORITY + 4, NULL);
    xTaskCreate(vAttitudeControlTask, "AttitudeControl", 512, NULL, tskIDLE_PRIORITY + 4, NULL);
    xTaskCreate(vTelemetryTask, "Telemetry", 512, NULL, tskIDLE_PRIORITY + 3, NULL);
    xTaskCreate(vHealthMonitorTask, "HealthMonitor", 512, NULL, tskIDLE_PRIORITY + 2, NULL);

    printf("Starting FreeRTOS scheduler...\n");
    vTaskStartScheduler();

    // Should never reach here
    printf("ERROR: FreeRTOS scheduler exited!\n");
    while (1);

    return 0;
}

#ifdef PICO_BUILD
// FreeRTOS hooks moved to freertos_hooks.c
#endif
