/**
 * @file post.c
 * @brief Power-On Self-Test (POST) suite implementation.
 *
 * Runs at the end of vStartupTask(), after all sensor inits have
 * completed.  POST VERIFIES that each subsystem is working correctly
 * by performing read-back tests — it does NOT re-initialise drivers.
 *
 * Results are persisted to a ring buffer in the W25Q64 flash sector
 * at FLASH_SECTOR_POST (0x700000), and the most recent record is
 * stored in the Data Layer for fast STATUS queries.
 *
 * Spec ref: FMM-SPEC-090–100, FMM-DES-001 §4
 */

#include "post.h"

#include "bh1750.h"
#include "data_layer.h"
#include "drivers/imu/mpu6050.h"
#include "drivers/mag/hmc5883l.h"
#include "ds3231.h"
#include "fault_manager.h"
#include "flash_layout.h"
#include "gps_driver.h"
#include "ina219.h"
#include "log_event_ids.h"
#include "logger.h"
#include "sht31.h"
#include "w25q64.h"

#include <string.h>

#ifdef PICO_BUILD
  #include "hardware/watchdog.h"
#endif

/* ------------------------------------------------------------------ */
/* CRC-32 lookup table (polynomial 0xEDB88320, reflected)              */
/* ------------------------------------------------------------------ */

static uint32_t s_crc32_table[256];
static bool s_crc32_table_initialised = false;

static void post_crc32_init_table(void)
{
  for (uint32_t i = 0; i < 256; i++)
  {
    uint32_t crc = i;
    for (uint32_t j = 0; j < 8; j++)
    {
      if (crc & 1u)
      {
        crc = (crc >> 1u) ^ 0xEDB88320u;
      }
      else
      {
        crc >>= 1u;
      }
    }
    s_crc32_table[i] = crc;
  }
  s_crc32_table_initialised = true;
}

static uint32_t post_crc32(const void *data, size_t len)
{
  if (!s_crc32_table_initialised)
  {
    post_crc32_init_table();
  }

  uint32_t crc = 0xFFFFFFFFu;
  const uint8_t *bytes = (const uint8_t *)data;
  for (size_t i = 0; i < len; i++)
  {
    uint8_t idx = (uint8_t)((crc ^ bytes[i]) & 0xFFu);
    crc = (crc >> 8u) ^ s_crc32_table[idx];
  }
  return crc ^ 0xFFFFFFFFu;
}

/* ------------------------------------------------------------------ */
/* Boot reason detection                                               */
/* ------------------------------------------------------------------ */

static uint32_t post_detect_boot_reason(char *task_name_out, size_t task_name_size)
{
  if (task_name_out != NULL && task_name_size > 0)
  {
    task_name_out[0] = '\0';
  }

#ifdef PICO_BUILD
  uint32_t scratch = watchdog_hw->scratch[0];

  if (scratch == 0xDEAD0001u)
  {
    /* Stack overflow — read task name from scratch[1-3] */
    if (task_name_out != NULL && task_name_size > 0)
    {
      char name[13] = {0};
      for (int i = 0; i < 3; i++)
      {
        uint32_t w = watchdog_hw->scratch[1 + i];
        name[i * 4 + 0] = (char)(w & 0xFFu);
        name[i * 4 + 1] = (char)((w >> 8u) & 0xFFu);
        name[i * 4 + 2] = (char)((w >> 16u) & 0xFFu);
        name[i * 4 + 3] = (char)((w >> 24u) & 0xFFu);
      }
      name[12] = '\0';
      size_t copy_len = (task_name_size - 1) < 12 ? (task_name_size - 1) : 12;
      memcpy(task_name_out, name, copy_len);
      task_name_out[copy_len] = '\0';
    }

    /* Clear scratch so we don't repeat on next boot */
    watchdog_hw->scratch[0] = 0;

    return POST_BOOT_STACK_OVERFLOW;
  }

  if (scratch != 0)
  {
    return POST_BOOT_WATCHDOG;
  }
#else
  (void)task_name_out;
  (void)task_name_size;
#endif /* PICO_BUILD */

  /* Default: power-on reset */
  return POST_BOOT_POWER_ON;
}

