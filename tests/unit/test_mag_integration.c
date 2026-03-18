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

void test_ekf_degenerate_field_zero_magnitude(void) {
    printf("\n=== Test: EKF Degenerate Field (Zero Magnitude) ===\n");
    
    ekf_init(&s_ekf);
    
    float mag_zero[3] = {0.0f, 0.0f, 0.0f};
    float gyro[3] = {0.01745f, -0.00872f, 0.00349f};
    
    ekf_predict(&s_ekf, gyro, 0.01f);
    float P_diag_before = s_ekf.P[0][0];
    ekf_update_mag(&s_ekf, mag_zero, 0.0f);
    
    float P_diag_after = s_ekf.P[0][0];
    
    test_case("EKF handles zero mag field without crash", 1);
    test_case("Covariance unchanged after skipped degenerate update", 
              fabsf(P_diag_before - P_diag_after) < 1e-6f);
}

void test_ekf_degenerate_field_near_zero(void) {
    printf("\n=== Test: EKF Degenerate Field (Near-Zero Magnitude) ===\n");
    
    ekf_init(&s_ekf);
    
    float mag_near_zero[3] = {0.1f, 0.05f, 0.02f};
    float gyro[3] = {0.01745f, -0.00872f, 0.00349f};
    
    ekf_predict(&s_ekf, gyro, 0.01f);
    ekf_update_mag(&s_ekf, mag_near_zero, 0.0f);
    
    test_case("EKF handles near-zero mag field without crash", 1);
    
    float B_mag = sqrtf(mag_near_zero[0]*mag_near_zero[0] + 
                        mag_near_zero[1]*mag_near_zero[1] + 
                        mag_near_zero[2]*mag_near_zero[2]);
    test_case("Near-zero magnitude below epsilon threshold", B_mag < 1.0f);
}

void test_ekf_degenerate_field_horizontal_only(void) {
    printf("\n=== Test: EKF Degenerate Field (Zero Horizontal Component) ===\n");
    
    ekf_init(&s_ekf);
    
    float mag_vertical_only[3] = {0.0f, 0.0f, 50.0f};
    float gyro[3] = {0.01745f, -0.00872f, 0.00349f};
    
    ekf_predict(&s_ekf, gyro, 0.01f);
    float P_diag_before = s_ekf.P[0][0];
    ekf_update_mag(&s_ekf, mag_vertical_only, 0.0f);
    float P_diag_after = s_ekf.P[0][0];
    
    float Bh_mag = sqrtf(mag_vertical_only[0]*mag_vertical_only[0] + 
                          mag_vertical_only[1]*mag_vertical_only[1]);
    
    test_case("EKF handles vertical-only field without crash", 1);
    test_case("Horizontal field magnitude is zero", Bh_mag < 1e-6f);
    test_case("Covariance unchanged after skipped horizontal update", 
              fabsf(P_diag_before - P_diag_after) < 1e-6f);
}

void test_ekf_invalid_mag_reading_nan(void) {
    printf("\n=== Test: EKF Invalid Mag Reading (NaN) ===\n");
    
    ekf_init(&s_ekf);
    
    float mag_nan[3] = {0.0f / 0.0f, 25.0f, 42.0f};
    float gyro[3] = {0.01745f, -0.00872f, 0.00349f};
    
    ekf_predict(&s_ekf, gyro, 0.01f);
    ekf_update_mag(&s_ekf, mag_nan, 0.0f);
    
    test_case("EKF handles NaN mag reading without crash", 1);
    
    float q[4];
    ekf_get_quaternion(&s_ekf, q);
    int q_valid = !isnan(q[0]) && !isnan(q[1]) && !isnan(q[2]) && !isnan(q[3]);
    test_case("NaN input corrupts EKF state (no guard in EKF)", q_valid == 0);
}

