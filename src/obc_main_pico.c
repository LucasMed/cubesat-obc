/**
 * @file obc_main_pico.c
 * @brief CubeSat OBC Main Entry Point (Pico SDK with Real FreeRTOS)
 *
 * This is the Pico 2W / Pico SDK version with real FreeRTOS kernel.
 * Uses:
 * - CYW43 for WiFi (initialized via cyw43_arch)
 * - Real FreeRTOS kernel and tasks
 * - I2C drivers (future Phase 2.3)
 */

#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"

// FreeRTOS headers
#include "FreeRTOS.h"
#include "task.h"

// ============================================================================
// Task Declarations
// ============================================================================

// Task stubs (will be replaced with real implementations)
void vSensorReadTask(void *pvParameters) {
    printf("Sensor Read Task Started\n");
    for (;;) {
        printf("SensorRead: IMU polling...\n");
        vTaskDelay(pdMS_TO_TICKS(100));  // 10 Hz
    }
}

void vAttitudeControlTask(void *pvParameters) {
    printf("Attitude Control Task Started\n");
    for (;;) {
        printf("AttitudeCtrl: Computing control law...\n");
        vTaskDelay(pdMS_TO_TICKS(50));   // 20 Hz
    }
}

void vTelemetryTask(void *pvParameters) {
    printf("Telemetry Task Started\n");
    for (;;) {
        printf("Telemetry: Transmitting data...\n");
        vTaskDelay(pdMS_TO_TICKS(1000)); // 1 Hz
    }
}

void vHealthMonitorTask(void *pvParameters) {
    printf("Health Monitor Task Started\n");
    for (;;) {
        printf("HealthMon: Checking status...\n");
        vTaskDelay(pdMS_TO_TICKS(5000)); // 0.2 Hz (every 5 sec)
    }
}

// ============================================================================
// LED Blink Helper (for diagnostics)
// ============================================================================

void vLedBlinkTask(void *pvParameters) {
    // Slow blink to show system is alive
    for (;;) {
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
        vTaskDelay(pdMS_TO_TICKS(200));
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
        vTaskDelay(pdMS_TO_TICKS(800));  // Fast blink = 200ms on, 800ms off
    }
}

// ============================================================================
// Main Entry Point
// ============================================================================

int main(void) {
    stdio_init_all();
    
    printf("\n");
    printf("=====================================\n");
    printf("  CubeSat OBC - Pico 2W Firmware\n");
    printf("  FreeRTOS Real Kernel (Phase 2.2)\n");
    printf("=====================================\n\n");
    
    // Initialize CYW43 (required for LED on Pico 2W)
    printf("Initializing CYW43 (WiFi chip)...\n");
    if (cyw43_arch_init()) {
        printf("ERROR: CYW43 initialization failed!\n");
        return -1;
    }
    printf("✓ CYW43 initialized\n\n");
    
    // Create FreeRTOS tasks
    printf("Creating FreeRTOS tasks...\n");
    
    // LED blink task (diagnostic)
    xTaskCreate(
        vLedBlinkTask,
        "LEDBlink",
        256,  // Stack size (words, typically 1 word = 4 bytes on ARM)
        NULL,
        tskIDLE_PRIORITY + 2,
        NULL
    );
    printf("✓ LED Blink task created\n");
    
    // Sensor read task (HIGH priority)
    xTaskCreate(
        vSensorReadTask,
        "SensorRead",
        512,
        NULL,
        tskIDLE_PRIORITY + 4,  // HIGH priority (4)
        NULL
    );
    printf("✓ Sensor Read task created\n");
    
    // Attitude control task (HIGH priority)
    xTaskCreate(
        vAttitudeControlTask,
        "AttitudeCtrl",
        512,
        NULL,
        tskIDLE_PRIORITY + 4,  // HIGH priority (4)
        NULL
    );
    printf("✓ Attitude Control task created\n");
    
    // Telemetry task (MEDIUM priority)
    xTaskCreate(
        vTelemetryTask,
        "Telemetry",
        512,
        NULL,
        tskIDLE_PRIORITY + 3,  // MEDIUM priority (3)
        NULL
    );
    printf("✓ Telemetry task created\n");
    
    // Health monitor task (LOW priority)
    xTaskCreate(
        vHealthMonitorTask,
        "HealthMon",
        512,
        NULL,
        tskIDLE_PRIORITY + 2,  // LOW priority (2)
        NULL
    );
    printf("✓ Health Monitor task created\n");
    
    printf("\nStarting FreeRTOS scheduler...\n");
    printf("LED will blink to indicate system is running.\n");
    printf("Task output will appear below:\n");
    printf("-------------------------------------\n\n");
    
    // Start the real FreeRTOS scheduler
    // (This call should never return)
    vTaskStartScheduler();
    
    // If we reach here, FreeRTOS ran out of heap memory
    printf("\nERROR: FreeRTOS scheduler exited!\n");
    printf("This usually means insufficient heap memory.\n");
    
    // Deinit CYW43
    cyw43_arch_deinit();
    
    return -1;
}

// ============================================================================
// FreeRTOS Required Hooks
// ============================================================================

/**
 * @brief vApplicationTickHook - Called once per tick from scheduler
 * Can be used for timing diagnostics
 */
void vApplicationTickHook(void) {
    // Optional: Track timing statistics
    // This is called very frequently (1000x per second at 1000 Hz tick rate)
}

/**
 * @brief vApplicationIdleHook - Called when all tasks are idle
 * Good place for low-power sleep if desired
 */
void vApplicationIdleHook(void) {
    // Optional: Could put MCU to sleep here
    // For now, just let it idle
}

/**
 * @brief vApplicationMallocFailedHook - Called if malloc fails
 * FreeRTOS malloc is static, so this shouldn't happen in normal use
 */
void vApplicationMallocFailedHook(void) {
    printf("FATAL: FreeRTOS malloc failed (no heap memory)!\n");
    for (;;) {
        tight_loop_contents();
    }
}

/**
 * @brief vApplicationStackOverflowHook - Called if a task stack overflows
 * Typically indicates a task was allocated too little stack
 */
void vApplicationStackOverflowHook(TaskHandle_t pxTask, char *pcTaskName) {
    printf("FATAL: Stack overflow in task '%s'!\n", pcTaskName);
    for (;;) {
        tight_loop_contents();
    }
}
