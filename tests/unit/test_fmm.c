/**
 * @file test_fmm.c
 * @brief PR-3 gate: Flight Mode Manager correctness checks.
 *
 * Tests:
 *   1.  init() sets mode to FM_BOOT
 *   2.  Allowed transitions from FM_BOOT
 *   3.  Allowed transitions from FM_SAFE
 *   4.  Allowed transitions from FM_DETUMBLE
 *   5.  Allowed transitions from FM_NOMINAL
 *   6.  Allowed transitions from FM_DIAGNOSTIC
 *   7.  Disallowed transitions return FMM_ERR_NOT_ALLOWED
 *   8.  Same-mode request returns FMM_OK (no-op)
 *   9.  Invalid target (>= FM_COUNT) returns FMM_ERR_INVALID
 *   10. fmm_force_safe() reaches FM_SAFE from any mode
 *   11. Transition to FM_SAFE always allowed (even through matrix)
 *   12. fmm_get_mode() agrees with DLA data_layer_get_flight_mode()
 *   13. FAULT_LEVEL_CRITICAL blocks non-safe transitions (via stub override)
 *
 * Note: fault_get_highest_level() is stubbed here to test fault-blocking
 * behaviour without requiring the full fault_manager implementation (PR-4).
 */

#include "../../include/data_layer.h"
#include "../../include/fault_manager.h"
#include "../../include/flight_mode.h"

#include <stdbool.h>
#include <stdio.h>

/* ------------------------------------------------------------------ */
/* Fault-level stub control                                            */
/* ------------------------------------------------------------------ */

/*
 * The test controls the simulated fault level through this variable.
 * The override of fault_get_highest_level() below replaces the weak
 * symbol in flight_mode_manager.c only in this translation unit's
 * link step — giving us full control without needing the real FMM lib.
 *
 * HOWEVER: clang/gcc weak symbol overriding requires the non-weak
 * definition to appear in a *different* TU from the weak one (which it
 * does: flight_mode_manager.c has the weak version, this .c has the
 * strong override).
 */
static fault_level_t g_sim_fault_level = FAULT_LEVEL_NONE;

fault_level_t fault_get_highest_level(void)
{
  return g_sim_fault_level;
}

/* ------------------------------------------------------------------ */
/* Test helpers                                                        */
/* ------------------------------------------------------------------ */

static int g_failures = 0;

#define CHECK(cond, msg)                                                                           \
  do                                                                                               \
  {                                                                                                \
    if (!(cond))                                                                                   \
    {                                                                                              \
      printf("FAIL [%s:%d]: %s\n", __FILE__, __LINE__, (msg));                                     \
      g_failures++;                                                                                \
    }                                                                                              \
  } while (0)

/* Force FMM into a specific mode via data_layer directly (test helper) */
static void force_mode(flight_mode_t m)
{
  data_layer_init();
  data_layer_set_flight_mode(m);
}

/* ------------------------------------------------------------------ */
/* Test 1: init sets FM_BOOT                                           */
/* ------------------------------------------------------------------ */

static void test_init(void)
{
  data_layer_init();
  flight_mode_manager_init();
  CHECK(fmm_get_mode() == FM_BOOT, "init: mode must be FM_BOOT");
  CHECK(data_layer_get_flight_mode() == FM_BOOT, "init: DLA must agree");
  printf("test_init: OK\n");
}

/* ------------------------------------------------------------------ */
/* Test 2: transitions from FM_BOOT                                    */
/* ------------------------------------------------------------------ */

static void test_from_boot(void)
{
  fmm_result_t r;

  force_mode(FM_BOOT);
  r = fmm_request_transition(FM_SAFE);
  CHECK(r == FMM_OK, "BOOT->SAFE must be OK");
  CHECK(fmm_get_mode() == FM_SAFE, "BOOT->SAFE: mode must be FM_SAFE");

  force_mode(FM_BOOT);
  r = fmm_request_transition(FM_DETUMBLE);
  CHECK(r == FMM_OK, "BOOT->DETUMBLE must be OK");

  force_mode(FM_BOOT);
  r = fmm_request_transition(FM_NOMINAL);
  CHECK(r == FMM_ERR_NOT_ALLOWED, "BOOT->NOMINAL must be NOT_ALLOWED");

  force_mode(FM_BOOT);
  r = fmm_request_transition(FM_DIAGNOSTIC);
  CHECK(r == FMM_ERR_NOT_ALLOWED, "BOOT->DIAGNOSTIC must be NOT_ALLOWED");

  force_mode(FM_BOOT);
  r = fmm_request_transition(FM_PAYLOAD);
  CHECK(r == FMM_ERR_NOT_ALLOWED, "BOOT->PAYLOAD must be NOT_ALLOWED");

  printf("test_from_boot: OK\n");
}

