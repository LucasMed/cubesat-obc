/**
 * @file test_diskio.c
 * @brief Unit tests for FatFs disk I/O bridge (diskio.c).
 *
 * Covers all 6 functions:
 *   disk_status, disk_initialize, disk_read, disk_write, disk_ioctl,
 *   get_fattime
 *
 * Runs on host.  Mock sd_spi_* functions replace the real SPI driver
 * so we can inject success/failure and verify FatFs-level responses.
 */

#include "ff.h"
#include "diskio.h"
#include "sd_spi.h"

#ifdef PICO_BUILD
  #include "unity.h"
#else
  #include "host/unity.h"
#endif

#include <string.h>

/* get_fattime() is defined in diskio.c but hidden from ff.h when
 * FF_FS_NORTC == 1 (the default).  Forward-declare it so the test
 * can call it directly.
 */
DWORD get_fattime(void);

/* ------------------------------------------------------------------ */
/* Mock sd_spi state                                                   */
/* ------------------------------------------------------------------ */

static bool      g_mock_sd_init_ok     = true;
static sd_type_t g_mock_sd_type        = SD_TYPE_SDHC;
static bool      g_mock_sd_read_ok     = true;
static bool      g_mock_sd_write_ok    = true;
static int       g_mock_sd_init_calls  = 0;
static int       g_mock_sd_read_calls  = 0;
static int       g_mock_sd_write_calls = 0;
static int       g_mock_sd_type_calls  = 0;

/* ------------------------------------------------------------------ */
/* Mock sd_spi function implementations                                */
/* ------------------------------------------------------------------ */

bool sd_spi_init(void)
{
  g_mock_sd_init_calls++;
  return g_mock_sd_init_ok;
}

sd_type_t sd_spi_get_type(void)
{
  g_mock_sd_type_calls++;
  return g_mock_sd_type;
}

bool sd_spi_read_sector(uint32_t sector, uint8_t *buffer)
{
  (void)sector;
  g_mock_sd_read_calls++;
  if (buffer)
  {
    /* Fill with predictable pattern so callers can verify data was written */
    memset(buffer, 0xA5, 512);
  }
  return g_mock_sd_read_ok;
}

bool sd_spi_write_sector(uint32_t sector, const uint8_t *buffer)
{
  (void)sector;
  (void)buffer;
  g_mock_sd_write_calls++;
  return g_mock_sd_write_ok;
}

/* ------------------------------------------------------------------ */
/* Helpers                                                             */
/* ------------------------------------------------------------------ */

#define DEV_SD  0
#define DEV_BAD 1

static void reset_mocks(void)
{
  g_mock_sd_init_ok     = true;
  g_mock_sd_type        = SD_TYPE_SDHC;
  g_mock_sd_read_ok     = true;
  g_mock_sd_write_ok    = true;
  g_mock_sd_init_calls  = 0;
  g_mock_sd_read_calls  = 0;
  g_mock_sd_write_calls = 0;
  g_mock_sd_type_calls  = 0;
}

/* ------------------------------------------------------------------ */
/* Unity boilerplate                                                   */
/* ------------------------------------------------------------------ */

void setUp(void)
{
  reset_mocks();
}

void tearDown(void)
{
}

/* ================================================================== */
/* disk_status                                                         */
/* ================================================================== */

void test_disk_status_wrong_pdrv(void)
{
  TEST_ASSERT_EQUAL_INT(STA_NOINIT, disk_status(DEV_BAD));
  TEST_ASSERT_EQUAL_INT(0, g_mock_sd_type_calls);
}

void test_disk_status_unknown_type(void)
{
  g_mock_sd_type = SD_TYPE_UNKNOWN;
  TEST_ASSERT_EQUAL_INT(STA_NOINIT, disk_status(DEV_SD));
  TEST_ASSERT_EQUAL_INT(1, g_mock_sd_type_calls);
}

void test_disk_status_known_type(void)
{
  TEST_ASSERT_EQUAL_INT(0, disk_status(DEV_SD));
  TEST_ASSERT_EQUAL_INT(1, g_mock_sd_type_calls);
}

/* ================================================================== */
/* disk_initialize                                                     */
/* ================================================================== */

void test_disk_initialize_wrong_pdrv(void)
{
  TEST_ASSERT_EQUAL_INT(STA_NOINIT, disk_initialize(DEV_BAD));
  TEST_ASSERT_EQUAL_INT(0, g_mock_sd_init_calls);
}

