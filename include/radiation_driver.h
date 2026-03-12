/**
 * @file radiation_driver.h
 * @brief Radiation Detector Driver API (ADC2 interface).
 *
 * The radiation detector uses a Hamamatsu S1223-01 PIN photodiode with
 * an OPA134 transimpedance amplifier (TIA).  The analog output is sampled
 * by the RP2350 ADC on GPIO28 (ADC2 / RAD_SIGNAL_PIN).
 *
 * An optional digital comparator output (RAD_IRQ_PIN, GPIO13) can be used
 * as an interrupt source for high-energy event detection.
 *
 * Spec ref: ICD-PAYLOAD-001 §4.3, §11.5
 */

#ifndef RADIATION_DRIVER_H
#define RADIATION_DRIVER_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/* Configuration constants                                             */
/* ------------------------------------------------------------------ */

/** ADC full-scale voltage (3.3 V on RP2350). */
#define RAD_ADC_VREF_V 3.3f

/** ADC resolution in bits (RP2350 ADC is 12-bit). */
#define RAD_ADC_BITS 12u

/** Detection threshold as a fraction of full-scale (default: 10%). */
#define RAD_THRESHOLD_FRACTION 0.10f

/* ------------------------------------------------------------------ */
/* Data structures                                                     */
/* ------------------------------------------------------------------ */

/**
 * @brief Single radiation detector sample.
 */
typedef struct
{
  uint16_t raw_adc;   /**< Raw 12-bit ADC count (0–4095)            */
  float    voltage_V; /**< Converted voltage (V)                     */
  bool     threshold; /**< true if voltage exceeded detection level  */
} rad_sample_t;

/**
 * @brief Radiation event accumulator (reset after downlink).
 */
typedef struct
{
  uint32_t event_count;  /**< Total events above threshold since last reset */
  float    dose_Gy;      /**< Estimated accumulated dose (Gy, placeholder)  */
} rad_accumulator_t;

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */

/**
 * @brief Initialise the radiation detector ADC channel and comparator GPIO.
 *
 * Configures ADC2 (GPIO28) for input.  Optionally configures RAD_IRQ_PIN
 * (GPIO13) as a digital input for comparator-generated events.
 *
 * @return true on success.
 */
bool radiation_init(void);

/**
 * @brief Read a single ADC sample from the radiation detector.
 *
 * Selects ADC channel 2 (GPIO28), triggers a conversion, and populates
 * the result structure.  The threshold flag is set if voltage exceeds
 * RAD_THRESHOLD_FRACTION × RAD_ADC_VREF_V.
 *
 * @param[out] sample  Pointer to result buffer (must not be NULL).
 * @return true on success.
 */
bool radiation_read(rad_sample_t *sample);

/**
 * @brief Process a sample and accumulate event counts.
 *
 * If @p sample->threshold is true, increments the internal event counter
 * and updates the dose estimate.  Dose calculation is a placeholder
 * (1 count = 1 µGy) until calibration data is available.
 *
 * @param sample  Sample to process.
 */
void radiation_accumulate(const rad_sample_t *sample);

/**
 * @brief Return the current accumulator state and reset counters.
 *
 * @param[out] acc  Pointer to store the accumulated values.
 */
void radiation_get_and_reset(rad_accumulator_t *acc);

#ifdef __cplusplus
}
#endif

#endif /* RADIATION_DRIVER_H */
