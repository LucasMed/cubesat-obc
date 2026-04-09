/**
 * @file bh1750.h
 * @brief BH1750 Digital Light Sensor Driver
 *
 * I2C digital light sensor for illuminance measurement (Lux).
 * Datasheet: https://www.mouser.com/datasheet/2/348/bh1750fvi-e-186247.pdf
 *
 * Specifications:
 * - Illuminance range: 1 - 65535 lux
 * - Resolution: 0.5 lux (H-Res-2 mode)
 * - Measurement time: 16 ms (H-Res-2 mode)
 * - I2C addresses: 0x23 (default), 0x5C (ADR = HIGH)
 */

#ifndef BH1750_H
#define BH1750_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  /**
   * @brief Default I2C address (ADR pin floating/low).
   */
#define BH1750_ADDR_DEFAULT 0x23

  /**
   * @brief Alternate I2C address (ADR pin tied to HIGH).
   */
#define BH1750_ADDR_ALTERNATE 0x5C

  /**
   * @brief Power on command.
   */
#define BH1750_CMD_POWER_ON 0x01

  /**
   * @brief Reset command (reset data register, not the whole chip).
   */
#define BH1750_CMD_RESET 0x07

  /**
   * @brief One-time H-Res-2 mode (0.5 lux resolution, 16 ms).
   *
   * This is the mode used by this driver. It provides the best balance
   * between resolution and measurement speed, fitting within the 100 ms
   * sensor_read_task cycle without timing issues.
   */
#define BH1750_CMD_OT_H_RES2 0x20

  /**
   * @brief Continuous H-Res-2 mode (0.5 lux resolution, 16 ms).
   */
#define BH1750_CMD_CONT_H_RES2 0x11

  /**
   * @brief Initialize the BH1750 sensor.
   *
   * Sends power-on and reset commands, then verifies the sensor is present.
   *
   * @param addr I2C address (0x23 or 0x5C)
   * @return true on success, false on failure
   */
  bool bh1750_init(uint8_t addr);

  /**
   * @brief Check if a BH1750 sensor is present at the given address.
   *
   * @param addr I2C address to probe
   * @return true if the sensor responds with valid data
   */
  bool bh1750_is_present(uint8_t addr);

  /**
   * @brief Read current illuminance.
   *
   * Triggers a one-time H-Res-2 measurement (16 ms), then reads the result.
   *
   * @param lux Pointer to float where illuminance in Lux will be stored.
   *            On failure, the value is unchanged.
   * @return true on success, false on failure (I2C error or invalid data)
   */
  bool bh1750_read(float *lux);

#ifdef __cplusplus
}
#endif

#endif /* BH1750_H */
