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
#include "system_state.h"
#include "drivers/i2c_interface.h"
#include "drivers/imu/mpu6050.h"
#include "drivers/temperature.h"

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

    // Wait for USB connection (optional but helpful for Putty/Minicom)
    // Giving 2 seconds for the host to detect the device
    for (int i = 0; i < 20; i++) {
        printf(".");
        sleep_ms(100);
    }
    printf("\nUSB Connected / Startup delay finished.\n");
#else
    printf("=== CubeSat OBC Firmware (Host Simulation) ===\n");
#endif

    // Initialize System State
    printf("Initializing system state...\n");
    fflush(stdout);
    system_state_init();

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
    fflush(stdout);
    sleep_ms(100); // Small pause to let serial buffers clear

    vTaskStartScheduler();

    // Should never reach here
    printf("ERROR: FreeRTOS scheduler exited!\n");
    while (1);

    return 0;
}

#ifdef PICO_BUILD
// FreeRTOS hooks moved to freertos_hooks.c
#endif
