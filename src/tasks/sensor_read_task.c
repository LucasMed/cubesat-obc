#include "config.h"
#include "sensor_read_task.h"
#include <stdio.h>
#include "FreeRTOS.h"
#include "task.h"

// Sensor read task: reads IMU at 10 Hz
void vSensorReadTask(void *pvParameters) {
    (void)pvParameters;
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(100); // 10 Hz

    printf("[sensor_read_task] Started\n");

    while (1) {
        // Read IMU (simulated)
        // In real code: call mpu6050_read(&roll, &pitch, &yaw)

        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}
