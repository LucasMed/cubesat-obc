/**
 * @file test_fdir_isr.c
 * @brief Verification of ISR-safe fault signaling mechanism (Phase 2).
 *
 * This test verifies that:
 *  1. fault_report() called with CRITICAL level notifies HM task.
 *  2. The notification uses task notifications (xTaskNotifyFromISR).
 *  3. In host simulation, this correctly sets bits in a mocked handle.
 */

#include "fault_manager.h"
#include "fault_ids.h"
#include "flight_mode.h"
#include "FreeRTOS.h"
#include "task.h"
#include "config.h"

#include <stdio.h>
#include <stdbool.h>

/* ---- Test Mocks --------------------------------------------------------- */

static int g_failures = 0;
static bool g_force_safe_called = false;

/* Mock for fmm_force_safe */
void fmm_force_safe(void)
{
  g_force_safe_called = true;
}

#define CHECK(cond, msg)                                                                           \
  do                                                                                               \
  {                                                                                                \
    if (!(cond))                                                                                   \
    {                                                                                              \
      printf("  FAIL [%s:%d] %s\n", __func__, __LINE__, msg);                                      \
      g_failures++;                                                                                \
    }                                                                                              \
  } while (0)

/* ---- Tests -------------------------------------------------------------- */

/**
 * T-FDIR-01: Direct notification via task handle.
 * Verifies that registering a task handle and reporting a CRITICAL fault
 * triggers the notification bits.
 */
static void test_notification_flow(void)
{
  fault_manager_init();
  g_force_safe_called = false;

  uint32_t hm_notify_val = 0;
  /* Use pointer to our mock value as the TaskHandle_t */
  fault_manager_set_hm_task_handle(&hm_notify_val);

  /* Raise critical fault */
  fault_report(FAULT_WDT_KICK_MISSED, FAULT_LEVEL_CRITICAL);

  /* Verify bit was set by xTaskNotifyFromISR stub */
  CHECK(hm_notify_val & HM_NOTIFY_FAULT_CRITICAL, "Notification bit must be set in HM task handle");
  
  /* Verify direct fmm_force_safe was NOT called (handled by task instead) */
  CHECK(!g_force_safe_called, "fmm_force_safe must NOT be called directly when HM task is registered");

  printf("  PASS T-FDIR-01 notification flow (ISR-safe)\n");
}

/**
 * T-FDIR-02: Fallback to direct call.
 * Verifies that if no HM task is registered, it still triggers safe mode
 * via direct call (legacy/test behavior).
 */
static void test_fallback_flow(void)
{
  fault_manager_init();
  g_force_safe_called = false;

  /* Ensure handle is NULL */
  fault_manager_set_hm_task_handle(NULL);

  /* Raise critical fault */
  fault_report(FAULT_WDT_KICK_MISSED, FAULT_LEVEL_CRITICAL);

  /* Verify direct call */
  CHECK(g_force_safe_called, "fmm_force_safe must be called directly if no HM task registered");

  printf("  PASS T-FDIR-02 fallback flow (Direct call)\n");
}

int main(void)
{
  printf("=== FDIR ISR-safe signaling tests (Phase 2) ===\n");

  test_notification_flow();
  test_fallback_flow();

  if (g_failures == 0)
  {
    printf("ALL FDIR ISR TESTS PASSED\n");
    return 0;
  }
  printf("%d FDIR ISR TEST(S) FAILED\n", g_failures);
  return 1;
}
