#include "pwm_hal.h"

int pwm_hal_rw_init(void)
{
    return 0;
}

void pwm_hal_rw_set_duty(uint8_t axis, float duty_cycle)
{
    (void)axis;
    (void)duty_cycle;
}

void pwm_hal_rw_enable(uint8_t axis, bool enable)
{
    (void)axis;
    (void)enable;
}
