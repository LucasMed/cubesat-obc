/**
 * @file post.h
 * @brief Power-On Self-Test (POST) definitions and API.
 *
 * The POST suite runs once during vStartupTask() before any deploy
 * sequence begins.  Results are persisted to a dedicated W25Q64 flash
 * sector for boot-failure analysis and are queryable via the STATUS
 * command.
 *
 * Spec ref: FMM-SPEC-090–100 (deploy-automation)
 */

#ifndef POST_H
#define POST_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  /* ------------------------------------------------------------------ */
  /* Constants                                                           */
  /* ------------------------------------------------------------------ */

#define POST_MAGIC          0x504F5354u  /**< "POST" magic               */
#define POST_RECORD_COUNT   32u          /**< Ring buffer depth in flash */
#define POST_FLASH_OFFSET   0x700000u    /**< 7 MB offset in W25Q64      */
#define POST_FLASH_SECTOR   0x700000u    /**< Single 4 KB sector         */

  /* ------------------------------------------------------------------ */
  /* Boot reason codes                                                   */
  /* ------------------------------------------------------------------ */

#define POST_BOOT_POWER_ON       0u  /**< Normal power-on reset         */
#define POST_BOOT_WATCHDOG       1u  /**< Watchdog-triggered reset      */
#define POST_BOOT_STACK_OVERFLOW 2u  /**< Stack overflow detected       */
#define POST_BOOT_BROWNOUT       3u  /**< Brownout / voltage dip        */

  /* ------------------------------------------------------------------ */
  /* POST test bitmap positions                                          */
  /* ------------------------------------------------------------------ */

#define POST_TEST_IMU         0u   /**< MPU6050 / ICM-20948        */
#define POST_TEST_MAG         1u   /**< HMC5883L / RM3100          */
#define POST_TEST_SHT31       2u   /**< SHT31 temp/humidity        */
#define POST_TEST_BH1750      3u   /**< BH1750 light sensor        */
#define POST_TEST_RTC         4u   /**< DS3231 RTC                 */
#define POST_TEST_POWER       5u   /**< INA219 bus power           */
#define POST_TEST_SOLAR       6u   /**< INA219 solar panel         */
#define POST_TEST_GPS         7u   /**< GPS module                 */
#define POST_TEST_FLASH       8u   /**< W25Q64 external flash      */
#define POST_TEST_I2C_BUS     9u   /**< I2C bus scan               */
#define POST_TEST_COUNT       10u  /**< Number of POST tests       */

  /* ------------------------------------------------------------------ */
  /* POST record structure                                               */
  /* ------------------------------------------------------------------ */

  /**
   * @brief Persistent POST record written to flash on every boot.
   *
   * Size: 40 bytes.  A 4 KB flash sector can hold ~102 records,
   * but we limit to POST_RECORD_COUNT (32) in the ring buffer.
   */
  typedef struct
  {
    uint32_t magic;          /**< POST_MAGIC for validity check     */
    uint32_t boot_count;     /**< Incremented each boot             */
    uint32_t boot_reason;    /**< POST_BOOT_*                      */
    uint32_t timestamp_rtc;  /**< RTC seconds at POST time, 0 if NA*/
    uint32_t test_bitmap;    /**< Bit N = 1 if test N passed       */
    uint32_t test_detail;    /**< Bit N = 1 if test N was executed */
    char     task_name[12];  /**< Stack-overflow task name (NUL)   */
    uint32_t crc32;          /**< CRC32 over preceding fields      */
  } post_record_t;

  /* ------------------------------------------------------------------ */
  /* POST API                                                            */
  /* ------------------------------------------------------------------ */

  /**
   * @brief Run the full POST suite.
   *
   * Detects boot reason, runs each subsystem test, writes the record
   * to flash, stores it in the data layer, and emits
   * LOG_EVT_POST_COMPLETE.  On critical failure, reports a fault
   * which triggers fmm_force_safe().
   *
   * @param[out] record  Filled with test results and metadata.
   */
  void post_run(post_record_t *record);

  /**
   * @brief Read the most recent POST record from flash.
   *
   * @param[out] record  Populated with the last written record, or
   *                     zeroed if no valid record exists.
   */
  void post_read_last(post_record_t *record);

  /**
   * @brief Check whether a POST record indicates critical failure.
   *
   * Critical failures are defined as any of the following tests
   * having their bitmap bit clear: IMU, RTC, Flash.
   *
   * @param[in] record  POST record to evaluate.
   * @return true if a critical subsystem test failed.
   */
  bool post_is_critical_fail(const post_record_t *record);

  /**
   * @brief Return a human-readable name for a boot-reason code.
   *
   * @param reason  POST_BOOT_* value.
   * @return Pointer to a constant string, or "UNKNOWN".
   */
  const char *post_boot_reason_name(uint32_t reason);

#ifdef __cplusplus
}
#endif

#endif /* POST_H */
