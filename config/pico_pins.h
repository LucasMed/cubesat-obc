/**
 * @file pico_pins.h
 * @brief Pico 2W GPIO Pin Definitions
 *
 * Pin mappings for Raspberry Pi Pico 2W OBC hardware.
 * RP2350 has 30 GPIO pins (GPIO0-GPIO29).
 *
 * === HARDWARE VERIFICATION COMPLETED (2026-04-08) ===
 * 
 * Verified working sensors:
 * - MPU-6050/6500 IMU: I2C0 @ GPIO4/5 (0x68 / 0x70)
 * - QMC5883L magnetometer: I2C0 @ GPIO4/5 (0x2C) — NOTE: module is QMC5883L clone, not HMC5883L
 * - GPS NEO-6M/7M: UART0 @ GPIO0/1, 9600 baud
 * - HC-12 radio: UART1 @ GPIO8/9, 9600 baud
 * 
 * NOT YET CONNECTED:
 * - W25Q64 SPI Flash (GPIO7 CS)
 * - OV2640 camera (SPI0)
 * - Reaction wheels (PWM)
 * - Magnetorquers (PWM)
 *
 * GPIO allocation summary (ACTUAL):
 *   GPIO0   UART0 TX  → GPS RX
 *   GPIO1   UART0 RX  ← GPS TX
 *   GPIO2   I2C1 SDA  → OV2640 Camera (register config)
 *   GPIO3   I2C1 SCL  → OV2640 Camera (register config)
 *   GPIO4   I2C0 SDA  ← MPU-6050 SDA / QMC5883L SDA (shared bus)
 *   GPIO5   I2C0 SCL  ← MPU-6050 SCL / QMC5883L SCL (shared bus)
 *   GPIO6   SPI0 CS   → RM3100 (magnetometer SPI)
 *   GPIO7   SPI0 CS   → W25Q64 Flash (pending connection)
 *   GPIO8   UART1 TX  → HC-12 RX
 *   GPIO9   UART1 RX  ← HC-12 TX
 *   GPIO10  PWM5A     → RW Motor 1 / CAM_FIFO_RDY ⚠️ shared
 *   GPIO11  PWM5B     → RW Motor 2 / MAG_DRDY ⚠️ shared
 *   GPIO12  PWM6A     → RW Motor 3 / GPS PPS ⚠️ shared
 *   GPIO13  RAD_IRQ   ← Radiation detector interrupt
 *   GPIO14  PWM7A     → Magnetorquer X (was SPI0 CS)
 *   GPIO15  PWM7B     → Magnetorquer Y (was CAM_RESET)
 *   GPIO16  PWM0A     → Magnetorquer Z (was SPI0 MISO)
 *   GPIO17  SPI0 MISO ← Camera / Mag / Flash (was MAG_X)
 *   GPIO18  SPI0 SCK  → Camera / Mag / Flash
 *   GPIO19  SPI0 MOSI → Camera / Mag / Flash
 *   GPIO20  Watchdog kick (TPS3431)
 *   GPIO21  PAYLOAD_ENABLE → 5V rail (was MAG_Y)
 *   GPIO22  CAM_TRIGGER → OV2640 capture trigger (was MAG_Z)
 *   GPIO23  SPI0 CS   → OV2640 Camera (was spare)
 *   GPIO24  CAM_RESET → OV2640 hardware reset (was spare)
 *   GPIO25  LED       Onboard status LED
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
#define I2C1_PORT    i2c1
/* ======================================================================
 * I2C Pin Definitions — MPU-6050 IMU
 * ====================================================================== */

/**
 * I2C0 Bus for MPU-6050/6500 IMU.
 * RP2350 I2C0: GPIO4 (SDA), GPIO5 (SCL)
 * Note: MPU-6050 (0x68) or MPU-6500 (0x70) supported.
 */
#define I2C0_PORT i2c0
#define I2C0_SDA_PIN 4
#define I2C0_SCL_PIN 5
#define I2C0_SPEED_HZ 400000

/* ======================================================================
 * SPI Pin Definitions — Shared Payload Bus (SPI0)
 * ====================================================================== */

/**
 * SPI0 is shared by RM3100 Magnetometer, OV2640 Camera, and Flash.
 * Each device is activated by its individual Chip Select (active low).
 * Baud rate: 1 MHz at init; may be raised to 20 MHz for data transfers.
 *
 * NOTE: SPI0_MISO (GPIO16) and SPI_CS_CAM (GPIO14) were reassigned to
 * Magnetorquer PWM outputs (MAG_Z, MAG_X). Camera SPI pins updated below.
 */
#define SPI0_PORT     spi0
#define SPI0_MISO_PIN 17   /**< CHANGED: was GPIO16 — now MAG_Z_PIN */
#define SPI0_SCK_PIN  18
#define SPI0_MOSI_PIN 19
#define SPI0_BAUD_RATE_INIT 1000000   /* 1 MHz  — safe for all devices  */
#define SPI0_BAUD_RATE_FAST 10000000  /* 10 MHz — data transfers         */