/* ------------------------------------------------------------------ */
/* Flash ring buffer helpers                                           */
/* ------------------------------------------------------------------ */

static uint32_t post_ring_read_index(void)
{
  uint32_t idx = 0;
#ifdef PICO_BUILD
  w25q64_read(FLASH_SECTOR_POST, (uint8_t *)&idx, sizeof(idx));
#else
  (void)idx;
#endif
  return idx;
}

static void post_ring_write_index(uint32_t idx)
{
#ifdef PICO_BUILD
  w25q64_write_page(FLASH_SECTOR_POST, (const uint8_t *)&idx, sizeof(idx));
#else
  (void)idx;
#endif
}

static bool post_ring_is_empty(void)
{
  uint32_t idx = post_ring_read_index();
  // cppcheck-suppress knownConditionTrueFalse
  return (idx == 0 || idx > POST_RECORD_COUNT);
}

static uint32_t post_ring_next_index(uint32_t current_idx)
{
  if (current_idx >= POST_RECORD_COUNT)
  {
    return 1u; /* wrap to first slot */
  }
  return current_idx + 1u;
}

static uint32_t post_ring_record_offset(uint32_t index)
{
  /* First 4 bytes = ring index; records start at offset 4 */
  return FLASH_SECTOR_POST + 4u + ((index - 1u) * sizeof(post_record_t));
}

/* ------------------------------------------------------------------ */
/* POST tests — each returns true if the sensor passes                 */
/* ------------------------------------------------------------------ */

static bool post_test_imu(void)
{
#ifdef PICO_BUILD
  float roll = 0.0f, pitch = 0.0f, yaw = 0.0f;
  return (mpu6050_read(&roll, &pitch, &yaw) == 0);
#else
  return true; /* host stub: assume pass */
#endif
}

static bool post_test_mag(void)
{
#ifdef PICO_BUILD
  float field_uT[3] = {0.0f, 0.0f, 0.0f};
  return (hmc5883l_read(field_uT) == 0);
#else
  return true;
#endif
}

static bool post_test_sht31(void)
{
#ifdef PICO_BUILD
  float temp = 0.0f, hum = 0.0f;
  return sht31_read(&temp, &hum);
#else
  return true;
#endif
}

static bool post_test_bh1750(void)
{
#ifdef PICO_BUILD
  float lux = 0.0f;
  return bh1750_read(&lux);
#else
  return true;
#endif
}

static bool post_test_rtc(void)
{
#ifdef PICO_BUILD
  return ds3231_init(); /* re-init to verify RTC is alive */
#else
  return true;
#endif
}

static bool post_test_power(void)
{
#ifdef PICO_BUILD
  int16_t mv = ina219_get_voltage_mv();
  return (mv > 0);
#else
  return true;
#endif
}

static bool post_test_solar(void)
{
#ifdef PICO_BUILD
  ina219_data_t data;
  return ina219_solar_read_power(&data);
#else
  return true;
#endif
}

static bool post_test_gps(void)
{
#ifdef PICO_BUILD
  /* GPS init is called during startup; verify it responds */
  GpsFix_t fix;
  return gps_get_last_fix(&fix);
#else
  return true;
#endif
}

static bool post_test_flash(void)
{
#ifdef PICO_BUILD
  w25q64_id_t id;
  if (w25q64_read_id(&id) != W25Q64_OK)
  {
    return false;
  }
  return id.valid;
#else
  return true;
#endif
}

static bool post_test_i2c_bus(void)
{
#ifdef PICO_BUILD
  /* I2C bus init happened during startup — try scanning for any device.
   * A minimal check: read from the IMU to confirm bus is alive. */
  float roll = 0.0f, pitch = 0.0f, yaw = 0.0f;
  return (mpu6050_read(&roll, &pitch, &yaw) == 0);
#else
  return true;
#endif
}

