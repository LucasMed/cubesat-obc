/**
 * @file w25q64.c
 * @brief W25Q64 SPI Flash Memory Driver implementation
 *
 * Host stub (PICO_BUILD not defined):
 *   All functions are no-ops that return W25Q64_OK.
 *
 * Pico (PICO_BUILD defined):
 *   Real SPI implementation using spi_payload.h bus.
 *
 * Spec ref: W25Q64JV datasheet
 */

#include "w25q64.h"

#include "crc32.h"
#include "drivers/imu/imu_calib.h"
#include "spi_payload.h"

#include <stdio.h>
#include <string.h>

/* Constants available for both Pico and Host builds */
#define W25Q64_PAGE_SIZE 256
#define W25Q64_SECTOR_SIZE 4096
#define W25Q64_BLOCK32_SIZE 32768
#define W25Q64_BLOCK64_SIZE 65536

/* IMU calibration flash constants — shared to avoid duplication */
#define W25Q64_IMU_CALIB_MAGIC 0xDEADBEEF
#define W25Q64_IMU_CALIB_SIZE 64u /* Padded to sector-friendly alignment */

#include "flash_layout.h"

#ifdef PICO_BUILD
  #include "hardware/gpio.h"
  #include "hardware/spi.h"
  #include "pico/time.h"

  #include <stdio.h>
  #include <string.h>

  /* Flash layout for region definitions */
  #include "flash_layout.h"

  /* W25Q64 Flash Chip Select pin (GPIO7) */
  #define W25Q64_CS_PIN 7

  /* SPI port fallback */
  #ifndef SPI0_PORT
    #define SPI0_PORT spi0
  #endif

  /* W25Q64 Commands */
  #define W25Q64_CMD_READ_JEDEC_ID 0x9F
  #define W25Q64_CMD_READ_STATUS_REG1 0x05
  #define W25Q64_CMD_WRITE_STATUS_REG1 0x01
  #define W25Q64_CMD_WRITE_ENABLE 0x06
  #define W25Q64_CMD_WRITE_DISABLE 0x04
  #define W25Q64_CMD_READ_DATA 0x03
  #define W25Q64_CMD_PAGE_PROGRAM 0x02
  #define W25Q64_CMD_SECTOR_ERASE_4KB 0x20
  #define W25Q64_CMD_BLOCK_ERASE_32KB 0x52
  #define W25Q64_CMD_BLOCK_ERASE_64KB 0xD8
  #define W25Q64_CMD_CHIP_ERASE 0xC7
  #define W25Q64_CMD_POWER_DOWN 0xB9
  #define W25Q64_CMD_RELEASE_POWER_DOWN 0xAB

  /* Expected JEDEC ID for W25Q64 */
  #define W25Q64_MANUFACTURER_WINBOND 0xEF
  #define W25Q64_MEMORY_TYPE 0x40
  #define W25Q64_CAPACITY_W25Q64 0x17

  /* Status register bits */
  #define W25Q64_STATUS_BUSY (1 << 0)
  #define W25Q64_STATUS_WRITE_EN (1 << 1)

  /* Timing */
  #define W25Q64_TIMEOUT_MS 500

static bool s_initialized = false;

static void cs_select(void)
{
  spi_payload_cs_select(W25Q64_CS_PIN);
}

static void cs_deselect(void)
{
  spi_payload_cs_deselect(W25Q64_CS_PIN);
}

static uint8_t spi_transfer(uint8_t data)
{
  uint8_t rx;
  spi_write_read_blocking(SPI0_PORT, &data, &rx, 1);
  return rx;
}

static void spi_transfer_buf(const uint8_t *tx, uint8_t *rx, size_t len)
{
  spi_write_read_blocking(SPI0_PORT, tx, rx, len);
}

w25q64_status_t w25q64_init(void)
{
  if (s_initialized)
  {
    return W25Q64_OK;
  }

  /* Initialize SPI bus if not already done */
  spi_payload_init();

  /* Verify device ID */
  w25q64_id_t id;
  if (w25q64_read_id(&id) != W25Q64_OK)
  {
    printf("w25q64: Failed to read ID\n");
    return W25Q64_ERR_ID;
  }

  if (!id.valid)
  {
    printf("w25q64: Invalid ID (M=%02X T=%02X C=%02X)\n", id.manufacturer_id, id.memory_type,
           id.capacity);
    return W25Q64_ERR_ID;
  }

  printf("w25q64: Initialized (M=%02X T=%02X C=%02X)\n", id.manufacturer_id, id.memory_type,
         id.capacity);

  s_initialized = true;
  return W25Q64_OK;
}

