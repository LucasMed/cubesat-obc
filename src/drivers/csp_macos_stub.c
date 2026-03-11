/**
 * @file csp_macos_stub.c
 * @brief Minimal stubs for libcsp USART drivers on macOS host.
 */

#include <csp/drivers/usart.h>
#include <stddef.h>

void csp_usart_lock(void *driver_data) {
    (void)driver_data;
}

void csp_usart_unlock(void *driver_data) {
    (void)driver_data;
}

int csp_usart_write(void *driver_data, const void *data, size_t data_len) {
    (void)driver_data;
    (void)data;
    return (int)data_len;
}

int csp_usart_read(void *driver_data, void *buf, size_t buf_len, int timeout) {
    (void)driver_data;
    (void)buf;
    (void)timeout;
    return 0;
}