void test_disk_initialize_success(void)
{
  DSTATUS st = disk_initialize(DEV_SD);
  TEST_ASSERT_EQUAL_INT(0, st);
  TEST_ASSERT_EQUAL_INT(1, g_mock_sd_init_calls);
}

void test_disk_initialize_failure(void)
{
  g_mock_sd_init_ok = false;
  DSTATUS st = disk_initialize(DEV_SD);
  TEST_ASSERT_EQUAL_INT(STA_NOINIT, st);
  TEST_ASSERT_EQUAL_INT(1, g_mock_sd_init_calls);
}

/* ================================================================== */
/* disk_read                                                           */
/* ================================================================== */

void test_disk_read_wrong_pdrv(void)
{
  uint8_t buf[512];
  DRESULT res = disk_read(DEV_BAD, buf, 0, 1);
  TEST_ASSERT_EQUAL_INT(RES_PARERR, res);
  TEST_ASSERT_EQUAL_INT(0, g_mock_sd_read_calls);
}

void test_disk_read_zero_count(void)
{
  uint8_t buf[512];
  DRESULT res = disk_read(DEV_SD, buf, 0, 0);
  TEST_ASSERT_EQUAL_INT(RES_PARERR, res);
  TEST_ASSERT_EQUAL_INT(0, g_mock_sd_read_calls);
}

void test_disk_read_single_sector(void)
{
  uint8_t buf[512];
  memset(buf, 0, sizeof(buf));

  DRESULT res = disk_read(DEV_SD, buf, 42, 1);
  TEST_ASSERT_EQUAL_INT(RES_OK, res);
  TEST_ASSERT_EQUAL_INT(1, g_mock_sd_read_calls);
  /* Verify buffer was filled by mock */
  for (int i = 0; i < 512; i++)
  {
    TEST_ASSERT_EQUAL_INT(0xA5, buf[i]);
  }
}

void test_disk_read_multi_sector(void)
{
  uint8_t buf[1024]; /* 2 sectors */
  memset(buf, 0, sizeof(buf));

  DRESULT res = disk_read(DEV_SD, buf, 100, 2);
  TEST_ASSERT_EQUAL_INT(RES_OK, res);
  TEST_ASSERT_EQUAL_INT(2, g_mock_sd_read_calls);
  /* Both sectors filled */
  for (int i = 0; i < 1024; i++)
  {
    TEST_ASSERT_EQUAL_INT(0xA5, buf[i]);
  }
}

void test_disk_read_failure(void)
{
  uint8_t buf[512];
  g_mock_sd_read_ok = false;

  DRESULT res = disk_read(DEV_SD, buf, 0, 1);
  TEST_ASSERT_EQUAL_INT(RES_ERROR, res);
  TEST_ASSERT_EQUAL_INT(1, g_mock_sd_read_calls);
}

/* ================================================================== */
/* disk_write                                                          */
/* ================================================================== */

void test_disk_write_wrong_pdrv(void)
{
  const uint8_t buf[512] = {0};
  DRESULT res = disk_write(DEV_BAD, buf, 0, 1);
  TEST_ASSERT_EQUAL_INT(RES_PARERR, res);
  TEST_ASSERT_EQUAL_INT(0, g_mock_sd_write_calls);
}

void test_disk_write_zero_count(void)
{
  const uint8_t buf[512] = {0};
  DRESULT res = disk_write(DEV_SD, buf, 0, 0);
  TEST_ASSERT_EQUAL_INT(RES_PARERR, res);
  TEST_ASSERT_EQUAL_INT(0, g_mock_sd_write_calls);
}

void test_disk_write_single_sector(void)
{
  const uint8_t buf[512] = {0};
  DRESULT res = disk_write(DEV_SD, buf, 42, 1);
  TEST_ASSERT_EQUAL_INT(RES_OK, res);
  TEST_ASSERT_EQUAL_INT(1, g_mock_sd_write_calls);
}

void test_disk_write_multi_sector(void)
{
  const uint8_t buf[1024] = {0};
  DRESULT res = disk_write(DEV_SD, buf, 100, 2);
  TEST_ASSERT_EQUAL_INT(RES_OK, res);
  TEST_ASSERT_EQUAL_INT(2, g_mock_sd_write_calls);
}

void test_disk_write_failure(void)
{
  const uint8_t buf[512] = {0};
  g_mock_sd_write_ok = false;

  DRESULT res = disk_write(DEV_SD, buf, 0, 1);
  TEST_ASSERT_EQUAL_INT(RES_ERROR, res);
  TEST_ASSERT_EQUAL_INT(1, g_mock_sd_write_calls);
}