w25q64_status_t w25q64_read_id(w25q64_id_t *id)
{
  if (!id)
  {
    return W25Q64_ERR_INIT;
  }

  cs_select();
  spi_transfer(W25Q64_CMD_READ_JEDEC_ID);
  id->manufacturer_id = spi_transfer(0x00);
  id->memory_type = spi_transfer(0x00);
  id->capacity = spi_transfer(0x00);
  cs_deselect();

  id->valid = (id->manufacturer_id == W25Q64_MANUFACTURER_WINBOND &&
               id->memory_type == W25Q64_MEMORY_TYPE && id->capacity == W25Q64_CAPACITY_W25Q64);

  return W25Q64_OK;
}

uint8_t w25q64_read_status(void)
{
  cs_select();
  spi_transfer(W25Q64_CMD_READ_STATUS_REG1);
  uint8_t status = spi_transfer(0x00);
  cs_deselect();
  return status;
}

w25q64_status_t w25q64_wait_ready(uint32_t timeout_ms)
{
  absolute_time_t deadline = make_timeout_time_ms(timeout_ms);

  while (w25q64_read_status() & W25Q64_STATUS_BUSY)
  {
    if (time_reached(deadline))
    {
      return W25Q64_ERR_TIMEOUT;
    }
    /* Small delay to avoid hammering the SPI bus */
    busy_wait_us(10);
  }

  return W25Q64_OK;
}

w25q64_status_t w25q64_read(uint32_t addr, uint8_t *buf, uint32_t len)
{
  if (!buf || len == 0)
  {
    return W25Q64_ERR_WRITE;
  }

  cs_select();
  spi_transfer(W25Q64_CMD_READ_DATA);
  spi_transfer((addr >> 16) & 0xFF);
  spi_transfer((addr >> 8) & 0xFF);
  spi_transfer(addr & 0xFF);

  for (uint32_t i = 0; i < len; i++)
  {
    buf[i] = spi_transfer(0x00);
  }
  cs_deselect();

  return W25Q64_OK;
}

static w25q64_status_t write_enable(void)
{
  cs_select();
  spi_transfer(W25Q64_CMD_WRITE_ENABLE);
  cs_deselect();
  return W25Q64_OK;
}

w25q64_status_t w25q64_write_page(uint32_t addr, const uint8_t *buf, uint32_t len)
{
  if (!buf || len == 0 || len > W25Q64_PAGE_SIZE)
  {
    return W25Q64_ERR_WRITE;
  }

  /* Check if write crosses page boundary */
  uint32_t page_start = addr & ~(W25Q64_PAGE_SIZE - 1);
  uint32_t page_end = page_start + W25Q64_PAGE_SIZE;

  if (addr + len > page_end)
  {
    /* Split across pages: write first part, then remainder */
    uint32_t first_len = page_end - addr;
    if (w25q64_write_page(addr, buf, first_len) != W25Q64_OK)
    {
      return W25Q64_ERR_WRITE;
    }
    return w25q64_write_page(page_end, buf + first_len, len - first_len);
  }

  write_enable();

  cs_select();
  spi_transfer(W25Q64_CMD_PAGE_PROGRAM);
  spi_transfer((addr >> 16) & 0xFF);
  spi_transfer((addr >> 8) & 0xFF);
  spi_transfer(addr & 0xFF);

  for (uint32_t i = 0; i < len; i++)
  {
    spi_transfer(buf[i]);
  }
  cs_deselect();

  if (w25q64_wait_ready(100) != W25Q64_OK)
  {
    return W25Q64_ERR_TIMEOUT;
  }

  return W25Q64_OK;
}

w25q64_status_t w25q64_erase_sector(uint32_t addr)
{
  /* Must be 4KB-aligned */
  if (addr % W25Q64_SECTOR_SIZE != 0)
  {
    return W25Q64_ERR_ERASE;
  }

  write_enable();

  cs_select();
  spi_transfer(W25Q64_CMD_SECTOR_ERASE_4KB);
  spi_transfer((addr >> 16) & 0xFF);
  spi_transfer((addr >> 8) & 0xFF);
  spi_transfer(addr & 0xFF);
  cs_deselect();

  /* Wait for erase to complete */
  w25q64_wait_ready(500);

  return W25Q64_OK;
}