/** Chip Select pins (active low, GPIO-controlled) */
#define SPI_CS_FLASH_PIN 7   /**< W25Q64 Flash chip select (replaces microSD) */
#define SPI_CS_MAG_PIN 6     /**< RM3100 Magnetometer chip select */
#define SPI_CS_CAM_PIN 23    /**< CHANGED: was GPIO14 — now MAG_X_PIN */

/* ======================================================================
 * UART Pin Definitions
 * ====================================================================== */

/**
 * UART0 — GPS NEO-6M / NEO-7M receiver
 * Default baud rate: 9600 bps (NMEA); may be raised to 115200 via UBX.
 */
#define UART0_PORT      uart0
#define UART0_TX_PIN    0     /**< OBC TX → GPS RX */
#define UART0_RX_PIN    1     /**< GPS TX → OBC RX */
#define UART0_BAUD_RATE 9600

/**
 * UART1 — TT&C radio (CSP / KISS framing)
 * Note: Using GPIO8/9 to avoid conflict with I2C0 (GPIO4/5) for MPU-6050
 */
#define UART1_PORT      uart1
#define UART1_TX_PIN    8     /**< OBC TX → Radio RX */
#define UART1_RX_PIN    9     /**< Radio TX → OBC RX */
#define UART1_BAUD_RATE 9600

/* ======================================================================
 * Payload Interrupt & Timing Signals
 * ====================================================================== */

#define CAM_FIFO_RDY_PIN 10  /**< Camera FIFO ready interrupt (active high) */
#define MAG_DRDY_PIN     11  /**< RM3100 data-ready interrupt (active high)  */
#define GPS_PPS_PIN      12  /**< GPS 1 Hz PPS timing reference              */
#define RAD_IRQ_PIN      13  /**< Radiation comparator threshold interrupt    */

/* ======================================================================
 * Payload Control Signals
 * ====================================================================== */

#define CAM_RESET_PIN      24  /**< CHANGED: was GPIO15 — now MAG_Y_PIN        */
#define CAM_TRIGGER_PIN    22  /**< OV2640 capture trigger (active high pulse) */
#define PAYLOAD_ENABLE_PIN 21  /**< Payload power rail enable (active high)   */

/* ======================================================================
 * Reaction Wheel PWM Outputs
 * ====================================================================== */

/**
 * RP2350/Pico 2W PWM pins.
 * RW1-3 use GPIO10/11/12.
 */
#define RW_MOTOR1_PIN 10  /**< PWM5A — Reaction Wheel 1 */
#define RW_MOTOR2_PIN 11  /**< PWM5B — Reaction Wheel 2 */
#define RW_MOTOR3_PIN 12  /**< PWM6A — Reaction Wheel 3 */

/* ======================================================================
 * Magnetorquer PWM/GPIO Outputs
 * ====================================================================== */

/**
 * Pico 2W: GPIO0-22, GPIO26-28 only.
 * Using GPIO14/15/16 — avoids conflict with PAYLOAD_ENABLE (GPIO21) and
 * CAM_TRIGGER (GPIO22). SPI0 camera interface moved to alternative pins.
 *
 * NOTE: GPIO14-16 were previously allocated to SPI0 camera interface.
 * The camera has been re-assigned to use SPI1 or alternative SPI0 pins
 * (see SPI section below).
 */
#define MAG_X_PIN 14 /**< PWM7A — Magnetorquer X-axis */
#define MAG_Y_PIN 15 /**< PWM7B — Magnetorquer Y-axis */
#define MAG_Z_PIN 16 /**< PWM0A — Magnetorquer Z-axis */

/* ======================================================================
 * Miscellaneous
 * ====================================================================== */

#define STATUS_LED_PIN PICO_DEFAULT_LED_PIN  /**< GPIO25 onboard LED */
#define WATCHDOG_PIN   20                    /**< External watchdog kick output */

/* ======================================================================
 * Sensor Addresses (I2C)
 * ====================================================================== */

/** MPU6050 — standard address (AD0 = GND) */
#define MPU6050_I2C_ADDR 0x68
#define MPU6050_I2C_PORT I2C1_PORT

/* ======================================================================
 * ADC Channel Pins
 * ====================================================================== */

#define ADC_VBATT_PIN  26  /**< ADC0 — Battery voltage sense                */
#define ADC_TEMP_PIN   27  /**< ADC1 — Board / sensor temperature monitor    */
#define RAD_SIGNAL_PIN 28  /**< ADC2 — Radiation detector analog signal      */

/* Sun Sensor pins (photodiodes for ADCS) */
#define SUN_SENSOR_X_PIN 27  /**< ADC1 — Sun sensor X axis */
#define SUN_SENSOR_Y_PIN 28  /**< ADC2 — Sun sensor Y axis */

#endif /* PICO_PINS_H */
