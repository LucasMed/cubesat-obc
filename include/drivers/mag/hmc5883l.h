/**
 * @file hmc5883l.h
 * @brief Driver interface for the HMC5883L 3-axis magnetometer.
 *
 * Provides a minimal read API used by sensor_read_task to obtain
 * calibrated magnetic field measurements for EKF yaw update (PR-19)
 * and momentum-dump direction reference (PR-17).
 *
 * Platform implementations:
 *   Host / unit-test  : src/drivers/mag/hmc5883l.c  (fixed-value stub)
 *   RP2040 / Pico     : src/drivers/mag/hmc5883l.c  (I2C hardware, same file
 *                       with PICO_BUILD guard selecting the real read path)
 *
 * Units:  µT (micro-Tesla)
 * I2C address: 0x1E (factory default)
 * Spec ref: HMC5883L datasheet Rev D, PHASE5_PLAN PR-18
 */

#ifndef HMC5883L_H
#define HMC5883L_H

/**
 * @brief Initialise the HMC5883L and verify communication.
 *
 * Configures the device for continuous-measurement mode at the default
 * output data rate.  On the host stub this is a no-op that always
 * succeeds.
 *
 * @return  0  Success (or host stub always returns 0).
 * @return -1  Communication failure (RP2040 only).
 */
int hmc5883l_init(void);

/**
 * @brief Read the calibrated magnetic field vector.
 *
 * Performs one I2C burst read and converts raw counts to µT.
 * On the host stub the function always succeeds and returns a fixed
 * representative field vector: {25.0f, 0.0f, 42.0f} µT.
 *
 * @param field_uT  Output: [Bx, By, Bz] in µT.  3-element float array.
 * @return  0  Success.
 * @return -1  Read failure (RP2040 only; never on host stub).
 */
int hmc5883l_read(float field_uT[3]);

#endif /* HMC5883L_H */
