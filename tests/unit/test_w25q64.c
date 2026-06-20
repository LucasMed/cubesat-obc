/**
 * @file test_w25q64.c
 * @brief Unit tests for W25Q64 flash driver
 */

#include "w25q64.h"
#include "drivers/imu/imu_calib.h"
#include "unity.h"
#include <string.h>

/* Host-only test helper declared extern (defined in w25q64.c host section) */
extern void w25q64_host_set_busy(bool busy);

void setUp(void)
{
}

void tearDown(void)
{
}

void test_w25q64_init(void)
{
    w25q64_status_t status = w25q64_init();
    TEST_ASSERT_EQUAL_INT(W25Q64_OK, status);
}

void test_w25q64_read_id(void)
{
    w25q64_id_t id;
    w25q64_status_t status = w25q64_read_id(&id);
    
    TEST_ASSERT_EQUAL_INT(W25Q64_OK, status);
    TEST_ASSERT_TRUE(id.valid);
    TEST_ASSERT_EQUAL_INT(0xEF, id.manufacturer_id);  /* Winbond */
    TEST_ASSERT_EQUAL_INT(0x40, id.memory_type);
    TEST_ASSERT_EQUAL_INT(0x17, id.capacity);  /* W25Q64 */
}

void test_w25q64_capacity(void)
{
    uint32_t capacity = w25q64_get_capacity();
    TEST_ASSERT_EQUAL_UINT32(8 * 1024 * 1024, capacity);  /* 8MB */
}

void test_w25q64_is_present(void)
{
    bool present = w25q64_is_present();
    TEST_ASSERT_TRUE(present);
}

void test_w25q64_read_write(void)
{
    /* Test reading - should succeed with dummy data in host mode */
    uint8_t read_buf[16];
    memset(read_buf, 0xAA, sizeof(read_buf));
    
    w25q64_status_t status = w25q64_read(0, read_buf, 16);
    TEST_ASSERT_EQUAL_INT(W25Q64_OK, status);
    
    /* Test write - should succeed in host mode */
    uint8_t write_data[16] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                               0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10};
    
    status = w25q64_write_page(0, write_data, 16);
    TEST_ASSERT_EQUAL_INT(W25Q64_OK, status);
}

void test_w25q64_erase(void)
{
    w25q64_status_t status;
    
    /* Test sector erase */
    status = w25q64_erase_sector(0);
    TEST_ASSERT_EQUAL_INT(W25Q64_OK, status);
    
    /* Test block erase 32KB */
    status = w25q64_erase_block32(0);
    TEST_ASSERT_EQUAL_INT(W25Q64_OK, status);
    
    /* Test block erase 64KB */
    status = w25q64_erase_block64(0);
    TEST_ASSERT_EQUAL_INT(W25Q64_OK, status);
}

void test_w25q64_erase_alignment(void)
{
    w25q64_status_t status;
    
    /* Sector erase must be 4KB-aligned - test with misaligned address */
    status = w25q64_erase_sector(100);  /* Not aligned */
    TEST_ASSERT_EQUAL_INT(W25Q64_ERR_ERASE, status);
    
    /* Block 32KB must be 32KB-aligned */
    status = w25q64_erase_block32(100);
    TEST_ASSERT_EQUAL_INT(W25Q64_ERR_ERASE, status);
    
    /* Block 64KB must be 64KB-aligned */
    status = w25q64_erase_block64(100);
    TEST_ASSERT_EQUAL_INT(W25Q64_ERR_ERASE, status);
}

void test_w25q64_status(void)
{
    uint8_t status = w25q64_read_status();
    /* In host mode, returns 0 (idle) */
    TEST_ASSERT_EQUAL_INT(0, status);
}

void test_w25q64_wait_ready(void)
{
    /* Host stub always succeeds */
    w25q64_status_t status = w25q64_wait_ready(100);
    TEST_ASSERT_EQUAL_INT(W25Q64_OK, status);
}

void test_w25q64_wait_ready_zero_timeout(void)
{
    /* Even with zero timeout, if device is idle it should succeed */
    w25q64_status_t status = w25q64_wait_ready(0);
    TEST_ASSERT_EQUAL_INT(W25Q64_OK, status);
}

void test_w25q64_wait_ready_timeout_expiry(void)
{
    /* Simulate busy device — wait_ready should time out */
    w25q64_host_set_busy(true);

    /* Very short timeout ensures timeout behaviour */
    w25q64_status_t status = w25q64_wait_ready(1);
    TEST_ASSERT_EQUAL_INT(W25Q64_ERR_TIMEOUT, status);

    /* Reset for subsequent tests */
    w25q64_host_set_busy(false);
}

void test_w25q64_wait_ready_success_after_busy_clears(void)
{
    /* Host stub can't simulate busy clearing mid-poll without threading,
     * but verify that wait_ready succeeds when device is idle */
    w25q64_host_set_busy(false);
    TEST_ASSERT_EQUAL_INT(W25Q64_OK, w25q64_wait_ready(100));
}

