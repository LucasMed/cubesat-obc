// This file is compiled only during unit testing.  It pulls in the real
// command_task.c implementation but tells it to redirect CSP calls to the
// mocks defined in the test harness (see test_command.c for definitions).

// Forward declarations of types used in the mock prototypes.
#include <stddef.h>       /* for size_t */
#include <stdint.h>       /* for uint16_t, uint32_t */
typedef struct csp_conn_s   csp_conn_t;
typedef struct csp_packet_s csp_packet_t;
typedef struct csp_socket_s csp_socket_t;

// Forward declarations of the mock functions so that the macros applied in
// command_task.c have proper prototypes and avoid implicit-declaration
// warnings/errors.
int mock_csp_buffer_free(void *packet);
csp_packet_t *mock_csp_buffer_get(size_t size);
void mock_csp_send(csp_conn_t *conn, csp_packet_t *packet);
int mock_csp_conn_src(const csp_conn_t *conn);
int mock_csp_bind(csp_socket_t *sock, uint16_t port);
int mock_csp_listen(csp_socket_t *sock, size_t backlog);
csp_conn_t *mock_csp_accept(csp_socket_t *sock, uint32_t timeout);
csp_packet_t *mock_csp_read(csp_conn_t *conn, uint32_t timeout);
void mock_csp_close(csp_conn_t *conn);
void gps_reset_stats(void);

/* Pull in FreeRTOS types first so the include guard (FREERTOS_H) is set.
 * Then undefine the vTaskDelay no-op macro — when command_task.c later
 * hits its own #include "FreeRTOS.h" the guard prevents re-inclusion, so
 * vTaskDelay stays undefined as a macro and becomes a real function call
 * that links to the mock defined in test_command.c. */
#include "FreeRTOS.h"
#undef vTaskDelay
/* Forward declaration so the implicit-function-declaration warning is
 * suppressed; the real body lives in test_command.c. */
extern void vTaskDelay(uint32_t ticks);

/* ------------------------------------------------------------------ */
/*  PICO_BUILD stubs — allow process_text_command() to be compiled     */
/*  on the host for comprehensive regression testing.                  */
/* ------------------------------------------------------------------ */
#include "pico_stubs.h"

#define CSP_MOCK

#include "payload_task.h"
#undef xTaskGetHandle
extern TaskHandle_t xTaskGetHandle(const char *pcName);

#undef xTaskNotify
#include "FreeRTOS.h"
extern BaseType_t xTaskNotify_Stub(TaskHandle_t xTask, uint32_t ulValue, eNotifyAction eAction);
#define xTaskNotify xTaskNotify_Stub

/* Define PICO_BUILD so that process_text_command() gets compiled
   (the uart1_listen() function also becomes available but is not called
    by the tests — the linker still needs its symbols; they are satisfied
    by the pico_stubs.h / test_command.c stubs). */
#define PICO_BUILD

#include "../../src/tasks/command_task.c"

/* Test wrapper for text commands — delegates to the REAL
 * process_text_command() from command_task.c now that PICO_BUILD
 * makes it available in host mode. */
void test_run_text_command(const char *cmd)
{
  process_text_command(cmd);
}
