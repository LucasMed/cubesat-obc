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
#include <stddef.h> /* for size_t */
typedef struct csp_conn_s csp_conn_t;
typedef struct csp_packet_s csp_packet_t;

// Forward declarations of the mock functions
int mock_csp_buffer_free(void *packet);
csp_packet_t *mock_csp_buffer_get(size_t size);
void mock_csp_send(csp_conn_t *conn, csp_packet_t *packet);
int mock_csp_conn_src(const csp_conn_t *conn);
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

#define CSP_MOCK

#include "payload_task.h"
#undef xTaskGetHandle
extern TaskHandle_t xTaskGetHandle(const char *pcName);

#undef xTaskNotify
#include "FreeRTOS.h"
extern BaseType_t xTaskNotify_Stub(TaskHandle_t xTask, uint32_t ulValue, eNotifyAction eAction);
#define xTaskNotify xTaskNotify_Stub

#include "../../src/tasks/command_task.c"

/* Test wrapper — implements the text-command parsing for the subset of
 * commands exercised by the test suite (DEPLOY, DEPLOYCLEAR, MODE=).
 * This replicates the production logic from process_text_command() but
 * without the PICO_BUILD dependency, so the unit test can compile on
 * host.  Production code paths are identical — same API calls. */
void test_run_text_command(const char *cmd)
{
  if (strncmp(cmd, "DEPLOYCLEAR", 11) == 0)
  {
    data_layer_set_deploy_in_progress(false);
  }
  else if (strncmp(cmd, "DEPLOY", 6) == 0)
  {
    flight_mode_t m = fmm_get_mode();
    if (m == FM_BOOT || m == FM_SAFE)
    {
      fmm_result_t r = fmm_request_transition(FM_DETUMBLE);
      if (r == FMM_OK)
      {
        data_layer_set_deploy_in_progress(true);
      }
    }
    else if (m != FM_DETUMBLE)
    {
      /* Rejected from non-deployable modes */
    }
  }
  else if (strncmp(cmd, "MODE=", 5) == 0)
  {
    int mode = -1;
    const char *arg = cmd + 5;
    size_t arg_len = strlen(arg);
    if (arg_len > 0 && (arg[0] < '0' || arg[0] > '9'))
    {
      for (int i = 0; i < FM_COUNT; i++)
      {
        if (strcasecmp(arg, fmm_mode_name((flight_mode_t)i)) == 0)
        {
          mode = i;
          break;
        }
      }
    }
    else
    {
      mode = atoi(arg);
    }
    if (mode >= 0 && mode < FM_COUNT)
    {
      fmm_request_transition((flight_mode_t)mode);
    }
  }
}
