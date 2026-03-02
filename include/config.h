// Global configuration and hardware mapping
#ifndef CONFIG_H
#define CONFIG_H

#define I2C_SDA_PIN 16
#define I2C_SCL_PIN 17

#define UART_TX_PIN 0
#define UART_RX_PIN 1

#define CONTROL_LOOP_HZ 20

// Reaction wheel params
#define RW_MAX_OMEGA_RPM 4000.0f
#define RW_INERTIA 0.001f

#define DEFAULT_KP 0.5f
#define DEFAULT_KI 0.01f
#define DEFAULT_KD 0.1f

#endif  // CONFIG_H
