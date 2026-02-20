#include <stdio.h>
#include <assert.h>
#include "sensor_read_task.h"
#include "attitude_control_task.h"
#include "telemetry_task.h"
#include "health_monitor_task.h"

void test_sensor_read_step(void) {
    printf("Running test_sensor_read_step...\n");
    // Call the step function
    vSensorReadTask_Step();
    // For now, it just doesn't crash.
    // In the future, we can check system_state updates.
    printf("test_sensor_read_step passed\n");
}

void test_attitude_control_step(void) {
    printf("Running test_attitude_control_step...\n");
    vAttitudeControlTask_Step();
    printf("test_attitude_control_step passed\n");
}

void test_telemetry_step(void) {
    printf("Running test_telemetry_step...\n");
    vTelemetryTask_Step();
    printf("test_telemetry_step passed\n");
}

void test_health_monitor_step(void) {
    printf("Running test_health_monitor_step...\n");
    vHealthMonitorTask_Step();
    printf("test_health_monitor_step passed\n");
}

int main(void) {
    printf("=== OBC Tasks Unit Tests ===\n");
    
    test_sensor_read_step();
    test_attitude_control_step();
    test_telemetry_step();
    test_health_monitor_step();
    
    printf("All tasks tests passed!\n");
    return 0;
}
