/**
 * @file pico_stubs.h
 * @brief PICO_BUILD stub header for host-unit-testing text commands.
 *
 * When included BEFORE `command_task.c` (with PICO_BUILD defined), this
 * file preempts Pico‑SDK includes so that `process_text_command()` can be
 * compiled on the host.
 *
 * Every PICO‑only function called by `process_text_command()` is declared
 * here; implementations live in `test_command.c` (or this file for trivial
 * stubs).
 */
#ifndef PICO_STUBS_H
#define PICO_STUBS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* ------------------------------------------------------------------ */
/*  hardware/watchdog.h  — resolved via include/host/hardware/watchdog.h
    (include/host/ is now on the include path).  No blocking needed.    */
/* ------------------------------------------------------------------ */

/* ------------------------------------------------------------------ */
/*  hardware/uart.h  — Pico SDK UART HAL.  uart1_listen() calls
    uart_is_readable() and uart_getc() which the test never invokes but
    the compiler must see declarations for.                             */
/* ------------------------------------------------------------------ */
#define _HARDWARE_UART_H

typedef struct uart_inst uart_inst_t;
extern uart_inst_t *const uart1;

static inline bool uart_is_readable(uart_inst_t *uart)
{
  (void)uart;
  return false;
}

static inline char uart_getc(uart_inst_t *uart)
{
  (void)uart;
  return '\0';
}

/* ------------------------------------------------------------------ */
/*  pico/stdlib.h — Pico SDK standard library compat.  Not used by
    process_text_command() or uart1_listen().                           */
/* ------------------------------------------------------------------ */
#define _PICO_STDLIB_H_
#define _PICO_STDLIB_H

/* ------------------------------------------------------------------ */
/*  pico_pins.h — pin definitions, no platform types.  The include
    guard PICO_PINS_H is pre-defined so the compiler skips the file.    */
/* ------------------------------------------------------------------ */
#define PICO_PINS_H

/* ------------------------------------------------------------------ */
/*  ../drivers/uart/pico_usart.h — project UART wrapper.
    uart1_puts_safe / uart1_acquire_lock / etc. are stubbed in
    test_command.c.  The include passes through normally.               */
/* ------------------------------------------------------------------ */

#endif /* PICO_STUBS_H */