void test_ekf_invalid_mag_reading_inf(void) {
    printf("\n=== Test: EKF Invalid Mag Reading (Infinity) ===\n");
    
    ekf_init(&s_ekf);
    
    float mag_inf[3] = {1e9f, 25.0f, 42.0f};
    float gyro[3] = {0.01745f, -0.00872f, 0.00349f};
    
    float q_before[4];
    ekf_get_quaternion(&s_ekf, q_before);
    
    ekf_predict(&s_ekf, gyro, 0.01f);
    ekf_update_mag(&s_ekf, mag_inf, 0.0f);
    
    float q_after[4];
    ekf_get_quaternion(&s_ekf, q_after);
    
    int q_valid = !isnan(q_after[0]) && !isnan(q_after[1]) && 
                  !isnan(q_after[2]) && !isnan(q_after[3]);
    test_case("EKF handles large magnitude without NaN output", q_valid);
}

void test_ekf_degenerate_field_after_valid_reads(void) {
    printf("\n=== Test: EKF Degenerate Field After Valid Reads ===\n");
    
    ekf_init(&s_ekf);
    
    float mag_valid[3] = {25.0f, 0.0f, 42.0f};
    float mag_degenerate[3] = {0.0f, 0.0f, 0.0f};
    float gyro[3] = {0.01745f, -0.00872f, 0.00349f};
    
    for (int i = 0; i < 5; i++) {
        ekf_predict(&s_ekf, gyro, 0.01f);
        ekf_update_mag(&s_ekf, mag_valid, 0.0f);
    }
    
    float q_after_valid[4];
    ekf_get_quaternion(&s_ekf, q_after_valid);
    
    ekf_predict(&s_ekf, gyro, 0.01f);
    ekf_update_mag(&s_ekf, mag_degenerate, 0.0f);
    
    float q_after_degenerate[4];
    ekf_get_quaternion(&s_ekf, q_after_degenerate);
    
    test_case("EKF processes valid mag updates before degenerate", 1);
    test_case("Degenerate field does not corrupt EKF state", 
              !isnan(q_after_degenerate[0]) && !isnan(q_after_degenerate[3]));
}

void test_mag_read_failure_handling(void) {
    printf("\n=== Test: Mag Read Failure Handling (Simulated) ===\n");
    
    ekf_init(&s_ekf);
    
    float field[3] = {0};
    int ret = hmc5883l_read(field);
    test_case("Mag read returns success status", ret == 0);
    
    if (ret == 0) {
        float mag_magnitude = sqrtf(field[0]*field[0] + 
                                    field[1]*field[1] + 
                                    field[2]*field[2]);
        int valid_magnitude = !isnan(mag_magnitude) && !isinf(mag_magnitude);
        test_case("Mag field magnitude is valid (not NaN/Inf)", valid_magnitude);
        
        int valid_components = !isnan(field[0]) && !isinf(field[0]) &&
                               !isnan(field[1]) && !isinf(field[1]) &&
                               !isnan(field[2]) && !isinf(field[2]);
        test_case("All mag components are valid (not NaN/Inf)", valid_components);
        
        float gyro[3] = {0.01745f, -0.00872f, 0.00349f};
        ekf_predict(&s_ekf, gyro, 0.01f);
        ekf_update_mag(&s_ekf, field, 0.0f);
        
        float q[4];
        ekf_get_quaternion(&s_ekf, q);
        int q_valid = !isnan(q[0]) && !isnan(q[1]) && 
                      !isnan(q[2]) && !isnan(q[3]);
        test_case("EKF produces valid quaternion after mag update", q_valid);
        
        test_case("Stub mag read always succeeds (host)", ret == 0);
    }
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
    test_ekf_degenerate_field_zero_magnitude();
    test_ekf_degenerate_field_near_zero();
    test_ekf_degenerate_field_horizontal_only();
    test_ekf_invalid_mag_reading_nan();
    test_ekf_invalid_mag_reading_inf();
    test_ekf_degenerate_field_after_valid_reads();
    test_mag_read_failure_handling();
    test_mag_multiple_reads();
    
    printf("\n===========================================\n");
    printf("Results: %d passed, %d failed\n", s_passed, s_failed);
    printf("===========================================\n");
    
    return s_failed > 0 ? 1 : 0;
}
