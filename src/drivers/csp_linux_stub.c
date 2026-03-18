/**
 * @file csp_linux_stub.c
 * @brief Minimal stubs for libcsp USART drivers on Linux host.
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

int csp_usart_open(const csp_usart_conf_t *conf, csp_usart_callback_t rx_callback, void *user_data,
                   csp_usart_fd_t *fd)  // NOLINT(readability-non-const-parameter)
{
  (void)conf;
  (void)rx_callback;
  (void)user_data;
  (void)fd;
  return CSP_ERR_NONE;
}
