// This file is compiled only during unit testing.  It pulls in the real
// command_task.c implementation but tells it to redirect CSP calls to the
// mocks defined in the test harness (see test_command.c for definitions).

// Forward declarations of types used in the mock prototypes.
#include <stddef.h> /* for size_t */
typedef struct csp_conn_s csp_conn_t;
typedef struct csp_packet_s csp_packet_t;

// Forward declarations of the mock functions so that the macros applied in
// command_task.c have proper prototypes and avoid implicit-declaration
// warnings/errors.
int mock_csp_buffer_free(void *packet);
csp_packet_t *mock_csp_buffer_get(size_t size);
void mock_csp_send(csp_conn_t *conn, csp_packet_t *packet);
int mock_csp_conn_src(const csp_conn_t *conn);

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

#define CSP_MOCK
#include "../../src/tasks/command_task.c"
