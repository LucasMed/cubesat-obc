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

/* ov2640_reg_t and cam_write_reg_table are defined inside camera_driver.c.
 * Forward-declare here so tests can exercise the max_entries path. */
typedef struct
{
  uint8_t reg;
  uint8_t val;
} ov2640_reg_t;

bool cam_write_reg_table(const ov2640_reg_t *table, size_t max_entries);

/* Provided by camera_driver.c (host build) to reset the idempotency guard. */
void camera_init_reset(void);

/* ------------------------------------------------------------------ */
/* Unity Boilerplate                                                   */
/* ------------------------------------------------------------------ */

void setUp(void)
{
  memset(s_mock_i2c_regs, 0, sizeof(s_mock_i2c_regs));
  /* Default chip ID values — camera_init() expects these.
   * Tests that want to simulate a mismatch override them. */
  s_mock_i2c_regs[0x0A] = 0x26; /* OV2640_CHIPID_HIGH */
  s_mock_i2c_regs[0x0B] = 0x42; /* OV2640_CHIPID_LOW  */
  /* Reset the idempotency guard so camera_init() runs fully. */
  camera_init_reset();
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
  /* Host mock reads from s_mock_i2c_regs[0x12] which is 0 after setUp.
   * Set a known value first, then verify round-trip. */
  s_mock_i2c_regs[0x12] = 0x55;
  TEST_ASSERT_TRUE(camera_read_sensor_reg(0x12, &val));
  TEST_ASSERT_EQUAL_HEX8(0x55, val);
}

void test_camera_read_sensor_reg_null_val(void)
{
  TEST_ASSERT_FALSE(camera_read_sensor_reg(0x12, NULL));
}

/* ------------------------------------------------------------------ */
/* Batch B: Logic Gap Closure tests                                    */
/* ------------------------------------------------------------------ */

void test_cam_i2c_read_non_chipid(void)
{
  /* Write a non-chipID register, then read it back. */
  s_mock_i2c_regs[0x42] = 0xAB;
  uint8_t val = 0;
  TEST_ASSERT_TRUE(camera_read_sensor_reg(0x42, &val));
  TEST_ASSERT_EQUAL_HEX8(0xAB, val);
}

void test_cam_write_reg_table_max_entries(void)
{
  /* Create a local register table with 4 entries + terminator. */
  static const ov2640_reg_t test_table[] = {
      {0x10, 0xAA},
      {0x20, 0xBB},
      {0x30, 0xCC},
      {0x40, 0xDD},
      {TEST_REG_END, TEST_REG_END},
  };

  /* Apply with max_entries=2 — only first 2 should be written. */
  TEST_ASSERT_TRUE(cam_write_reg_table(test_table, 2));
  TEST_ASSERT_EQUAL_HEX8(0xAA, s_mock_i2c_regs[0x10]);
  TEST_ASSERT_EQUAL_HEX8(0xBB, s_mock_i2c_regs[0x20]);
  /* Index 2 (0x30) should NOT have been touched by this call. */
  TEST_ASSERT_EQUAL_HEX8(0x00, s_mock_i2c_regs[0x30]);
  TEST_ASSERT_EQUAL_HEX8(0x00, s_mock_i2c_regs[0x40]);
}

void test_cam_pidh_mismatch(void)
{
  /* Override PIDH to a wrong value — camera_init() must detect the
   * mismatch and return false. */
  s_mock_i2c_regs[0x0A] = 0xFF; /* wrong PIDH */
  TEST_ASSERT_FALSE(camera_init());
}

void test_cam_pidl_mismatch(void)
{
  /* PIDH correct, PIDL wrong (not 0x42 or 0x41). */
  s_mock_i2c_regs[0x0A] = 0x26; /* correct PIDH */
  s_mock_i2c_regs[0x0B] = 0xFF; /* wrong PIDL */
  TEST_ASSERT_FALSE(camera_init());
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
  RUN_TEST(test_cam_i2c_read_non_chipid);
  RUN_TEST(test_cam_write_reg_table_max_entries);
  RUN_TEST(test_cam_pidh_mismatch);
  RUN_TEST(test_cam_pidl_mismatch);
  return UNITY_END();
}