/* ------------------------------------------------------------------ */
/* Test 3: transitions from FM_SAFE                                    */
/* ------------------------------------------------------------------ */

static void test_from_safe(void)
{
  fmm_result_t r;

  force_mode(FM_SAFE);
  r = fmm_request_transition(FM_DETUMBLE);
  CHECK(r == FMM_OK, "SAFE->DETUMBLE must be OK");

  force_mode(FM_SAFE);
  r = fmm_request_transition(FM_NOMINAL);
  CHECK(r == FMM_ERR_NOT_ALLOWED, "SAFE->NOMINAL must be NOT_ALLOWED");

  force_mode(FM_SAFE);
  r = fmm_request_transition(FM_DIAGNOSTIC);
  CHECK(r == FMM_ERR_NOT_ALLOWED, "SAFE->DIAGNOSTIC must be NOT_ALLOWED");

  force_mode(FM_SAFE);
  r = fmm_request_transition(FM_BOOT);
  CHECK(r == FMM_ERR_NOT_ALLOWED, "SAFE->BOOT must be NOT_ALLOWED");

  force_mode(FM_SAFE);
  r = fmm_request_transition(FM_PAYLOAD);
  CHECK(r == FMM_ERR_NOT_ALLOWED, "SAFE->PAYLOAD must be NOT_ALLOWED");

  printf("test_from_safe: OK\n");
}

/* ------------------------------------------------------------------ */
/* Test 4: transitions from FM_DETUMBLE                               */
/* ------------------------------------------------------------------ */

static void test_from_detumble(void)
{
  fmm_result_t r;

  force_mode(FM_DETUMBLE);
  r = fmm_request_transition(FM_SAFE);
  CHECK(r == FMM_OK, "DETUMBLE->SAFE must be OK");

  force_mode(FM_DETUMBLE);
  r = fmm_request_transition(FM_NOMINAL);
  CHECK(r == FMM_OK, "DETUMBLE->NOMINAL must be OK");

  force_mode(FM_DETUMBLE);
  r = fmm_request_transition(FM_DIAGNOSTIC);
  CHECK(r == FMM_ERR_NOT_ALLOWED, "DETUMBLE->DIAGNOSTIC must be NOT_ALLOWED");

  force_mode(FM_DETUMBLE);
  r = fmm_request_transition(FM_BOOT);
  CHECK(r == FMM_ERR_NOT_ALLOWED, "DETUMBLE->BOOT must be NOT_ALLOWED");

  force_mode(FM_DETUMBLE);
  r = fmm_request_transition(FM_PAYLOAD);
  CHECK(r == FMM_ERR_NOT_ALLOWED, "DETUMBLE->PAYLOAD must be NOT_ALLOWED");

  printf("test_from_detumble: OK\n");
}

/* ------------------------------------------------------------------ */
/* Test 5: transitions from FM_NOMINAL                                 */
/* ------------------------------------------------------------------ */

static void test_from_nominal(void)
{
  fmm_result_t r;

  force_mode(FM_NOMINAL);
  r = fmm_request_transition(FM_SAFE);
  CHECK(r == FMM_OK, "NOMINAL->SAFE must be OK");

  force_mode(FM_NOMINAL);
  r = fmm_request_transition(FM_DETUMBLE);
  CHECK(r == FMM_OK, "NOMINAL->DETUMBLE must be OK");

  force_mode(FM_NOMINAL);
  r = fmm_request_transition(FM_DIAGNOSTIC);
  CHECK(r == FMM_OK, "NOMINAL->DIAGNOSTIC must be OK");

  force_mode(FM_NOMINAL);
  r = fmm_request_transition(FM_BOOT);
  CHECK(r == FMM_ERR_NOT_ALLOWED, "NOMINAL->BOOT must be NOT_ALLOWED");

  force_mode(FM_NOMINAL);
  r = fmm_request_transition(FM_PAYLOAD);
  CHECK(r == FMM_OK, "NOMINAL->PAYLOAD must be OK");

  printf("test_from_nominal: OK\n");
}

