/**
 * @file temperature.h
 * @brief Temperature Sensor Driver Interface.
 */

#ifndef TEMPERATURE_H
#define TEMPERATURE_H

/**
 * @brief Initialize the temperature sensor.
 * 
 * @return 0 on success, negative error code otherwise.
 */
int temperature_init(void);

/**
 * @brief Read the current temperature.
 * 
 * @return Temperature in degrees Celsius.
 */
float temperature_read(void);

#endif // TEMPERATURE_H
