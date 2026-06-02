/**
 * @file pwm_hal.h
 * @brief PWM Hardware Abstraction Layer for reaction wheels and magnetorquers.
 *
 * Provides a platform-independent PWM interface using the Pico SDK hardware_pwm
 * API, targeting 25 kHz operation (above audible range) for motor control.
 *
 * Platform implementations:
 *   Host / unit-test  : stub implementations (no-ops)
 *   RP2040/RP2350 Pico : src/actuators/pwm_hal.c (hardware_pwm)
 *
 * Pin mapping (from pico_pins.h):
 *   Reaction Wheels: GPIO10 (PWM5A), GPIO11 (PWM5B), GPIO12 (PWM6A)
 *   Magnetorquers:  GPIO22 (MAG_X), GPIO15 (MAG_Y), GPIO16 (MAG_Z)
 */

#ifndef PWM_HAL_H
#define PWM_HAL_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define PWM_HZ 25000

  int pwm_hal_rw_init(void);

  void pwm_hal_rw_set_duty(uint8_t axis, float duty_cycle);

  void pwm_hal_rw_enable(uint8_t axis, bool enable);

  int pwm_hal_mtq_init(void);

  void pwm_hal_mtq_set_duty(uint8_t axis, float duty_cycle);

  void pwm_hal_mtq_enable(uint8_t axis, bool enable);

#ifdef __cplusplus
}
#endif

#endif /* PWM_HAL_H */
