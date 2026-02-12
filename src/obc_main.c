#include "config.h"
#include "sensor_read_task.h"
#include "attitude_control_task.h"
#include "telemetry_task.h"
#include "health_monitor_task.h"
#include <stdio.h>
#include "FreeRTOS.h"
#include "task.h"

int main(void) {
    printf("=== CubeSat OBC Firmware ===\n");
    printf("Initializing FreeRTOS...\n");

    // Create sensor read task (HIGH priority, 10 Hz)
    xTaskCreate(
        vSensorReadTask,
        "SensorRead",
        512,
        NULL,
        4,  // High priority
        NULL
    );

    // Create attitude control task (HIGH priority, 20 Hz)
    xTaskCreate(
        vAttitudeControlTask,
        "AttitudeControl",
        512,
        NULL,
        4,  // High priority
        NULL
    );

    // Create telemetry task (MEDIUM priority, 1 Hz)
    xTaskCreate(
        vTelemetryTask,
        "Telemetry",
        512,
        NULL,
        3,  // Medium priority
        NULL
    );

    // Create health monitor task (LOW priority, ~0.2 Hz)
    xTaskCreate(
        vHealthMonitorTask,
        "HealthMonitor",
        512,
        NULL,
        2,  // Low priority
        NULL
    );

    printf("Starting FreeRTOS scheduler...\n");
    vTaskStartScheduler();

    // Should never reach here
    printf("ERROR: FreeRTOS scheduler exited!\n");
    while (1);

    return 0;
}
