/**
 * @file telemetry_storage.h
 * @brief Telemetry storage interface using W25Q64 SPI flash
 *
 * Provides persistent storage for telemetry data when no downlink is available.
 * Uses W25Q64 external flash for large data storage.
 *
 * Flash layout (8MB W25Q64):
 *   0x000000 - 0x0FFFFF (1MB): Telemetry log
 *   0x100000 - 0x1FFFFF (1MB): Event log
 *   0x200000 - 0x7FFFFF (6MB): Reserved for future use
 *
 * Each telemetry record is 64 bytes:
 *   [0..3]   timestamp (uint32_t, seconds since boot)
 *   [4..7]   sequence number
 *   [8..11]  roll (float)
 *   [12..15] pitch (float)
 *   [16..19] yaw (float)
 *   [20..23] gyro_x (float)
 *   [24..27] gyro_y (float)
 *   [28..31] gyro_z (float)
 *   [32..35] acc_x (float)
 *   [36..39] acc_y (float)
 *   [40..43] acc_z (float)
 *   [44..47] mag_x (float)
 *   [48..51] mag_y (float)
 *   [52..55] mag_z (float)
 *   [56..59] battery_voltage (float)
 *   [60..63] flags (uint32_t)
 */

#ifndef TELEMETRY_STORAGE_H
#define TELEMETRY_STORAGE_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  /**
   * @brief Telemetry record structure
   */
  typedef struct
  {
    uint32_t timestamp;    /**< Seconds since boot */
    uint32_t sequence;     /**< Record sequence number */
    float roll;            /**< Roll angle in degrees */
    float pitch;           /**< Pitch angle in degrees */
    float yaw;             /**< Yaw angle in degrees */
    float gyro_x;          /**< Gyroscope X in deg/s */
    float gyro_y;          /**< Gyroscope Y in deg/s */
    float gyro_z;          /**< Gyroscope Z in deg/s */
    float acc_x;           /**< Accelerometer X in g */
    float acc_y;           /**< Accelerometer Y in g */
    float acc_z;           /**< Accelerometer Z in g */
    float mag_x;           /**< Magnetometer X in uT */
    float mag_y;           /**< Magnetometer Y in uT */
    float mag_z;           /**< Magnetometer Z in uT */
    float battery_voltage; /**< Battery voltage in V */
    uint32_t flags;        /**< Status flags */
  } telemetry_record_t;

  _Static_assert(sizeof(telemetry_record_t) == 64, "Telemetry record must be 64 bytes");

  /**
   * @brief Storage statistics
   */
  typedef struct
  {
    uint32_t records_written; /**< Total records stored */
    uint32_t current_address; /**< Current write address */
    uint32_t last_sequence;   /**< Last sequence number */
    bool initialized;         /**< Storage initialized */
  } telemetry_storage_stats_t;

  /**
   * @brief Initialize telemetry storage
   *
   * @return true on success
   */
  bool telemetry_storage_init(void);

  /**
   * @brief Store a telemetry record
   *
   * @param record Pointer to telemetry record
   * @return true on success
   */
  bool telemetry_storage_store(const telemetry_record_t *record);

  /**
   * @brief Read a telemetry record
   *
   * @param address Flash address to read from
   * @param record Pointer to store record
   * @return true on success
   */
  bool telemetry_storage_read(uint32_t address, telemetry_record_t *record);

  /**
   * @brief Get storage statistics
   *
   * @param stats Pointer to store statistics
   */
  void telemetry_storage_get_stats(telemetry_storage_stats_t *stats);

  /**
   * @brief Get number of records that can be stored
   *
   * @return Number of available record slots
   */
  uint32_t telemetry_storage_available(void);

  /**
   * @brief Check if storage is available
   *
   * @return true if storage initialized and ready
   */
  bool telemetry_storage_is_available(void);

  /**
   * @brief Clear all telemetry data (factory reset)
   *
   * @return true on success
   */
  bool telemetry_storage_clear(void);

  /**
   * @brief Get the base address for telemetry storage
   *
   * @return Base address of telemetry region
   */
  uint32_t telemetry_storage_get_base_addr(void);

#ifdef __cplusplus
}
#endif

#endif /* TELEMETRY_STORAGE_H */