/* ================================================================== */
/* disk_ioctl                                                          */
/* ================================================================== */

void test_disk_ioctl_wrong_pdrv(void)
{
  uint32_t val = 0;
  DRESULT res = disk_ioctl(DEV_BAD, GET_SECTOR_COUNT, &val);
  TEST_ASSERT_EQUAL_INT(RES_PARERR, res);
}

void test_disk_ioctl_ctrl_sync(void)
{
  DRESULT res = disk_ioctl(DEV_SD, CTRL_SYNC, NULL);
  TEST_ASSERT_EQUAL_INT(RES_OK, res);
}

void test_disk_ioctl_get_sector_count(void)
{
  LBA_t count = 0;
  DRESULT res = disk_ioctl(DEV_SD, GET_SECTOR_COUNT, &count);
  TEST_ASSERT_EQUAL_INT(RES_OK, res);
  TEST_ASSERT_EQUAL_UINT32(2000000, count);
}

void test_disk_ioctl_get_sector_size(void)
{
  WORD size = 0;
  DRESULT res = disk_ioctl(DEV_SD, GET_SECTOR_SIZE, &size);
  TEST_ASSERT_EQUAL_INT(RES_OK, res);
  TEST_ASSERT_EQUAL_INT(512, size);
}

void test_disk_ioctl_get_block_size(void)
{
  DWORD blk = 0;
  DRESULT res = disk_ioctl(DEV_SD, GET_BLOCK_SIZE, &blk);
  TEST_ASSERT_EQUAL_INT(RES_OK, res);
  TEST_ASSERT_EQUAL_UINT32(1, blk);
}

void test_disk_ioctl_unknown_cmd(void)
{
  DRESULT res = disk_ioctl(DEV_SD, 0xFF, NULL);
  TEST_ASSERT_EQUAL_INT(RES_PARERR, res);
}

/* ================================================================== */
/* get_fattime                                                         */
/* ================================================================== */

void test_get_fattime_returns_expected(void)
{
  /* Expect 2026-03-14 15:00:00 encoded in FatFs format */
  DWORD ft = get_fattime();
  /* Bit fields:
   * [31:25] year  = 2026 - 1980 = 46  (0x5C)
   * [24:21] month = 3                   (0x03 << 21)
   * [20:16] day   = 14                  (0x0E << 16)
   * [15:11] hour  = 15                  (0x0F << 11)
   * [10:5]  min   = 0                   (0x00 << 5)
   * [4:0]   sec/2 = 0                   (0x00)
   */
  DWORD expected = ((DWORD)(2026 - 1980) << 25)
                 | ((DWORD)3 << 21)
                 | ((DWORD)14 << 16)
                 | ((DWORD)15 << 11)
                 | ((DWORD)0 << 5)
                 | ((DWORD)0 >> 1);
  TEST_ASSERT_EQUAL_UINT32(expected, ft);
}

/* ================================================================== */
/* Main — register all test cases below                               */
/* ================================================================== */

int main(void)
{
  UNITY_BEGIN();

  /* disk_status */
  RUN_TEST(test_disk_status_wrong_pdrv);
  RUN_TEST(test_disk_status_unknown_type);
  RUN_TEST(test_disk_status_known_type);

  /* disk_initialize */
  RUN_TEST(test_disk_initialize_wrong_pdrv);
  RUN_TEST(test_disk_initialize_success);
  RUN_TEST(test_disk_initialize_failure);

  /* disk_read */
  RUN_TEST(test_disk_read_wrong_pdrv);
  RUN_TEST(test_disk_read_zero_count);
  RUN_TEST(test_disk_read_single_sector);
  RUN_TEST(test_disk_read_multi_sector);
  RUN_TEST(test_disk_read_failure);

  /* disk_write */
  RUN_TEST(test_disk_write_wrong_pdrv);
  RUN_TEST(test_disk_write_zero_count);
  RUN_TEST(test_disk_write_single_sector);
  RUN_TEST(test_disk_write_multi_sector);
  RUN_TEST(test_disk_write_failure);

  /* disk_ioctl */
  RUN_TEST(test_disk_ioctl_wrong_pdrv);
  RUN_TEST(test_disk_ioctl_ctrl_sync);
  RUN_TEST(test_disk_ioctl_get_sector_count);
  RUN_TEST(test_disk_ioctl_get_sector_size);
  RUN_TEST(test_disk_ioctl_get_block_size);
  RUN_TEST(test_disk_ioctl_unknown_cmd);

  /* get_fattime */
  RUN_TEST(test_get_fattime_returns_expected);

  return UNITY_END();
}
