/**
 * @file payload_manager.h
 * @brief Payload subsystem central controller.
 *
 * Orchestrates power rail control, driver initialization, and
 * metadata tracking (image counts, cumulative dose, etc.).
 */

#ifndef PAYLOAD_MANAGER_H
#define PAYLOAD_MANAGER_H

#include <stdbool.h>
#include <stdint.h>

/** Payload Global Status */
typedef struct
{
  bool rail_enabled;
  bool sd_mounted;
  uint32_t image_count;
  float cumulative_dose;
  float last_mag_vector[3];
  uint32_t last_update_ts;
} payload_status_t;

/**
 * @brief Initialize payload manager and GPIOs.
 */
void payload_manager_init(void);

/**
 * @brief Enable or disable the payload power rail (3.3V/5V).
 *
 * @param enable true to enable, false to disable.
 * @return true if state was changed successfully.
 */
bool payload_manager_enable(bool enable);

/**
 * @brief Perform a full subsystem health check.
 *
 * Pings Magnetometer, Camera, and SD card.
 * @return true if all critical devices respond.
 */
bool payload_manager_health_check(void);

/**
 * @brief Get the current status snapshot.
 */
payload_status_t payload_manager_get_status(void);

/**
 * @brief Increment the image capture counter by one.
 *
 * Called after a successful image storage operation so that
 * get_status() always reflects the actual count.
 */
void payload_manager_increment_image_count(void);

/**
 * @brief Get the cumulative radiation dose estimate.
 *
 * @return Estimated total dose in Gy (placeholder units until calibration).
 */
float payload_manager_get_cumulative_dose(void);

#endif /* PAYLOAD_MANAGER_H */
