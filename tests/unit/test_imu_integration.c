// test_imu_integration.c -- Integration test for IMU driver with data layer
// Tests: IMU data -> data layer -> system state

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "data_layer.h"
#include "system_state.h"

// Mock IMU for testing
static float mock_accel[3] = {0.0f, 0.0f, 1.0f};
static float mock_gyro[3] = {0.1f, -0.2f, 0.05f};
static int mock_init_called = 0;
static int mock_read_called = 0;

// Mock implementations
int mpu6050_init(void) {
    mock_init_called++;
    return 0;
}

int mpu6050_read_raw(float accel[3], float gyro[3]) {
    mock_read_called++;
    if (accel) {
        accel[0] = mock_accel[0];
        accel[1] = mock_accel[1];
        accel[2] = mock_accel[2];
    }
    if (gyro) {
        gyro[0] = mock_gyro[0];
        gyro[1] = mock_gyro[1];
        gyro[2] = mock_gyro[2];
    }
    return 0;
}

int mpu6050_read(float *roll, float *pitch, float *yaw) {
    (void)roll;
    (void)pitch;
    (void)yaw;
    return mpu6050_read_raw(NULL, NULL);
}

float temperature_read(void) {
    return 25.0f;
}

static int s_passed = 0;
static int s_failed = 0;

void test_case(const char *name, int condition) {
    if (condition) {
        printf("  [PASS] %s\n", name);
        s_passed++;
    } else {
        printf("  [FAIL] %s\n", name);
        s_failed++;
    }
}

void test_imu_init(void) {
    printf("\n=== Test: IMU Init ===\n");
    mock_init_called = 0;
    int ret = mpu6050_init();
    test_case("IMU init returns 0", ret == 0);
    test_case("IMU init was called", mock_init_called == 1);
}

void test_imu_read_raw(void) {
    printf("\n=== Test: IMU Read Raw ===\n");
    float accel[3] = {0}, gyro[3] = {0};
    int ret = mpu6050_read_raw(accel, gyro);
    test_case("IMU read returns 0", ret == 0);
    test_case("Accel X not zero", accel[0] != 0.0f || accel[1] != 0.0f || accel[2] != 0.0f);
    test_case("Gyro values in reasonable range", 
              fabsf(gyro[0]) < 1.0f && fabsf(gyro[1]) < 1.0f && fabsf(gyro[2]) < 1.0f);
}

void test_data_layer_imu_write(void) {
    printf("\n=== Test: Data Layer IMU Write ===\n");
    
    float att_rad[3] = {0.1f, -0.05f, 0.02f};
    float rates_rad[3] = {0.01f, -0.02f, 0.005f};
    
    data_layer_write_imu(att_rad, rates_rad);
    
    system_state_t state = {0};
    system_state_get(&state);
    
    test_case("IMU valid flag set", state.imu_valid == true);
    test_case("Attitude not zero", fabsf(state.attitude[0]) > 0.0f || fabsf(state.attitude[1]) > 0.0f);
}

void test_imu_data_consistency(void) {
    printf("\n=== Test: IMU Data Consistency ===\n");
    
    mock_accel[0] = 0.0f;
    mock_accel[1] = 0.0f;
    mock_accel[2] = 1.0f;
    mock_gyro[0] = 0.0f;
    mock_gyro[1] = 0.0f;
    mock_gyro[2] = 0.0f;
    
    float accel[3] = {0}, gyro[3] = {0};
    mpu6050_read_raw(accel, gyro);
    
    test_case("Accel Z is 1g (flat)", fabsf(accel[2] - 1.0f) < 0.01f);
    test_case("Gyro all zero (no rotation)", gyro[0] == 0.0f && gyro[1] == 0.0f && gyro[2] == 0.0f);
    
    mock_gyro[0] = 10.0f;
    mock_gyro[1] = -5.0f;
    mock_gyro[2] = 2.0f;
    
    mpu6050_read_raw(accel, gyro);
    
    test_case("Gyro X reflects roll rate", fabsf(gyro[0] - 10.0f) < 0.1f);
    test_case("Gyro Y reflects pitch rate", fabsf(gyro[1] - (-5.0f)) < 0.1f);
    test_case("Gyro Z reflects yaw rate", fabsf(gyro[2] - 2.0f) < 0.1f);
}

void test_imu_multiple_reads(void) {
    printf("\n=== Test: IMU Multiple Reads ===\n");
    
    mock_read_called = 0;
    
    for (int i = 0; i < 5; i++) {
        float accel[3], gyro[3];
        mpu6050_read_raw(accel, gyro);
    }
    
    test_case("Multiple reads count correct", mock_read_called == 5);
}

void test_sensor_availability(void) {
    printf("\n=== Test: Sensor Availability ===\n");
    
    data_layer_set_sensor_avail(true, true);
    
    system_state_t state = {0};
    system_state_get(&state);
    
    test_case("IMU available flag set", state.imu_available == true);
    test_case("Temp sensor available flag set", state.temp_available == true);
}

int main(void) {
    printf("===========================================\n");
    printf("IMU Integration Test Suite\n");
    printf("===========================================\n");
    
    system_state_init();
    
    test_imu_init();
    test_imu_read_raw();
    test_data_layer_imu_write();
    test_imu_data_consistency();
    test_imu_multiple_reads();
    test_sensor_availability();
    
    printf("\n===========================================\n");
    printf("Results: %d passed, %d failed\n", s_passed, s_failed);
    printf("===========================================\n");
    
    return s_failed > 0 ? 1 : 0;
}