w25q64_status_t w25q64_erase_block32(uint32_t addr)
{
  if (addr % W25Q64_BLOCK32_SIZE != 0)
  {
    return W25Q64_ERR_ERASE;
  }

  write_enable();

  cs_select();
  spi_transfer(W25Q64_CMD_BLOCK_ERASE_32KB);
  spi_transfer((addr >> 16) & 0xFF);
  spi_transfer((addr >> 8) & 0xFF);
  spi_transfer(addr & 0xFF);
  cs_deselect();

  w25q64_wait_ready(1000);

  return W25Q64_OK;
}

w25q64_status_t w25q64_erase_block64(uint32_t addr)
{
  if (addr % W25Q64_BLOCK64_SIZE != 0)
  {
    return W25Q64_ERR_ERASE;
  }

  write_enable();

  cs_select();
  spi_transfer(W25Q64_CMD_BLOCK_ERASE_64KB);
  spi_transfer((addr >> 16) & 0xFF);
  spi_transfer((addr >> 8) & 0xFF);
  spi_transfer(addr & 0xFF);
  cs_deselect();

  w25q64_wait_ready(2000);

  return W25Q64_OK;
}

w25q64_status_t w25q64_erase_chip(void)
{
  write_enable();

  cs_select();
  spi_transfer(W25Q64_CMD_CHIP_ERASE);
  cs_deselect();

  /* Chip erase takes ~20-50 seconds for full 8MB */
  w25q64_wait_ready(60000);

  return W25Q64_OK;
}

uint32_t w25q64_get_capacity(void)
{
  return 8 * 1024 * 1024; /* 8MB */
}

bool w25q64_is_present(void)
{
  w25q64_id_t id;
  return w25q64_read_id(&id) == W25Q64_OK && id.valid;
}

/* ------------------------------------------------------------------ */
/* IMU calibration persistence                                         */
/* ------------------------------------------------------------------ */

w25q64_status_t w25q64_write_imu_calib(const imu_calib_t *cal)
{
  if (!cal)
  {
    return W25Q64_ERR_INIT;
  }

  uint8_t buf[W25Q64_IMU_CALIB_SIZE];
  memset(buf, 0, sizeof(buf));
  uint32_t offset = 0;

  /* Magic */
  uint32_t magic = W25Q64_IMU_CALIB_MAGIC;
  memcpy(&buf[offset], &magic, 4);
  offset += 4;

  /* accel_offset[3] (12 bytes) */
  memcpy(&buf[offset], cal->accel_offset, 12);
  offset += 12;

  /* accel_scale[3] (12 bytes) */
  memcpy(&buf[offset], cal->accel_scale, 12);
  offset += 12;

  /* gyro_offset_raw[3] (6 bytes) */
  memcpy(&buf[offset], cal->gyro_offset_raw, 6);
  offset += 6;

  /* gyro_bias_rads[3] (12 bytes) */
  memcpy(&buf[offset], cal->gyro_bias_rads, 12);
  offset += 12;

  /* calibrated flag (1 byte) */
  buf[offset++] = cal->calibrated ? 1 : 0;

  /* CRC32 over payload (4 bytes) */
  uint32_t crc = crc32_compute(buf, offset);
  memcpy(&buf[offset], &crc, 4);
  offset += 4;

  /* Erase the IMU calibration sector before first write */
  w25q64_status_t status = w25q64_erase_sector(FLASH_SECTOR_IMU_CALIB);
  if (status != W25Q64_OK)
  {
    printf("[w25q64] IMU calib: sector erase failed\n");
    return status;
  }

  /* Write serialised data (fits in one 256-byte page) */
  return w25q64_write_page(FLASH_SECTOR_IMU_CALIB, buf, offset);
}

