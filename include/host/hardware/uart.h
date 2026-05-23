#ifndef _HOST_HARDWARE_UART_H
#define _HOST_HARDWARE_UART_H

#include <stdbool.h>
#include <stdint.h>

typedef struct uart_inst uart_inst_t;
extern uart_inst_t *const uart1;

bool uart_is_readable(uart_inst_t *uart);
char uart_getc(uart_inst_t *uart);

#endif
