#include <stdio.h>
#include <assert.h>
#include "sensor_read_task.h"
#include "attitude_control_task.h"
#include "telemetry_task.h"
#include "health_monitor_task.h"
#include "system_state.h"
#include <csp/csp.h>

void test_system_integration(void) {
    printf("Running test_system_integration...\n");
    
    // 1. Initialize State
    system_state_init();
    csp_init();
    
    // 2. Initial state check
    system_state_t state;
    system_state_get(&state);
    assert(state.imu_valid == false);
    assert(state.temp_valid == false);

    // 3. Run sensor read step (uses host i2c/temp mocks)
    vSensorReadTask_Step();
    
    // 4. Verify state updated
    system_state_get(&state);
    assert(state.imu_valid == true);
    assert(state.temp_valid == true);
    // Based on host mocks: ID 0x68 means mock IMU is "working"
    // Mock temp is 25.0f
    assert(state.temp == 25.0f);

    // 5. Run control and telemetry steps
    vAttitudeControlTask_Step();
    vTelemetryTask_Step();
    vHealthMonitorTask_Step();

    printf("test_system_integration passed\n");
}

int main(void) {
    printf("=== OBC Tasks Unit Tests ===\n");
    
    test_system_integration();
    
    printf("All tasks tests passed!\n");
    return 0;
}
