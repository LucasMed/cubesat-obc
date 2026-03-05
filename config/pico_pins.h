/**
 * @file pico_pins.h
 * @brief Pico 2W GPIO Pin Definitions
 *
 * Pin mappings for Raspberry Pi Pico 2W OBC hardware.
 * RP2040 has 30 GPIO pins (GPIO0-GPIO29).
 */

#ifndef PICO_PINS_H
#define PICO_PINS_H

/* ========== I2C Pin Definitions ========== */

/**
 * I2C0 Bus Configuration
 * RP2040 I2C0: GPIO4 (SDA), GPIO5 (SCL)
 * Standard speed: 100 kHz
 * Fast speed: 400 kHz
 */
#define I2C0_PORT i2c0
#define I2C0_SDA_PIN 4
#define I2C0_SCL_PIN 5
#define I2C0_SPEED_HZ 400000  // 400 kHz for sensor polling

/**
 * I2C1 Bus Configuration (optional, future expansion)
 * RP2040 I2C1: GPIO2 (SDA), GPIO3 (SCL)
 */
#define I2C1_PORT i2c1
#define I2C1_SDA_PIN 2
#define I2C1_SCL_PIN 3
#define I2C1_SPEED_HZ 400000

/* ========== UART Pin Definitions ========== */

/**
 * UART0 Debug/Logging Console
 * RP2040 UART0: GPIO0 (TX), GPIO1 (RX)
 * Baud rate: 115200
 */
#define UART0_PORT uart0
#define UART0_TX_PIN 0
#define UART0_RX_PIN 1
#define UART0_BAUD_RATE 115200

/**
 * UART1 Telemetry / TT&C (CSP/KISS to radio)
 * RP2350 UART1: GPIO8 (TX), GPIO9 (RX)
 * Dedicated pins — no conflict with I2C0 (GPIO4/5)
 */
#define UART1_PORT uart1
#define UART1_TX_PIN 8
#define UART1_RX_PIN 9
#define UART1_BAUD_RATE 115200

/* ========== Sensor Addresses (I2C) ========== */

/**
 * MPU6050 6-DOF IMU
 * Standard I2C address (AD0 pin pulled to GND)
 */
#define MPU6050_I2C_ADDR 0x68
#define MPU6050_I2C_PORT I2C0_PORT

/**
 * Temperature Sensor
 * Option A: Onboard RP2040 ADC4 (no I2C)
 * Option B: TMP102 external (I2C address 0x48 default)
 */
#define TEMP_SENSOR_MODE TEMP_SENSOR_ADC4  // Use onboard ADC
#define TEMP_I2C_ADDR 0x48                 // If using TMP102

/* ========== Power & Test Pins ========== */

/**
 * LED outputs (for debug/status)
 * PICO_DEFAULT_LED_PIN typically GPIO25 (onboard LED on Pico/Pico 2W)
 */
#define STATUS_LED_PIN PICO_DEFAULT_LED_PIN

/**
 * Watchdog pin (optional GPIO for external watchdog circuit)
 * Not used in Phase 2
 */
#define WATCHDOG_PIN 20  // Placeholder

/**
 * PWM outputs for Reaction Wheel motors (future Phase 3)
 * RP2040 supports PWM on GPIO0-29 via 8 PWM slices
 * Each slice has 2 channels (A and B)
 */
#define RW_MOTOR1_PIN 6   // PWM3A
#define RW_MOTOR2_PIN 7   // PWM3B
#define RW_MOTOR3_PIN 10  // PWM5A — GPIO8 reassigned to UART1 TX

/**
 * Magnetorquer control (PWM or digital GPIO)
 * Phase 2: Simulated (PWM pins reserved for future)
 */
#define MAG_X_PIN 14  // PWM7A
#define MAG_Y_PIN 15  // PWM7B
#define MAG_Z_PIN 16  // PWM0A

/* ========== ADC Channels ========== */

/**
 * Analog-to-Digital Converter pins for power monitoring
 */
#define ADC_VBATT_PIN 26  // ADC0 - Battery voltage sense
#define ADC_TEMP_PIN 27   // ADC1 - Onboard temperature sensor
// ADC4 (GPIO29) reserved for onboard RP2040 temperature

#endif  // PICO_PINS_H
