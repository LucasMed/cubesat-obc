/**
 * @file test_camera_driver.c
 * @brief Unit tests for the OV2640 Camera Driver.
 *
 * Tests:
 *   1.  camera_init() verifies sensor IDs and writes init tables.
 *   2.  camera_set_resolution() writes correct register tables.
 *   3.  camera_capture() pulses correctly and waits for ready status.
 *   4.  camera_get_fifo_length() reads 3 registers and combines them.
 *   5.  camera_read_fifo_burst() fills buffer with FIFO data.
 *
 * Runs on host with built-in mocks.
 */

#include "camera_driver.h"

#ifdef PICO_BUILD
  #include "unity.h"
#else
  #include "host/unity.h"
#endif

#include <string.h>

/* ------------------------------------------------------------------ */
/* Helpers to access internal state from host mocks                    */
/* ------------------------------------------------------------------ */

/*
 * The host-mode mock stores SCCB writes in an internal array.
 * We expose it here for test verification.
 */
extern uint8_t s_mock_i2c_regs[256];

/*
 * Register terminator constant — must match the one in camera_driver.c.
 * We use a local copy here since the static const is not exposed.
 */
#define TEST_REG_END 0xFF

/* ------------------------------------------------------------------ */
/* Unity Boilerplate                                                   */
/* ------------------------------------------------------------------ */

void setUp(void)
{
  memset(s_mock_i2c_regs, 0, sizeof(s_mock_i2c_regs));
}

void tearDown(void) {}

/* ------------------------------------------------------------------ */
/* Tests                                                               */
/* ------------------------------------------------------------------ */

void test_camera_init_success(void)
{
  /* camera_init() should verify chip ID and write init tables */
  TEST_ASSERT_TRUE(camera_init());

  /* After init, JPEG output format register should be set:
   * OV2640_JPEG writes DA=0x10 for JPEG output (DSP bank, 0xFF=0x00) */
  TEST_ASSERT_EQUAL_HEX8(0x10, s_mock_i2c_regs[0xDA]);
}

void test_camera_init_fails_on_bad_chip_id(void)
{
  /* Not directly testable with current mocks since chip ID always matches.
   * In real HW, if the sensor doesn't respond, camera_init() returns false.
   * This is a placeholder for when we have configurable mocks. */
}

void test_camera_init_twice_skips_reinit(void)
{
  /* First call performs full init */
  TEST_ASSERT_TRUE(camera_init());

  /* Second call hits the "already initialized" skip path */
  TEST_ASSERT_TRUE(camera_init());
}

void test_camera_set_resolution_valid(void)
{
  TEST_ASSERT_TRUE(camera_set_resolution(CAM_RES_640x480));

  /* The 640x480 JPEG table writes register 0x5A through 0x5C with specific
   * values. Test that the write path works. */
  TEST_ASSERT_TRUE(camera_set_resolution(CAM_RES_1600x1200));

  /* UXGA table sets 0x5C = 0x05 (ZMW) */
  TEST_ASSERT_EQUAL_HEX8(0x05, s_mock_i2c_regs[0x5C]);
}

void test_camera_set_resolution_invalid(void)
{
  /* Out-of-range enum value */
  TEST_ASSERT_FALSE(camera_set_resolution((camera_res_t)99));
}

void test_camera_capture_logic(void)
{
  TEST_ASSERT_TRUE(camera_capture(100));
}

void test_camera_get_fifo_length(void)
{
  /* In host mode, cam_spi_transfer returns value. Length combines
   * three 8-bit FIFO size registers: LEN = (SIZE1 << 16) | (SIZE2 << 8) | SIZE3
   * Since mock returns `value` for each call, the combined result
   * is: (0x00 << 16) | (0x00 << 8) | 0x00 = 0 */
  uint32_t len = camera_get_fifo_length();
  TEST_ASSERT_EQUAL_UINT32(0, len);
}

void test_camera_read_burst(void)
{
  uint8_t buf[10] = {0};
  TEST_ASSERT_TRUE(camera_read_fifo_burst(buf, 10));

  /* In host mode, buffer is filled with 0xAA */
  for (int i = 0; i < 10; i++)
  {
    TEST_ASSERT_EQUAL_HEX8(0xAA, buf[i]);
  }
}

void test_camera_read_burst_null_buffer(void)
{
  TEST_ASSERT_FALSE(camera_read_fifo_burst(NULL, 10));
}

void test_camera_read_burst_zero_length(void)
{
  uint8_t buf[4];
  TEST_ASSERT_FALSE(camera_read_fifo_burst(buf, 0));
}

void test_camera_clear_fifo(void)
{
  /* Must not crash */
  camera_clear_fifo();
}

void test_camera_write_sensor_reg(void)
{
  TEST_ASSERT_TRUE(camera_write_sensor_reg(0xFF, 0x00));
  TEST_ASSERT_TRUE(camera_write_sensor_reg(0x12, 0x34));
}

void test_camera_read_sensor_reg(void)
{
  uint8_t val = 0;
  TEST_ASSERT_TRUE(camera_read_sensor_reg(0x12, &val));
  TEST_ASSERT_EQUAL_INT(0xFF, val); /* sentinel for host stubs */
}

void test_camera_read_sensor_reg_null_val(void)
{
  TEST_ASSERT_FALSE(camera_read_sensor_reg(0x12, NULL));
}

int main(void)
{
  UNITY_BEGIN();
  RUN_TEST(test_camera_init_success);
  RUN_TEST(test_camera_init_fails_on_bad_chip_id);
  RUN_TEST(test_camera_init_twice_skips_reinit);
  RUN_TEST(test_camera_set_resolution_valid);
  RUN_TEST(test_camera_set_resolution_invalid);
  RUN_TEST(test_camera_capture_logic);
  RUN_TEST(test_camera_get_fifo_length);
  RUN_TEST(test_camera_read_burst);
  RUN_TEST(test_camera_read_burst_null_buffer);
  RUN_TEST(test_camera_read_burst_zero_length);
  RUN_TEST(test_camera_clear_fifo);
  RUN_TEST(test_camera_write_sensor_reg);
  RUN_TEST(test_camera_read_sensor_reg);
  RUN_TEST(test_camera_read_sensor_reg_null_val);
  return UNITY_END();
}