void test_w25q64_imu_calib_write_read(void)
{
    imu_calib_t cal_in;
    memset(&cal_in, 0, sizeof(cal_in));

    cal_in.accel_offset[0] = 0.05f;
    cal_in.accel_offset[1] = -0.02f;
    cal_in.accel_offset[2] = 0.10f;
    cal_in.accel_scale[0] = 1.0f;
    cal_in.accel_scale[1] = 1.01f;
    cal_in.accel_scale[2] = 0.98f;
    cal_in.gyro_offset_raw[0] = 12;
    cal_in.gyro_offset_raw[1] = -5;
    cal_in.gyro_offset_raw[2] = 3;
    cal_in.gyro_bias_rads[0] = 0.001f;
    cal_in.gyro_bias_rads[1] = -0.0005f;
    cal_in.gyro_bias_rads[2] = 0.002f;
    cal_in.calibrated = true;

    w25q64_status_t status = w25q64_write_imu_calib(&cal_in);
    TEST_ASSERT_EQUAL_INT(W25Q64_OK, status);

    imu_calib_t cal_out;
    memset(&cal_out, 0xFF, sizeof(cal_out));

    status = w25q64_read_imu_calib(&cal_out);
    TEST_ASSERT_EQUAL_INT(W25Q64_OK, status);

    /* Verify all fields survived the roundtrip (host backend uses memcpy, exact match) */
    TEST_ASSERT_TRUE(cal_in.accel_offset[0] == cal_out.accel_offset[0]);
    TEST_ASSERT_TRUE(cal_in.accel_offset[1] == cal_out.accel_offset[1]);
    TEST_ASSERT_TRUE(cal_in.accel_offset[2] == cal_out.accel_offset[2]);
    TEST_ASSERT_TRUE(cal_in.accel_scale[0] == cal_out.accel_scale[0]);
    TEST_ASSERT_TRUE(cal_in.accel_scale[1] == cal_out.accel_scale[1]);
    TEST_ASSERT_TRUE(cal_in.accel_scale[2] == cal_out.accel_scale[2]);
    TEST_ASSERT_EQUAL_INT(cal_in.gyro_offset_raw[0], cal_out.gyro_offset_raw[0]);
    TEST_ASSERT_EQUAL_INT(cal_in.gyro_offset_raw[1], cal_out.gyro_offset_raw[1]);
    TEST_ASSERT_EQUAL_INT(cal_in.gyro_offset_raw[2], cal_out.gyro_offset_raw[2]);
    TEST_ASSERT_TRUE(cal_in.gyro_bias_rads[0] == cal_out.gyro_bias_rads[0]);
    TEST_ASSERT_TRUE(cal_in.gyro_bias_rads[1] == cal_out.gyro_bias_rads[1]);
    TEST_ASSERT_TRUE(cal_in.gyro_bias_rads[2] == cal_out.gyro_bias_rads[2]);
    TEST_ASSERT_TRUE(cal_out.calibrated);
}

void test_w25q64_imu_calib_read_empty(void)
{
    /* Reading before any write should fail */
    imu_calib_t cal;
    memset(&cal, 0xFF, sizeof(cal));

    w25q64_status_t status = w25q64_read_imu_calib(&cal);
    TEST_ASSERT_EQUAL_INT(W25Q64_ERR_INIT, status);
}

void test_w25q64_imu_calib_null_params(void)
{
    w25q64_status_t status;

    status = w25q64_write_imu_calib(NULL);
    TEST_ASSERT_EQUAL_INT(W25Q64_ERR_INIT, status);

    status = w25q64_read_imu_calib(NULL);
    TEST_ASSERT_EQUAL_INT(W25Q64_ERR_INIT, status);
}

void test_w25q64_imu_calib_overwrite(void)
{
    /* Write twice, read back the latest */
    imu_calib_t cal_first;
    memset(&cal_first, 0, sizeof(cal_first));
    cal_first.accel_offset[0] = 1.0f;
    cal_first.calibrated = true;

    imu_calib_t cal_second;
    memset(&cal_second, 0, sizeof(cal_second));
    cal_second.accel_offset[0] = 2.0f;
    cal_second.calibrated = true;

    TEST_ASSERT_EQUAL_INT(W25Q64_OK, w25q64_write_imu_calib(&cal_first));
    TEST_ASSERT_EQUAL_INT(W25Q64_OK, w25q64_write_imu_calib(&cal_second));

    imu_calib_t cal_out;
    memset(&cal_out, 0, sizeof(cal_out));
    TEST_ASSERT_EQUAL_INT(W25Q64_OK, w25q64_read_imu_calib(&cal_out));

    /* Should read the second (latest) write */
    TEST_ASSERT_TRUE(2.0f == cal_out.accel_offset[0]);
}

void test_w25q64_erase_chip(void)
{
    w25q64_status_t st = w25q64_erase_chip();
    TEST_ASSERT_EQUAL_INT(W25Q64_OK, st);
}

int main(void)
{
    UNITY_BEGIN();
    
    RUN_TEST(test_w25q64_init);
    RUN_TEST(test_w25q64_read_id);
    RUN_TEST(test_w25q64_capacity);
    RUN_TEST(test_w25q64_is_present);
    RUN_TEST(test_w25q64_read_write);
    RUN_TEST(test_w25q64_erase);
    RUN_TEST(test_w25q64_erase_alignment);
    RUN_TEST(test_w25q64_status);
    RUN_TEST(test_w25q64_wait_ready);
    RUN_TEST(test_w25q64_wait_ready_zero_timeout);
    RUN_TEST(test_w25q64_wait_ready_timeout_expiry);
    RUN_TEST(test_w25q64_wait_ready_success_after_busy_clears);
    /* Read-empty and null-params must run BEFORE any write to verify uninitialized state */
    RUN_TEST(test_w25q64_erase_chip);
    RUN_TEST(test_w25q64_imu_calib_read_empty);
    RUN_TEST(test_w25q64_imu_calib_null_params);
    RUN_TEST(test_w25q64_imu_calib_write_read);
    RUN_TEST(test_w25q64_imu_calib_overwrite);

    return UNITY_END();
}