/* ------------------------------------------------------------------ */
/* Test 6: transitions from FM_DIAGNOSTIC                             */
/* ------------------------------------------------------------------ */

static void test_from_diagnostic(void)
{
  fmm_result_t r;

  force_mode(FM_DIAGNOSTIC);
  r = fmm_request_transition(FM_SAFE);
  CHECK(r == FMM_OK, "DIAGNOSTIC->SAFE must be OK");

  force_mode(FM_DIAGNOSTIC);
  r = fmm_request_transition(FM_NOMINAL);
  CHECK(r == FMM_OK, "DIAGNOSTIC->NOMINAL must be OK");

  force_mode(FM_DIAGNOSTIC);
  r = fmm_request_transition(FM_DETUMBLE);
  CHECK(r == FMM_ERR_NOT_ALLOWED, "DIAGNOSTIC->DETUMBLE must be NOT_ALLOWED");

  force_mode(FM_DIAGNOSTIC);
  r = fmm_request_transition(FM_BOOT);
  CHECK(r == FMM_ERR_NOT_ALLOWED, "DIAGNOSTIC->BOOT must be NOT_ALLOWED");

  force_mode(FM_DIAGNOSTIC);
  r = fmm_request_transition(FM_PAYLOAD);
  CHECK(r == FMM_ERR_NOT_ALLOWED, "DIAGNOSTIC->PAYLOAD must be NOT_ALLOWED");

  printf("test_from_diagnostic: OK\n");
}

/* ------------------------------------------------------------------ */
/* Test 6a: transitions from FM_PAYLOAD                                */
/* ------------------------------------------------------------------ */

static void test_from_payload(void)
{
  fmm_result_t r;

  force_mode(FM_PAYLOAD);
  r = fmm_request_transition(FM_SAFE);
  CHECK(r == FMM_OK, "PAYLOAD->SAFE must be OK");

  force_mode(FM_PAYLOAD);
  r = fmm_request_transition(FM_NOMINAL);
  CHECK(r == FMM_OK, "PAYLOAD->NOMINAL must be OK");

  force_mode(FM_PAYLOAD);
  r = fmm_request_transition(FM_DETUMBLE);
  CHECK(r == FMM_ERR_NOT_ALLOWED, "PAYLOAD->DETUMBLE must be NOT_ALLOWED");

  force_mode(FM_PAYLOAD);
  r = fmm_request_transition(FM_DIAGNOSTIC);
  CHECK(r == FMM_ERR_NOT_ALLOWED, "PAYLOAD->DIAGNOSTIC must be NOT_ALLOWED");

  force_mode(FM_PAYLOAD);
  r = fmm_request_transition(FM_BOOT);
  CHECK(r == FMM_ERR_NOT_ALLOWED, "PAYLOAD->BOOT must be NOT_ALLOWED");

  printf("test_from_payload: OK\n");
}

/* ------------------------------------------------------------------ */
/* Test 7: same-mode request is a no-op (FMM_OK)                      */
/* ------------------------------------------------------------------ */

static void test_same_mode(void)
{
  flight_mode_t modes[] = {FM_BOOT, FM_SAFE, FM_DETUMBLE, FM_NOMINAL, FM_DIAGNOSTIC, FM_PAYLOAD};
  for (int i = 0; i < (int)(sizeof(modes) / sizeof(modes[0])); i++)
  {
    force_mode(modes[i]);
    fmm_result_t r = fmm_request_transition(modes[i]);
    CHECK(r == FMM_OK, "same-mode request must return FMM_OK");
    CHECK(fmm_get_mode() == modes[i], "mode must not change on same-mode request");
  }
  printf("test_same_mode: OK\n");
}

/* ------------------------------------------------------------------ */
/* Test 8: invalid target returns FMM_ERR_INVALID                     */
/* ------------------------------------------------------------------ */

static void test_invalid_target(void)
{
  force_mode(FM_BOOT);
  fmm_result_t r = fmm_request_transition(FM_COUNT); /* sentinel */
  CHECK(r == FMM_ERR_INVALID, "FM_COUNT must return FMM_ERR_INVALID");
  r = fmm_request_transition((flight_mode_t)99);
  CHECK(r == FMM_ERR_INVALID, "out-of-range mode must return FMM_ERR_INVALID");
  printf("test_invalid_target: OK\n");
}

