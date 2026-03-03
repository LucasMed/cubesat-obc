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
  #include "pico/stdlib.h"
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
  (void)printf("CSP: Initializing stack...\n");

  // 1. Init CSP
  csp_init();

  // 2. Setup UART configuration for KISS
  csp_usart_conf_t conf = {
      .device = "uart1", .baudrate = 115200, .databits = 8, .stopbits = 1, .paritysetting = 0};

  // 3. Add KISS interface
  csp_iface_t *kiss_iface = NULL;
  int res = csp_usart_open_and_add_kiss_interface(&conf, "KISS", OBC_ADDRESS, &kiss_iface);
  if (res != CSP_ERR_NONE)
  {
    (void)printf("CSP ERROR: Failed to add KISS interface (%d)\n", res);
    return;
  }

  // 4. Set routing table
  // Route for address 1 (Ground Station) goes via KISS interface
  char rtable[64];
  (void)snprintf(rtable, sizeof(rtable), "%d/255 KISS", GN_ADDRESS);
  (void)csp_rtable_load(rtable);

  (void)printf("CSP: Interface KISS added @ address %d\n", OBC_ADDRESS);
  (void)printf("CSP: Route to GN (%d) configured via KISS\n", GN_ADDRESS);

#ifdef PICO_BUILD
  // 5. Start the CSP router task.
  //    libcsp 2.x has no built-in background router — csp_route_work() must
  //    be called from application code.  Use priority tskIDLE_PRIORITY + 5
  //    so it runs above normal OBC tasks but below the watchdog.
  xTaskCreate(vCspRouterTask, "CSPRouter", 512, NULL, tskIDLE_PRIORITY + 5, NULL);
  (void)printf("CSP: Router task started\n");
#endif
}
