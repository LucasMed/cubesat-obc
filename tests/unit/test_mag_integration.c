// test_mag_integration.c -- Integration test for magnetometer driver with EKF
// Tests: Magnetometer data -> EKF -> attitude estimation

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "data_layer.h"
#include "system_state.h"
#include "hmc5883l.h"
#include "ekf.h"

static ekf_t s_ekf;
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

void test_mag_init(void) {
    printf("\n=== Test: Magnetometer Init ===\n");
    int ret = hmc5883l_init();
    test_case("Mag init returns 0", ret == 0);
}

void test_mag_read(void) {
    printf("\n=== Test: Magnetometer Read ===\n");
    float field[3] = {0};
    int ret = hmc5883l_read(field);
    test_case("Mag read returns 0", ret == 0);
    test_case("Mag field not all zero", field[0] != 0.0f || field[1] != 0.0f || field[2] != 0.0f);
    test_case("Mag field reasonable magnitude", 
              sqrtf(field[0]*field[0] + field[1]*field[1] + field[2]*field[2]) > 20.0f &&
              sqrtf(field[0]*field[0] + field[1]*field[1] + field[2]*field[2]) < 70.0f);
}

void test_ekf_init(void) {
    printf("\n=== Test: EKF Init ===\n");
    ekf_init(&s_ekf);
    test_case("EKF initialized without crash", 1);
}

void test_ekf_update_mag(void) {
    printf("\n=== Test: EKF Update with Mag ===\n");
    
    float mag[3] = {0};
    hmc5883l_read(mag);
    
    float gyro[3] = {0.01745f, -0.00872f, 0.00349f};  // 1, -0.5, 0.2 deg/s in rad/s
    
    ekf_predict(&s_ekf, gyro, 0.01f);
    
    ekf_update_mag(&s_ekf, mag, 0.0f);
    
    test_case("EKF update with mag completed", 1);
}

void test_ekf_attitude_output(void) {
    printf("\n=== Test: EKF Attitude Output ===\n");
    
    float q[4] = {0};
    ekf_get_quaternion(&s_ekf, q);
    
    float mag = sqrtf(q[0]*q[0] + q[1]*q[1] + q[2]*q[2] + q[3]*q[3]);
    test_case("Quaternion normalized", fabsf(mag - 1.0f) < 0.01f);
    
    test_case("Quaternion not all zero", q[0] != 0.0f || q[1] != 0.0f || 
                                       q[2] != 0.0f || q[3] != 0.0f);
}

void test_data_layer_ekf_write(void) {
    printf("\n=== Test: Data Layer EKF Write ===\n");
    
    ekf_init(&s_ekf);
    
    float mag[3] = {0};
    hmc5883l_read(mag);
    
    float gyro[3] = {0.01745f, -0.00872f, 0.00349f};
    
    for (int i = 0; i < 10; i++) {
        ekf_predict(&s_ekf, gyro, 0.01f);
        ekf_update_mag(&s_ekf, mag, 0.0f);
    }
    
    float q[4], bias[3], cov[7];
    ekf_get_quaternion(&s_ekf, q);
    ekf_get_bias(&s_ekf, bias);
    for (int i = 0; i < 7; i++) {
        cov[i] = s_ekf.P[i][i];
    }
    
    data_layer_write_ekf(q, bias, cov);
    
    system_state_t state = {0};
    system_state_get(&state);
    
    test_case("EKF valid flag set", state.imu_ekf_valid == true);
}

void test_mag_multiple_reads(void) {
    printf("\n=== Test: Mag Multiple Reads ===\n");
    
    float fields[3][3];
    for (int i = 0; i < 3; i++) {
        hmc5883l_read(fields[i]);
    }
    
    test_case("Multiple reads consistent", 
              fabsf(fields[0][0] - fields[1][0]) < 0.1f &&
              fabsf(fields[1][0] - fields[2][0]) < 0.1f);
}

int main(void) {
    printf("===========================================\n");
    printf("Magnetometer Integration Test Suite\n");
    printf("===========================================\n");
    
    system_state_init();
    data_layer_init();
    
    test_mag_init();
    test_mag_read();
    test_ekf_init();
    test_ekf_update_mag();
    test_ekf_attitude_output();
    test_data_layer_ekf_write();
    test_mag_multiple_reads();
    
    printf("\n===========================================\n");
    printf("Results: %d passed, %d failed\n", s_passed, s_failed);
    printf("===========================================\n");
    
    return s_failed > 0 ? 1 : 0;
}
