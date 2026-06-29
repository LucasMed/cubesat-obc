/**
 * @file test_pid.c
 * @brief Unit tests for PID controller (anti-windup clamping).
 *
 * Tests
 * -----
 *   T-PID-01 — pid_init sets default intg_limit
 *   T-PID-02 — integral does NOT exceed +intg_limit under sustained error
 *   T-PID-03 — integral does NOT exceed -intg_limit under sustained negative error
 *   T-PID-04 — integral within limits is NOT clamped
 *   T-PID-05 — custom intg_limit is respected
 *   T-PID-06 — pid_update produces finite output
 */

#include "../../include/pid.h"

#include <math.h>
#include <stdio.h>

/* ------------------------------------------------------------------ */
/* Test infrastructure                                                 */
/* ------------------------------------------------------------------ */

#define PASS(label) printf("  PASS %s\n", (label))
#define FAIL(label, msg)                                                                           \
  do                                                                                               \
  {                                                                                                \
    printf("  FAIL %s: %s\n", (label), (msg));                                                     \
    g_failures++;                                                                                  \
  } while (0)
#define CHECK(label, cond, msg)                                                                    \
  do                                                                                               \
  {                                                                                                \
    if (!(cond))                                                                                   \
    {                                                                                              \
      FAIL(label, msg);                                                                            \
    }                                                                                              \
    else                                                                                           \
    {                                                                                              \
      PASS(label);                                                                                 \
    }                                                                                              \
  } while (0)

static int g_failures = 0;

/* ------------------------------------------------------------------ */
/* T-PID-01: pid_init sets default intg_limit                          */
/* ------------------------------------------------------------------ */
static void test_init_sets_default_intg_limit(void)
{
  pid_ctrl_t p;
  pid_init(&p, 1.0f, 0.1f, 0.01f);

  CHECK("T-PID-01", p.intg_limit == 100.0f, "default intg_limit must be 100.0");
}

/* ------------------------------------------------------------------ */
/* T-PID-02: integral clamped to +intg_limit under sustained error      */
/* ------------------------------------------------------------------ */
static void test_integral_clamped_positive(void)
{
  pid_ctrl_t p;
  pid_init(&p, 1.0f, 0.1f, 0.01f);
  p.intg_limit = 10.0f;

  float dt = 0.1f;

  /* Run many iterations with a large positive error to wind up the integral */
  for (int i = 0; i < 200; i++)
  {
    (void)pid_update(&p, 50.0f, dt);
  }

  CHECK("T-PID-02", p.integral <= p.intg_limit,
        "integral must not exceed +intg_limit after sustained error");
  CHECK("T-PID-02", p.integral >= 0.0f,
        "integral must be non-negative after positive error");
}

/* ------------------------------------------------------------------ */
/* T-PID-03: integral clamped to -intg_limit under sustained negative   */
/* ------------------------------------------------------------------ */
static void test_integral_clamped_negative(void)
{
  pid_ctrl_t p;
  pid_init(&p, 1.0f, 0.1f, 0.01f);
  p.intg_limit = 10.0f;

  float dt = 0.1f;

  /* Run many iterations with a large negative error */
  for (int i = 0; i < 200; i++)
  {
    (void)pid_update(&p, -50.0f, dt);
  }

  CHECK("T-PID-03", p.integral >= -p.intg_limit,
        "integral must not exceed -intg_limit after sustained negative error");
  CHECK("T-PID-03", p.integral <= 0.0f,
        "integral must be non-positive after negative error");
}

/* ------------------------------------------------------------------ */
/* T-PID-04: integral within limits is NOT clamped                      */
/* ------------------------------------------------------------------ */
static void test_integral_within_limits_not_clamped(void)
{
  pid_ctrl_t p;
  pid_init(&p, 1.0f, 0.1f, 0.01f);
  p.intg_limit = 10.0f;

  float dt = 0.1f;
  float small_error = 5.0f;

  /* Running a moderate error for a few steps should keep integral < 10 */
  for (int i = 0; i < 10; i++)
  {
    (void)pid_update(&p, small_error, dt);
    /* After each update, integral should be within bounds */
    CHECK("T-PID-04", fabsf(p.integral) <= p.intg_limit,
          "integral must never exceed intg_limit");
  }
}

/* ------------------------------------------------------------------ */
/* T-PID-05: custom intg_limit is respected                             */
/* ------------------------------------------------------------------ */
static void test_custom_intg_limit(void)
{
  pid_ctrl_t p;
  pid_init(&p, 1.0f, 0.1f, 0.01f);

  /* Set a very tight limit */
  p.intg_limit = 0.5f;

  float dt = 0.1f;

  /* Large error should quickly hit the tight limit */
  for (int i = 0; i < 50; i++)
  {
    (void)pid_update(&p, 100.0f, dt);
  }

  CHECK("T-PID-05", p.integral <= 0.5f,
        "integral must be clamped at custom intg_limit=0.5");
  CHECK("T-PID-05", p.integral > 0.0f,
        "integral must be positive");

  /* Now test negative side */
  pid_init(&p, 1.0f, 0.1f, 0.01f);
  p.intg_limit = 0.5f;

  for (int i = 0; i < 50; i++)
  {
    (void)pid_update(&p, -100.0f, dt);
  }

  CHECK("T-PID-05", p.integral >= -0.5f,
        "integral must be clamped at custom intg_limit=-0.5");
  CHECK("T-PID-05", p.integral < 0.0f,
        "integral must be negative");
}

/* ------------------------------------------------------------------ */
/* T-PID-06: pid_update produces finite output                          */
/* ------------------------------------------------------------------ */
static void test_output_is_finite(void)
{
  pid_ctrl_t p;
  pid_init(&p, 1.0f, 0.1f, 0.01f);

  float dt = 0.1f;
  float out1 = pid_update(&p, 1.0f, dt);
  float out2 = pid_update(&p, 1.0f, dt);

  CHECK("T-PID-06", isfinite(out1), "first output must be finite");
  CHECK("T-PID-06", isfinite(out2), "second output must be finite");
}

/* ------------------------------------------------------------------ */
/* Main                                                                */
/* ------------------------------------------------------------------ */
int main(void)
{
  printf("=== PID controller unit tests ===\n");

  test_init_sets_default_intg_limit();
  test_integral_clamped_positive();
  test_integral_clamped_negative();
  test_integral_within_limits_not_clamped();
  test_custom_intg_limit();
  test_output_is_finite();

  if (g_failures == 0)
  {
    printf("ALL PID TESTS PASSED\n");
    return 0;
  }
  else
  {
    printf("SOME TESTS FAILED (%d failure(s))\n", g_failures);
    return 1;
  }
}