w25q64_status_t w25q64_read_imu_calib(imu_calib_t *cal)
{
  if (!cal)
  {
    return W25Q64_ERR_INIT;
  }

  uint8_t buf[W25Q64_IMU_CALIB_SIZE];
  memset(buf, 0, sizeof(buf));

  w25q64_status_t status = w25q64_read(FLASH_SECTOR_IMU_CALIB, buf, sizeof(buf));
  if (status != W25Q64_OK)
  {
    return status;
  }

  uint32_t offset = 0;

  /* Magic */
  uint32_t magic;
  memcpy(&magic, &buf[offset], 4);
  offset += 4;
  if (magic != W25Q64_IMU_CALIB_MAGIC)
  {
    printf("[w25q64] IMU calib: invalid magic 0x%08X (sector not written)\n", magic);
    return W25Q64_ERR_INIT;
  }

  /* accel_offset[3] (12 bytes) */
  memcpy(cal->accel_offset, &buf[offset], 12);
  offset += 12;

  /* accel_scale[3] (12 bytes) */
  memcpy(cal->accel_scale, &buf[offset], 12);
  offset += 12;

  /* gyro_offset_raw[3] (6 bytes) */
  memcpy(cal->gyro_offset_raw, &buf[offset], 6);
  offset += 6;

  /* gyro_bias_rads[3] (12 bytes) */
  memcpy(cal->gyro_bias_rads, &buf[offset], 12);
  offset += 12;

  /* calibrated flag */
  cal->calibrated = (buf[offset++] == 1);

  /* CRC32 check */
  uint32_t crc_stored;
  memcpy(&crc_stored, &buf[offset], 4);
  uint32_t crc_computed = crc32_compute(buf, offset);
  if (crc_stored != crc_computed)
  {
    printf("[w25q64] IMU calib: CRC mismatch (stored=0x%08X computed=0x%08X)\n", crc_stored,
           crc_computed);
    return W25Q64_ERR_INIT;
  }

  return W25Q64_OK;
}

#else /* HOST BUILD - stub implementations */

/* Busy simulation for timeout testing */
static bool s_host_busy = false;

void w25q64_host_set_busy(bool busy)
{
  s_host_busy = busy;
}

w25q64_status_t w25q64_init(void)
{
  return W25Q64_OK;
}

w25q64_status_t w25q64_read_id(w25q64_id_t *id)
{
  if (id)
  {
    id->manufacturer_id = 0xEF;
    id->memory_type = 0x40;
    id->capacity = 0x17;
    id->valid = true;
  }
  return W25Q64_OK;
}

uint8_t w25q64_read_status(void)
{
  /* Return BUSY bit when simulation is active */
  return s_host_busy ? 0x01u : 0;
}

w25q64_status_t w25q64_wait_ready(uint32_t timeout_ms)
{
  if (!s_host_busy)
  {
    return W25Q64_OK;
  }

  /* Simulate timeout with iteration count (1 us per iteration) */
  uint32_t timeout_us = timeout_ms * 1000u;
  for (uint32_t elapsed = 0; elapsed < timeout_us; elapsed++)
  {
    if (!(w25q64_read_status() & 0x01u))
    {
      return W25Q64_OK;
    }
  }
  return W25Q64_ERR_TIMEOUT;
}

w25q64_status_t w25q64_read(uint32_t addr, uint8_t *buf, uint32_t len)
{
  (void)addr;
  if (buf && len > 0)
  {
    /* Host stub: fill with zeros */
    for (uint32_t i = 0; i < len; i++)
    {
      buf[i] = 0;
    }
  }
  return W25Q64_OK;
}

w25q64_status_t w25q64_write_page(uint32_t addr, const uint8_t *buf, uint32_t len)
{
  if (!buf || len == 0 || len > 256)
  {
    return W25Q64_ERR_WRITE;
  }
  (void)addr;

  if (w25q64_wait_ready(100) != W25Q64_OK)
  {
    return W25Q64_ERR_TIMEOUT;
  }

  return W25Q64_OK;
}

w25q64_status_t w25q64_erase_sector(uint32_t addr)
{
  if (addr % W25Q64_SECTOR_SIZE != 0)
  {
    return W25Q64_ERR_ERASE;
  }
  (void)addr;
  return W25Q64_OK;
}

w25q64_status_t w25q64_erase_block32(uint32_t addr)
{
  if (addr % W25Q64_BLOCK32_SIZE != 0)
  {
    return W25Q64_ERR_ERASE;
  }
  (void)addr;
  return W25Q64_OK;
}

w25q64_status_t w25q64_erase_block64(uint32_t addr)
{
  if (addr % W25Q64_BLOCK64_SIZE != 0)
  {
    return W25Q64_ERR_ERASE;
  }
  (void)addr;
  return W25Q64_OK;
}

