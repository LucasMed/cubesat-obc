#include "comm_init.h"

#include <csp/csp.h>
#include <csp/drivers/usart.h>
#include <csp/interfaces/csp_if_kiss.h>
/* cppcheck-suppress misra-c2012-21.6 -- MISRA deviation: stdio printf
 * used for CSP stack debug output; removed in production linker script
 * under NDEBUG. See MISRA_DEVIATIONS.md §21.6-D1. */
#include <stdio.h>

#ifdef PICO_BUILD
  #include "FreeRTOS.h"
  #include "hardware/gpio.h"
  #include "hardware/uart.h"
  #include "pico/stdlib.h"
  #include "pico_pins.h"
  #include "task.h"
#endif

// OBC Address: 10
// Ground Station Address: 1
#define OBC_ADDRESS 10
#define GN_ADDRESS 1

#ifdef PICO_BUILD
/**
 * CSP router task — required by libcsp 2.x (no background router thread).
 * csp_route_work() dequeues one packet from the RX queue and forwards it
 * to the appropriate socket.  Must be called repeatedly; a 1 ms yield
 * keeps latency low without burning CPU.
 */
static void vCspRouterTask(void *pvParameters)
{
  (void)pvParameters;
  for (;;)
  {
    csp_route_work();
    vTaskDelay(pdMS_TO_TICKS(1));
  }
}
#endif

void comm_init(void)
{
  (void)printf("CSP: initializing...\r\n");
  (void)fflush(stdout);

  csp_init();

#ifndef PICO_BUILD
  /* ── Host build: physical UART1 via KISS ── */
  csp_usart_conf_t conf = {
      .device = "uart1", .baudrate = 115200, .databits = 8, .stopbits = 1, .paritysetting = 0};

  csp_iface_t *kiss_iface = NULL;
  int res = csp_usart_open_and_add_kiss_interface(&conf, "KISS", OBC_ADDRESS, &kiss_iface);
  if (res != CSP_ERR_NONE)
  {
    (void)printf("CSP ERROR: KISS interface failed (%d)\r\n", res);
    (void)fflush(stdout);
    return;
  }

  char rtable[64];
  (void)snprintf(rtable, sizeof(rtable), "%d/255 KISS", GN_ADDRESS);
  (void)csp_rtable_load(rtable);
  (void)printf("CSP: KISS @ addr %d, route GN(%d)\r\n", OBC_ADDRESS, GN_ADDRESS);
  (void)fflush(stdout);
#else
  /* ── Pico build: UART1 for HC-12 radio ── */
  uart_init(uart1, UART1_BAUD_RATE);
  gpio_set_function(UART1_TX_PIN, GPIO_FUNC_UART);
  gpio_set_function(UART1_RX_PIN, GPIO_FUNC_UART);
  uart_set_hw_flow(uart1, false, false);
  uart_set_fifo_enabled(uart1, true);

  csp_usart_conf_t conf = {.device = "uart1",
                           .baudrate = UART1_BAUD_RATE,
                           .databits = 8,
                           .stopbits = 1,
                           .paritysetting = 0};

  csp_iface_t *kiss_iface = NULL;
  int res = csp_usart_open_and_add_kiss_interface(&conf, "KISS", OBC_ADDRESS, &kiss_iface);
  if (res != CSP_ERR_NONE)
  {
    (void)printf("CSP ERROR: KISS interface failed (%d)\r\n", res);
    (void)fflush(stdout);
    return;
  }

  char rtable[64];
  (void)snprintf(rtable, sizeof(rtable), "%d/255 KISS", GN_ADDRESS);
  (void)csp_rtable_load(rtable);
  (void)printf("CSP: KISS @ UART1 (GPIO%d/%d), addr %d, route GN(%d)\r\n", UART1_TX_PIN,
               UART1_RX_PIN, OBC_ADDRESS, GN_ADDRESS);
  (void)fflush(stdout);
#endif

#ifdef PICO_BUILD
  /* Priority must be < configMAX_PRIORITIES (5). Use 3 — runs between
   * Telemetry/Command and idle, low enough not to starve other tasks.
   * 1024 words (4 KB): csp_route_work() + newlib + queue ops need >2 KB
   * on Cortex-M33. */
  xTaskCreate(vCspRouterTask, "CSPRouter", 1024, NULL, tskIDLE_PRIORITY + 3, NULL);
  (void)printf("CSP: router task OK\r\n");
  (void)fflush(stdout);
#endif
}