/* ------------------------------------------------------------------ */
/* RTC timestamp helper                                                */
/* ------------------------------------------------------------------ */

static uint32_t post_read_rtc_timestamp(void)
{
#ifdef PICO_BUILD
  uint16_t year = 0;
  uint8_t month = 0, day = 0, hour = 0, minute = 0, second = 0;
  if (ds3231_read_time(&year, &month, &day, &hour, &minute, &second))
  {
    return ds3231_to_epoch(year, month, day, hour, minute, second);
  }
#else
  (void)0;
#endif
  return 0u;
}

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */

void post_run(post_record_t *record)
{
  if (record == NULL)
  {
    return;
  }
  memset(record, 0, sizeof(*record));

  /* ---- 1. Detect boot reason ---- */
  record->magic = POST_MAGIC;
  record->boot_reason = post_detect_boot_reason(record->task_name, sizeof(record->task_name));

  /* ---- 2. Read previous boot count ---- */
  post_record_t prev;
  post_read_last(&prev);
  if (prev.magic == POST_MAGIC)
  {
    record->boot_count = prev.boot_count + 1u;
  }
  else
  {
    record->boot_count = 1u; /* first valid boot */
  }

  /* ---- 3. Run subsystem tests ---- */

  /* IMU */
  record->test_detail |= (1u << POST_TEST_IMU);
  if (post_test_imu())
  {
    record->test_bitmap |= (1u << POST_TEST_IMU);
  }

  /* MAG */
  record->test_detail |= (1u << POST_TEST_MAG);
  if (post_test_mag())
  {
    record->test_bitmap |= (1u << POST_TEST_MAG);
  }

  /* SHT31 */
  record->test_detail |= (1u << POST_TEST_SHT31);
  if (post_test_sht31())
  {
    record->test_bitmap |= (1u << POST_TEST_SHT31);
  }

  /* BH1750 */
  record->test_detail |= (1u << POST_TEST_BH1750);
  if (post_test_bh1750())
  {
    record->test_bitmap |= (1u << POST_TEST_BH1750);
  }

  /* RTC */
  record->test_detail |= (1u << POST_TEST_RTC);
  if (post_test_rtc())
  {
    record->test_bitmap |= (1u << POST_TEST_RTC);
  }

  /* INA219 bus power */
  record->test_detail |= (1u << POST_TEST_POWER);
  if (post_test_power())
  {
    record->test_bitmap |= (1u << POST_TEST_POWER);
  }

  /* INA219 solar */
  record->test_detail |= (1u << POST_TEST_SOLAR);
  if (post_test_solar())
  {
    record->test_bitmap |= (1u << POST_TEST_SOLAR);
  }

  /* GPS */
  record->test_detail |= (1u << POST_TEST_GPS);
  if (post_test_gps())
  {
    record->test_bitmap |= (1u << POST_TEST_GPS);
  }

  /* Flash */
  record->test_detail |= (1u << POST_TEST_FLASH);
  if (post_test_flash())
  {
    record->test_bitmap |= (1u << POST_TEST_FLASH);
  }

  /* I2C bus */
  record->test_detail |= (1u << POST_TEST_I2C_BUS);
  if (post_test_i2c_bus())
  {
    record->test_bitmap |= (1u << POST_TEST_I2C_BUS);
  }

  /* ---- 4. Read RTC timestamp ---- */
  record->timestamp_rtc = post_read_rtc_timestamp();

  /* ---- 5. Compute CRC32 over preceding fields ---- */
  /* CRC covers all fields up to (but not including) crc32.
   * post_record_t layout: magic(4) + boot_count(4) + boot_reason(4)
   * + timestamp_rtc(4) + test_bitmap(4) + test_detail(4) + task_name(12)
   * = 36 bytes total. */
  record->crc32 = post_crc32(record, offsetof(post_record_t, crc32));

  /* ---- 6. Persist to flash ring buffer ---- */
#ifdef PICO_BUILD
  {
    uint32_t current_idx = post_ring_read_index();
    uint32_t next_idx;

    if (post_ring_is_empty())
    {
      /* First boot — erase sector and start at slot 1 */
      w25q64_erase_sector(FLASH_SECTOR_POST);
      next_idx = 1u;
    }
    else
    {
      next_idx = post_ring_next_index(current_idx);
      if (next_idx == 1u)
      {
        /* Wrapping around — erase entire sector for clean slate.
         * We lose previous records but avoid partial erase issues. */
        w25q64_erase_sector(FLASH_SECTOR_POST);
      }
    }

    /* Write the record */
    uint32_t addr = post_ring_record_offset(next_idx);
    w25q64_write_page(addr, (const uint8_t *)record, sizeof(*record));

    /* Update the ring index */
    post_ring_write_index(next_idx);
  }
#else
  (void)0; /* host stub — no-op */
#endif /* PICO_BUILD */

  /* ---- 7. Store in Data Layer ---- */
  data_layer_set_post_last(record);

  /* ---- 8. Log event ---- */
  {
    /* Payload: test_bitmap (4 bytes) + pass/fail flag (1 byte) */
    uint8_t event_data[5];
    memcpy(event_data, &record->test_bitmap, sizeof(record->test_bitmap));
    event_data[4] = (uint8_t)(post_is_critical_fail(record) ? 0u : 1u);
    log_event(LOG_EVT_POST_COMPLETE, LOG_CLASS_OPERATIONAL, event_data, sizeof(event_data));
  }

  /* ---- 9. Report fault on critical failure ---- */
  if (post_is_critical_fail(record))
  {
    fault_report(FAULT_POST_CRITICAL, FAULT_LEVEL_CRITICAL);
  }
}