w25q64_status_t w25q64_erase_chip(void)
{
  return W25Q64_OK;
}

uint32_t w25q64_get_capacity(void)
{
  return 8 * 1024 * 1024;
}

bool w25q64_is_present(void)
{
  return true;
}

/* Buffer-backed IMU calib storage for host tests */
static uint8_t s_imu_calib_buf[W25Q64_IMU_CALIB_SIZE];
static bool s_imu_calib_valid = false;

  #ifdef W25Q64_TEST_HELPERS
/* Test-only: corrupt internal calib buffer to exercise error paths */
void w25q64_host_corrupt_calib_magic(void)
{
  s_imu_calib_buf[0] = 0xFF; /*破坏 magic byte */
}

void w25q64_host_corrupt_calib_crc(void)
{
  /* Corrupt a data byte after a valid write — magic stays valid but CRC breaks */
  if (s_imu_calib_valid && W25Q64_IMU_CALIB_SIZE > 10)
  {
    s_imu_calib_buf[10] ^= 0xFF;
  }
}

void w25q64_host_reset_calib_valid(void)
{
  s_imu_calib_valid = false;
}
  #endif

w25q64_status_t w25q64_write_imu_calib(const imu_calib_t *cal)
{
  if (!cal)
  {
    return W25Q64_ERR_INIT;
  }

  memset(s_imu_calib_buf, 0, sizeof(s_imu_calib_buf));
  uint32_t offset = 0;

  /* Magic */
  uint32_t magic = W25Q64_IMU_CALIB_MAGIC;
  memcpy(&s_imu_calib_buf[offset], &magic, 4);
  offset += 4;

  /* accel_offset[3] (12 bytes) */
  memcpy(&s_imu_calib_buf[offset], cal->accel_offset, 12);
  offset += 12;

  /* accel_scale[3] (12 bytes) */
  memcpy(&s_imu_calib_buf[offset], cal->accel_scale, 12);
  offset += 12;

  /* gyro_offset_raw[3] (6 bytes) */
  memcpy(&s_imu_calib_buf[offset], cal->gyro_offset_raw, 6);
  offset += 6;

  /* gyro_bias_rads[3] (12 bytes) */
  memcpy(&s_imu_calib_buf[offset], cal->gyro_bias_rads, 12);
  offset += 12;

  /* calibrated flag (1 byte) */
  s_imu_calib_buf[offset++] = cal->calibrated ? 1 : 0;

  /* CRC32 (4 bytes) */
  uint32_t crc = crc32_compute(s_imu_calib_buf, offset);
  memcpy(&s_imu_calib_buf[offset], &crc, 4);

  s_imu_calib_valid = true;
  return W25Q64_OK;
}

w25q64_status_t w25q64_read_imu_calib(imu_calib_t *cal)
{
  if (!cal)
  {
    return W25Q64_ERR_INIT;
  }

  if (!s_imu_calib_valid)
  {
    return W25Q64_ERR_INIT;
  }

  uint32_t offset = 0;

  /* Magic */
  uint32_t magic;
  memcpy(&magic, &s_imu_calib_buf[offset], 4);
  offset += 4;
  if (magic != W25Q64_IMU_CALIB_MAGIC)
  {
    return W25Q64_ERR_INIT;
  }

  /* accel_offset[3] */
  memcpy(cal->accel_offset, &s_imu_calib_buf[offset], 12);
  offset += 12;

  /* accel_scale[3] */
  memcpy(cal->accel_scale, &s_imu_calib_buf[offset], 12);
  offset += 12;

  /* gyro_offset_raw[3] */
  memcpy(cal->gyro_offset_raw, &s_imu_calib_buf[offset], 6);
  offset += 6;

  /* gyro_bias_rads[3] */
  memcpy(cal->gyro_bias_rads, &s_imu_calib_buf[offset], 12);
  offset += 12;

  /* calibrated flag */
  cal->calibrated = (s_imu_calib_buf[offset++] == 1);

  /* CRC32 check */
  uint32_t crc_stored;
  memcpy(&crc_stored, &s_imu_calib_buf[offset], 4);
  uint32_t crc_computed = crc32_compute(s_imu_calib_buf, offset);
  if (crc_stored != crc_computed)
  {
    return W25Q64_ERR_INIT;
  }

  return W25Q64_OK;
}

#endif /* PICO_BUILD */
