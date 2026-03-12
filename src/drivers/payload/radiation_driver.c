/**
 * @file radiation_driver.c
 * @brief Radiation Detector Driver — ADC2 implementation.
 *
 * Reads the analog output of the Hamamatsu S1223-01 PIN diode /
 * OPA134 TIA circuit via ADC channel 2 (GPIO28 / RAD_SIGNAL_PIN).
 *
 * On PICO_BUILD: uses Pico SDK hardware/adc.h.
 * On host builds: uses a weak mock symbol for adc_read().
 *
 * Spec ref: ICD-PAYLOAD-001 §4.3, §11.5
 */

#include "radiation_driver.h"
#include "pico_pins.h"

#include <string.h>

#if defined(PICO_BUILD)
  #include "hardware/adc.h"
  #include "hardware/gpio.h"
#endif

/* ------------------------------------------------------------------ */
/* Internal constants                                                  */
/* ------------------------------------------------------------------ */

/** ADC channel for the radiation signal (ADC2 = GPIO28). */
#define RAD_ADC_CHANNEL 2u

/** 12-bit ADC full scale count. */
#define RAD_ADC_FULL_SCALE 4095u

/** Detection threshold in raw ADC counts (10% of full scale). */
#define RAD_THRESHOLD_RAW \
  ((uint16_t)((float)RAD_ADC_FULL_SCALE * RAD_THRESHOLD_FRACTION))

/**
 * Placeholder dose conversion: 1 event = 1 µGy.
 * Replace with real calibration once characterised.
 */
#define RAD_DOSE_PER_EVENT_GY 1e-6f

/* ------------------------------------------------------------------ */
/* Module state                                                        */
/* ------------------------------------------------------------------ */

static rad_accumulator_t s_acc;

/* ------------------------------------------------------------------ */
/* ADC read abstraction                                                */
/* ------------------------------------------------------------------ */

#if defined(PICO_BUILD)
static uint16_t adc_read_channel(void)
{
  adc_select_input(RAD_ADC_CHANNEL);
  return adc_read();
}
#else
/* Host: weak symbol overridden by the unit test */
extern uint16_t adc_read(void);
static uint16_t adc_read_channel(void)
{
  return adc_read();
}
#endif

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */

bool radiation_init(void)
{
#if defined(PICO_BUILD)
  adc_init();
  adc_gpio_init(RAD_SIGNAL_PIN);

  /* Optional: configure comparator interrupt GPIO as digital input */
  gpio_init(RAD_IRQ_PIN);
  gpio_set_dir(RAD_IRQ_PIN, GPIO_IN);
  gpio_pull_down(RAD_IRQ_PIN);
#endif

  memset(&s_acc, 0, sizeof(s_acc));
  return true;
}

bool radiation_read(rad_sample_t *sample)
{
  if (!sample)
  {
    return false;
  }

  uint16_t raw = adc_read_channel();

  sample->raw_adc   = raw;
  sample->voltage_V = ((float)raw / (float)RAD_ADC_FULL_SCALE) * RAD_ADC_VREF_V;
  sample->threshold = (raw >= RAD_THRESHOLD_RAW);

  return true;
}

void radiation_accumulate(const rad_sample_t *sample)
{
  if (!sample || !sample->threshold)
  {
    return;
  }
  s_acc.event_count++;
  s_acc.dose_Gy += RAD_DOSE_PER_EVENT_GY;
}

void radiation_get_and_reset(rad_accumulator_t *acc)
{
  if (!acc)
  {
    return;
  }
  *acc   = s_acc;
  s_acc.event_count = 0;
  s_acc.dose_Gy     = 0.0f;
}