void post_read_last(post_record_t *record)
{
  if (record == NULL)
  {
    return;
  }
  memset(record, 0, sizeof(*record));

#ifdef PICO_BUILD
  if (post_ring_is_empty())
  {
    return; /* no records — return zeroed */
  }

  uint32_t current_idx = post_ring_read_index();
  uint32_t addr = post_ring_record_offset(current_idx);

  w25q64_read(addr, (uint8_t *)record, sizeof(*record));

  /* Validate magic */
  if (record->magic != POST_MAGIC)
  {
    memset(record, 0, sizeof(*record));
    return;
  }

  /* Validate CRC32 */
  uint32_t expected_crc = record->crc32;
  record->crc32 = 0; /* temporarily zero for CRC computation */
  uint32_t computed_crc = post_crc32(record, offsetof(post_record_t, crc32));
  record->crc32 = expected_crc;

  if (computed_crc != expected_crc)
  {
    /* CRC mismatch — corruption detected */
    memset(record, 0, sizeof(*record));
  }
#else
  (void)0;
#endif /* PICO_BUILD */
}

bool post_is_critical_fail(const post_record_t *record)
{
  if (record == NULL)
  {
    return true; /* no record means critical by default */
  }

  /* Critical subsystems: IMU (bit 0), RTC (bit 4), Flash (bit 8) */
  const uint32_t critical_mask =
      (1u << POST_TEST_IMU) | (1u << POST_TEST_RTC) | (1u << POST_TEST_FLASH);

  return (record->test_bitmap & critical_mask) != critical_mask;
}

const char *post_boot_reason_name(uint32_t reason)
{
  switch (reason)
  {
  case POST_BOOT_POWER_ON:
    return "POWER_ON";
  case POST_BOOT_WATCHDOG:
    return "WATCHDOG";
  case POST_BOOT_STACK_OVERFLOW:
    return "STACK_OVERFLOW";
  case POST_BOOT_BROWNOUT:
    return "BROWNOUT";
  default:
    return "UNKNOWN";
  }
}
