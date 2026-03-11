/**
 * @file csp_macos_stub.c
 * @brief Minimal stubs for libcsp USART drivers on macOS host.
 */

#include <csp/drivers/usart.h>
#include <stddef.h>

void csp_usart_lock(void *driver_data)
{
  (void)driver_data;
}

void csp_usart_unlock(void *driver_data)
{
  (void)driver_data;
}

int csp_usart_write(csp_usart_fd_t fd, const void *data, size_t data_len)
{
  (void)fd;
  (void)data;
  return (int)data_len;
}
