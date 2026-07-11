// Global configuration and hardware mapping
#ifndef CONFIG_H
#define CONFIG_H

#define I2C_SDA_PIN 4
#define I2C_SCL_PIN 5

#define UART_TX_PIN 0
#define UART_RX_PIN 1

#define CONTROL_LOOP_HZ 10

/** Task notification bits for HealthMonitorTask */
#define HM_NOTIFY_FAULT_CRITICAL (1u << 0)

// Reaction wheel params
#define RW_MAX_OMEGA_RPM 4000.0f
#define RW_INERTIA 0.001f

#define PWM_HZ 25000

#define DEFAULT_KP 0.5f
#define DEFAULT_KI 0.01f
#define DEFAULT_KD 0.1f

/** Momentum-dump gain for FM_DETUMBLE [A·m²·s / (kg·m²)] */
#define DETUMBLE_K_DUMP 0.01f

/** Momentum dump threshold [kg·m²/s].
 *  Derived from RW max angular momentum × 80% safety margin.
 *  H_max = RW_INERTIA * RW_MAX_OMEGA_RPM * 2π/60
 *        = 0.001 * 4000 * 0.10472 ≈ 0.419 kg·m²/s
 *  Threshold = H_max * 0.80 = 0.335 kg·m²/s
 */
#define MOMENTUM_DUMP_THRESHOLD 0.335f

/**
 * Magnetic declination for the launch site [rad].
 *
 * Declination is the angle between magnetic north and geographic north.
 * Positive = east declination (magnetic north east of true north).
 *
 * Default 0.0 rad (equatorial/simulation).  For a real mission set this
 * to the value from NOAA IGRF (https://www.ngdc.noaa.gov/geomag/calculators/)
 * at the launch latitude/longitude, e.g.:
 *   Buenos Aires, Argentina ≈ -0.0524 rad (-3.0°)
 *   Cape Canaveral, USA     ≈ -0.1134 rad (-6.5°)
 */
#ifndef OBC_MAG_DECLINATION_RAD
  #define OBC_MAG_DECLINATION_RAD 0.0f
#endif

#endif  // CONFIG_H
