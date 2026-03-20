#include "pwm_hal.h"

#include "pico_pins.h"

#ifdef PICO_BUILD
  #include "hardware/gpio.h"
  #include "hardware/pwm.h"

  #define PWM_WRAP 65535u

static uint16_t rw_duty[3] = {0, 0, 0};
static uint16_t mtq_duty[3] = {0, 0, 0};

static const uint rw_pins[3] = {
    RW_MOTOR1_PIN,
    RW_MOTOR2_PIN,
    RW_MOTOR3_PIN,
};

static const uint mtq_pins[3] = {
    MAG_X_PIN,
    MAG_Y_PIN,
    MAG_Z_PIN,
};

int pwm_hal_rw_init(void)
{
  for (uint8_t i = 0; i < 3; i++)
  {
    uint pin = rw_pins[i];
    gpio_set_function(pin, GPIO_FUNC_PWM);

    uint slice_num = pwm_gpio_to_slice_num(pin);
    pwm_config cfg = pwm_get_default_config();
    pwm_config_set_clkdiv(&cfg, 76.1875f);
    pwm_config_set_wrap(&cfg, PWM_WRAP);
    pwm_init(slice_num, &cfg, true);

    pwm_set_gpio_level(pin, 0);
  }
  return 0;
}

void pwm_hal_rw_set_duty(uint8_t axis, float duty_cycle)
{
  if (axis >= 3)
  {
    return;
  }
  if (duty_cycle < 0.0f)
  {
    duty_cycle = 0.0f;
  }
  else if (duty_cycle > 1.0f)
  {
    duty_cycle = 1.0f;
  }
  uint16_t level = (uint16_t)(duty_cycle * PWM_WRAP);
  rw_duty[axis] = level;
  pwm_set_gpio_level(rw_pins[axis], level);
}

void pwm_hal_rw_enable(uint8_t axis, bool enable)
{
  if (axis >= 3)
  {
    return;
  }
  uint slice_num = pwm_gpio_to_slice_num(rw_pins[axis]);
  pwm_set_enabled(slice_num, enable);
  if (!enable)
  {
    pwm_set_gpio_level(rw_pins[axis], 0);
  }
  else
  {
    pwm_set_gpio_level(rw_pins[axis], rw_duty[axis]);
  }
}

int pwm_hal_mtq_init(void)
{
  for (uint8_t i = 0; i < 3; i++)
  {
    uint pin = mtq_pins[i];
    gpio_set_function(pin, GPIO_FUNC_PWM);

    uint slice_num = pwm_gpio_to_slice_num(pin);
    pwm_config cfg = pwm_get_default_config();
    pwm_config_set_clkdiv(&cfg, 76.1875f);
    pwm_config_set_wrap(&cfg, PWM_WRAP);
    pwm_init(slice_num, &cfg, true);

    pwm_set_gpio_level(pin, 0);
  }
  return 0;
}

void pwm_hal_mtq_set_duty(uint8_t axis, float duty_cycle)
{
  if (axis >= 3)
  {
    return;
  }
  if (duty_cycle < 0.0f)
  {
    duty_cycle = 0.0f;
  }
  else if (duty_cycle > 1.0f)
  {
    duty_cycle = 1.0f;
  }
  uint16_t level = (uint16_t)(duty_cycle * PWM_WRAP);
  mtq_duty[axis] = level;
  pwm_set_gpio_level(mtq_pins[axis], level);
}

void pwm_hal_mtq_enable(uint8_t axis, bool enable)
{
  if (axis >= 3)
  {
    return;
  }
  uint slice_num = pwm_gpio_to_slice_num(mtq_pins[axis]);
  pwm_set_enabled(slice_num, enable);
  if (!enable)
  {
    pwm_set_gpio_level(mtq_pins[axis], 0);
  }
  else
  {
    pwm_set_gpio_level(mtq_pins[axis], mtq_duty[axis]);
  }
}

#else

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

int pwm_hal_mtq_init(void)
{
  return 0;
}

void pwm_hal_mtq_set_duty(uint8_t axis, float duty_cycle)
{
  (void)axis;
  (void)duty_cycle;
}

void pwm_hal_mtq_enable(uint8_t axis, bool enable)
{
  (void)axis;
  (void)enable;
}

#endif
