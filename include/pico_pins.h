/**
 * @file pico_pins.h
 * @brief Pico 2W GPIO Pin Definitions
 *
 * Pin mappings for Raspberry Pi Pico 2W OBC hardware.
 * RP2350 has 30 GPIO pins (GPIO0-GPIO29).
 *
 * Payload Bus Architecture (Phase 7):
 *   SPI0 (GPIO16/18/19) shared by RM3100 Magnetometer, Camera, microSD.
 *   UART0 (GPIO0/1) used by GPS NEO-6M/7M.
 *   UART1 (GPIO4/5) used by TT&C radio (CSP/KISS).
 *   ADC2  (GPIO28) used by Radiation Detector analog signal.
 *
 * GPIO allocation summary:
 *   GPIO0   UART0 TX  → GPS RX
 *   GPIO1   UART0 RX  ← GPS TX
 *   GPIO2   I2C1 SDA  (optional / future)
 *   GPIO3   PWM RW3   (moved from GPIO10 to free CAM_FIFO_RDY)
 *   GPIO4   UART1 TX  → TT&C TX
 *   GPIO5   UART1 RX  ← TT&C RX
 *   GPIO6   SPI CS    → RM3100 CS (active low)
 *   GPIO7   SPI CS    → microSD CS (active low)
 *   GPIO8   PWM RW1   (PWM4A)
 *   GPIO9   PWM RW2   (PWM4B)
 *   GPIO10  INT       ← Camera FIFO Ready
 *   GPIO11  INT       ← Magnetometer DRDY
 *   GPIO12  INT/PPS   ← GPS 1PPS
 *   GPIO13  INT       ← Radiation comparator
 *   GPIO14  SPI CS    → Camera CS (active low)
 *   GPIO15  OUT       → Camera RESET
 *   GPIO16  SPI0 MISO ← Payload bus
 *   GPIO17  OUT       → Magnetorquer X
 *   GPIO18  SPI0 SCK  → Payload bus
 *   GPIO19  SPI0 MOSI → Payload bus
 *   GPIO20  OUT       → Watchdog kick
 *   GPIO21  OUT       → Payload rail enable
 *   GPIO22  OUT       → Camera TRIGGER
 *   GPIO23  (internal Pico 2W)
 *   GPIO24  (internal Pico 2W)
 *   GPIO25  LED       Onboard status LED
 *   GPIO26  ADC0      Battery voltage sense
 *   GPIO27  ADC1      Board temperature
 *   GPIO28  ADC2      Radiation detector signal
 */

#ifndef PICO_PINS_H
#define PICO_PINS_H

/* ======================================================================
 * I2C Pin Definitions — Shared Register Bus (I2C1)
 * ====================================================================== */

/**
 * I2C1 Bus is shared by MPU6050 IMU and OV2640 Camera registers.
 * RP2350 I2C1: GPIO2 (SDA), GPIO3 (SCL)
 */
#define I2C1_PORT i2c1
#define I2C1_SDA_PIN 2
#define I2C1_SCL_PIN 3
#define I2C1_SPEED_HZ 400000

/* ======================================================================
 * SPI Pin Definitions — Shared Payload Bus (SPI0)
 * ====================================================================== */

/**
 * SPI0 is shared by RM3100 Magnetometer, OV2640 Camera, and microSD.
 * Each device is activated by its individual Chip Select (active low).
 * Baud rate: 1 MHz at init; may be raised to 20 MHz for data transfers.
 */
#define SPI0_PORT spi0
#define SPI0_MISO_PIN 16
#define SPI0_SCK_PIN 18
#define SPI0_MOSI_PIN 19
#define SPI0_BAUD_RATE_INIT 1000000  /* 1 MHz  — safe for all devices  */
#define SPI0_BAUD_RATE_FAST 10000000 /* 10 MHz — data transfers         */

/** Chip Select pins (active low, GPIO-controlled) */
#define SPI_CS_MAG_PIN 6  /**< RM3100 Magnetometer chip select */
#define SPI_CS_SD_PIN 7   /**< microSD chip select              */
#define SPI_CS_CAM_PIN 14 /**< OV2640 Camera chip select        */

/* ======================================================================
 * UART Pin Definitions
 * ====================================================================== */

/**
 * UART0 — GPS NEO-6M / NEO-7M receiver
 * Default baud rate: 9600 bps (NMEA); may be raised to 115200 via UBX.
 */
#define UART0_PORT uart0
#define UART0_TX_PIN 0 /**< OBC TX → GPS RX */
#define UART0_RX_PIN 1 /**< GPS TX → OBC RX */
#define UART0_BAUD_RATE 9600

/**
 * UART1 — TT&C radio (CSP / KISS framing)
 */
#define UART1_PORT uart1
#define UART1_TX_PIN 4 /**< OBC TX → Radio RX */
#define UART1_RX_PIN 5 /**< Radio TX → OBC RX */
#define UART1_BAUD_RATE 115200

/* ======================================================================
 * Payload Interrupt & Timing Signals
 * ====================================================================== */

#define CAM_FIFO_RDY_PIN 10 /**< Camera FIFO ready interrupt (active high) */
#define MAG_DRDY_PIN 11     /**< RM3100 data-ready interrupt (active high)  */
#define GPS_PPS_PIN 12      /**< GPS 1 Hz PPS timing reference              */
#define RAD_IRQ_PIN 13      /**< Radiation comparator threshold interrupt    */

/* ======================================================================
 * Payload Control Signals
 * ====================================================================== */

#define CAM_RESET_PIN 15      /**< OV2640 hardware reset (active low)        */
#define CAM_TRIGGER_PIN 22    /**< OV2640 capture trigger (active high pulse) */
#define PAYLOAD_ENABLE_PIN 21 /**< Payload power rail enable (active high)   */

/* ======================================================================
 * Reaction Wheel PWM Outputs
 * ====================================================================== */

/**
 * RP2350 PWM.  RW3 was moved to GPIO29 to free GPIO3
 * for the I2C SCL bus (ICD Phase 7, WP-7.4).
 */
#define RW_MOTOR1_PIN 8  /**< PWM4A */
#define RW_MOTOR2_PIN 9  /**< PWM4B */
#define RW_MOTOR3_PIN 29 /**< PWM6B — moved from GPIO3 */

/* ======================================================================
 * Magnetorquer PWM/GPIO Outputs
 * ====================================================================== */

#define MAG_X_PIN 17 /**< Magnetorquer X-axis */
#define MAG_Y_PIN 23 /**< Magnetorquer Y-axis (GPIO23 on RP2350 is available) */
#define MAG_Z_PIN 24 /**< Magnetorquer Z-axis (GPIO24 on RP2350 is available) */

/* ======================================================================
 * Miscellaneous
 * ====================================================================== */

#define STATUS_LED_PIN PICO_DEFAULT_LED_PIN /**< GPIO25 onboard LED */
#define WATCHDOG_PIN 20                     /**< External watchdog kick output */

/* ======================================================================
 * Sensor Addresses (I2C)
 * ====================================================================== */

/** MPU6050 — standard address (AD0 = GND) */
#define MPU6050_I2C_ADDR 0x68
#define MPU6050_I2C_PORT I2C1_PORT

/* ======================================================================
 * ADC Channel Pins
 * ====================================================================== */

#define ADC_VBATT_PIN 26  /**< ADC0 — Battery voltage sense                */
#define ADC_TEMP_PIN 27   /**< ADC1 — Board / sensor temperature monitor    */
#define RAD_SIGNAL_PIN 28 /**< ADC2 — Radiation detector analog signal      */

#endif /* PICO_PINS_H */
