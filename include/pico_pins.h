/*
 * @file pico_pins.h
 * @brief Pico 2W GPIO Pin Definitions
 *
 * Pin mappings for Raspberry Pi Pico 2W OBC hardware.
 * RP2040 has 30 GPIO pins (GPIO0-GPIO29).
 */

#ifndef PICO_PINS_H
#define PICO_PINS_H

/* ========== I2C Pin Definitions ========== */

#define I2C0_PORT i2c0
#define I2C0_SDA_PIN 4
#define I2C0_SCL_PIN 5
#define I2C0_SPEED_HZ 400000  // 400 kHz for sensor polling

#define I2C1_PORT i2c1
#define I2C1_SDA_PIN 2
#define I2C1_SCL_PIN 3
#define I2C1_SPEED_HZ 400000

/* ========== SPI Pin Definitions ========== */

#define SPI0_PORT spi0
#define SPI0_SCK_PIN 18
#define SPI0_MOSI_PIN 19
#define SPI0_MISO_PIN 16
#define SPI0_BAUD_RATE 1000000  // 1 MHz initial baud rate

#define SPI_CS_MAG_PIN 6
#define SPI_CS_SD_PIN 7
#define SPI_CS_CAM_PIN 14

#define MAG_DRDY_PIN 11

/* ========== UART Pin Definitions ========== */

#define UART0_PORT uart0
#define UART0_TX_PIN 0
#define UART0_RX_PIN 1
#define UART0_BAUD_RATE 115200

#define UART1_PORT uart1
#define UART1_TX_PIN 8
#define UART1_RX_PIN 9
#define UART1_BAUD_RATE 115200

/* ========== Sensor Addresses (I2C) ========== */

#define MPU6050_I2C_ADDR 0x68
#define MPU6050_I2C_PORT I2C0_PORT

#define TEMP_SENSOR_MODE TEMP_SENSOR_ADC4  // Use onboard ADC
#define TEMP_I2C_ADDR 0x48                 // If using TMP102

/* ========== Power & Test Pins ========== */

#define STATUS_LED_PIN PICO_DEFAULT_LED_PIN
#define WATCHDOG_PIN 20  // Placeholder

#define RW_MOTOR1_PIN 6  // PWM3A
#define RW_MOTOR2_PIN 7  // PWM3B
#define RW_MOTOR3_PIN 3  // Moved from GPIO10 to GPIO3 to avoid CAM_FIFO_RDY conflict

/* ========== Payload Extra GPIOs ========== */

#define MAG_DRDY_PIN 11

#endif  // PICO_PINS_H