/* ------------------------------------------------------------------ */
/* Test 9: fmm_force_safe() works from every mode                     */
/* ------------------------------------------------------------------ */

static void test_force_safe(void)
{
  flight_mode_t modes[] = {FM_BOOT, FM_DETUMBLE, FM_NOMINAL, FM_DIAGNOSTIC, FM_PAYLOAD};
  for (int i = 0; i < (int)(sizeof(modes) / sizeof(modes[0])); i++)
  {
    force_mode(modes[i]);
    fmm_force_safe();
    CHECK(fmm_get_mode() == FM_SAFE, "force_safe must set FM_SAFE from any mode");
    CHECK(data_layer_get_flight_mode() == FM_SAFE, "DLA must reflect FM_SAFE after force_safe");
  }
  printf("test_force_safe: OK\n");
}

/* ------------------------------------------------------------------ */
/* Test 10: FAULT_LEVEL_CRITICAL blocks non-safe transitions          */
/* ------------------------------------------------------------------ */

static void test_fault_block(void)
{
  /* Simulate CRITICAL fault */
  g_sim_fault_level = FAULT_LEVEL_CRITICAL;

  /* Non-safe transition must be blocked */
  force_mode(FM_SAFE);
  fmm_result_t r = fmm_request_transition(FM_DETUMBLE);
  CHECK(r == FMM_ERR_FAULT_BLOCK, "CRITICAL fault must block normal transition");

  force_mode(FM_DETUMBLE);
  r = fmm_request_transition(FM_NOMINAL);
  CHECK(r == FMM_ERR_FAULT_BLOCK, "CRITICAL fault must block DETUMBLE->NOMINAL");

  /* Transition TO FM_SAFE must NOT be blocked even with CRITICAL fault */
  force_mode(FM_NOMINAL);
  r = fmm_request_transition(FM_SAFE);
  CHECK(r == FMM_OK, "transition to FM_SAFE must always be allowed");

  /* fmm_force_safe() must always work too */
  force_mode(FM_NOMINAL);
  fmm_force_safe();
  CHECK(fmm_get_mode() == FM_SAFE, "force_safe must work under CRITICAL fault");

  /* Non-critical fault must NOT block transitions */
  g_sim_fault_level = FAULT_LEVEL_ERROR;
  force_mode(FM_SAFE);
  r = fmm_request_transition(FM_DETUMBLE);
  CHECK(r == FMM_OK, "ERROR-level fault must NOT block transitions");

  /* Restore */
  g_sim_fault_level = FAULT_LEVEL_NONE;
  printf("test_fault_block: OK\n");
}

/* ------------------------------------------------------------------ */
/* Test 11: fmm_mode_name()                                           */
/* ------------------------------------------------------------------ */

static void test_mode_name(void)
{
  /* Must return non-NULL, non-empty for all valid modes */
  for (flight_mode_t m = FM_BOOT; m < FM_COUNT; m++)
  {
    const char *name = fmm_mode_name(m);
    CHECK(name != NULL, "fmm_mode_name must not return NULL");
    if (name == NULL)
    {
      continue;
    } /* guard: CHECK does not abort */
    CHECK(name[0] != '\0', "fmm_mode_name must not return empty string");
  }
  /* Out-of-range must return a safe fallback string */
  const char *unk = fmm_mode_name(FM_COUNT);
  CHECK(unk != NULL, "fmm_mode_name(FM_COUNT) must not return NULL");
  printf("test_mode_name: OK\n");
}

/* ------------------------------------------------------------------ */
/* Entry point                                                         */
/* ------------------------------------------------------------------ */

int main(void)
{
  test_init();
  test_from_boot();
  test_from_safe();
  test_from_detumble();
  test_from_nominal();
  test_from_diagnostic();
  test_from_payload();
  test_same_mode();
  test_invalid_target();
  test_force_safe();
  test_fault_block();
  test_mode_name();

  if (g_failures == 0)
  {
    printf("All FMM checks passed.\n");
    return 0;
  }
  printf("%d FMM check(s) FAILED.\n", g_failures);
  return 1;
}
