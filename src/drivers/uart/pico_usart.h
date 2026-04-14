#ifndef PICO_USART_H
#define PICO_USART_H

/**
 * @file pico_usart.h
 * @brief Thread-safe UART1 functions for Pico OBC
 */

#ifdef PICO_BUILD

/**
 * @brief Thread-safe wrapper for uart_puts to UART1 (single atomic write)
 */
void uart1_puts_safe(const char *str);

/**
 * @brief Acquire UART1 lock for atomic multi-write operations
 * Use with uart1_write_unsafe() and uart1_release_lock()
 */
void uart1_acquire_lock(void);

/**
 * @brief Release UART1 lock
 */
void uart1_release_lock(void);

/**
 * @brief Write to UART1 WITHOUT acquiring lock (use within uart1_acquire_lock/uart1_release_lock)
 */
void uart1_write_unsafe(const char *str);

#endif  // PICO_BUILD

#endif  // PICO_USART_H
