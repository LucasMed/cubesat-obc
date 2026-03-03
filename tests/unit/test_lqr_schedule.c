/* test_lqr_schedule.c — Unit tests for mode-scheduled LQR gains (PR-23)
 *
 * T-LQRS-01: FM_NOMINAL  → default gains (ωn = 10 rad/s).
 *            k_att_roll  = 1.00, k_rate_roll  = 0.20
 *            k_att_yaw   = 0.50, k_rate_yaw   = 0.10
 * T-LQRS-02: FM_DETUMBLE → high-bandwidth gains (ωn = 30 rad/s).
 *            k_att_roll  = 9.00, k_rate_roll  = 0.60
 *            k_att_yaw   = 4.50, k_rate_yaw   = 0.30
 *            All DETUMBLE gains must be strictly larger than NOMINAL.
 * T-LQRS-03: Unknown / unhandled mode → fallback to nominal gain set.
 *            FM_BOOT, FM_SAFE, and out-of-range value all give K_NOMINAL.
 *
 * Only lqr.c and lqr_schedule.c are compiled here; no RTOS dependency.
 */

#include "lqr.h"
#include "lqr_schedule.h"

#include <math.h>
#include <stdio.h>

/* ---- Test helpers ------------------------------------------------------- */
static int g_failures = 0;

#define CHECK(cond, msg)                                                                           \
  do                                                                                               \
  {                                                                                                \
    if (!(cond))                                                                                   \
    {                                                                                              \
      printf("  FAIL [%s:%d] %s\n", __func__, __LINE__, (msg));                                    \
      g_failures++;                                                                                \
    }                                                                                              \
  } while (0)

#define FPEQ(a, b) (fabsf((a) - (b)) < 1e-6f)

/* ========================================================================
 * T-LQRS-01  FM_NOMINAL → default critically-damped gains (ωn = 10 rad/s)
 * ======================================================================== */
static void test_nominal_gains(void)
{
  lqr_t lqr;
  lqr_init(&lqr);

  /* Dirty the matrix so we can confirm apply() actually writes it. */
  for (int i = 0; i < LQR_M; i++)
  {
    for (int j = 0; j < LQR_N; j++)
    {
      lqr.K[i][j] = 999.0f;
    }
  }

  lqr_schedule_apply(&lqr, FM_NOMINAL);

  /* Diagonal attitude gains */
  CHECK(FPEQ(lqr.K[0][0], 1.00f), "FM_NOMINAL: K_roll_att must be 1.00");
  CHECK(FPEQ(lqr.K[1][1], 1.00f), "FM_NOMINAL: K_pitch_att must be 1.00");
  CHECK(FPEQ(lqr.K[2][2], 0.50f), "FM_NOMINAL: K_yaw_att must be 0.50");

  /* Diagonal rate gains */
  CHECK(FPEQ(lqr.K[0][3], 0.20f), "FM_NOMINAL: K_roll_rate must be 0.20");
  CHECK(FPEQ(lqr.K[1][4], 0.20f), "FM_NOMINAL: K_pitch_rate must be 0.20");
  CHECK(FPEQ(lqr.K[2][5], 0.10f), "FM_NOMINAL: K_yaw_rate must be 0.10");

  /* Off-diagonal must be zero */
  CHECK(FPEQ(lqr.K[0][1], 0.00f), "FM_NOMINAL: K[0][1] must be 0");
  CHECK(FPEQ(lqr.K[0][2], 0.00f), "FM_NOMINAL: K[0][2] must be 0");

  printf("  PASS T-LQRS-01 FM_NOMINAL applies default gains\n");
}

/* ========================================================================
 * T-LQRS-02  FM_DETUMBLE → high-bandwidth gains (ωn = 30 rad/s)
 * ======================================================================== */
static void test_detumble_gains(void)
{
  lqr_t lqr;
  lqr_init(&lqr);

  lqr_schedule_apply(&lqr, FM_DETUMBLE);

  /* Diagonal attitude gains (k_att = I·ωn² with ωn=30) */
  CHECK(FPEQ(lqr.K[0][0], 9.00f), "FM_DETUMBLE: K_roll_att must be 9.00");
  CHECK(FPEQ(lqr.K[1][1], 9.00f), "FM_DETUMBLE: K_pitch_att must be 9.00");
  CHECK(FPEQ(lqr.K[2][2], 4.50f), "FM_DETUMBLE: K_yaw_att must be 4.50");

  /* Diagonal rate gains (k_rate = I·2·ζ·ωn with ωn=30) */
  CHECK(FPEQ(lqr.K[0][3], 0.60f), "FM_DETUMBLE: K_roll_rate must be 0.60");
  CHECK(FPEQ(lqr.K[1][4], 0.60f), "FM_DETUMBLE: K_pitch_rate must be 0.60");
  CHECK(FPEQ(lqr.K[2][5], 0.30f), "FM_DETUMBLE: K_yaw_rate must be 0.30");

  /* DETUMBLE gains must be strictly larger than NOMINAL gains */
  lqr_t lqr_nom;
  lqr_init(&lqr_nom);
  lqr_schedule_apply(&lqr_nom, FM_NOMINAL);

  CHECK(lqr.K[0][0] > lqr_nom.K[0][0], "FM_DETUMBLE k_att_roll > FM_NOMINAL");
  CHECK(lqr.K[2][2] > lqr_nom.K[2][2], "FM_DETUMBLE k_att_yaw > FM_NOMINAL");
  CHECK(lqr.K[0][3] > lqr_nom.K[0][3], "FM_DETUMBLE k_rate_roll > FM_NOMINAL");
  CHECK(lqr.K[2][5] > lqr_nom.K[2][5], "FM_DETUMBLE k_rate_yaw > FM_NOMINAL");

  printf("  PASS T-LQRS-02 FM_DETUMBLE applies high-bandwidth gains\n");
}

/* ========================================================================
 * T-LQRS-03  Unhandled modes fall back to FM_NOMINAL gain set
 * ======================================================================== */
static void test_fallback_gains(void)
{
  lqr_t lqr_nom, lqr_test;

  lqr_init(&lqr_nom);
  lqr_schedule_apply(&lqr_nom, FM_NOMINAL);

  /* FM_BOOT */
  lqr_init(&lqr_test);
  lqr_schedule_apply(&lqr_test, FM_BOOT);
  for (int i = 0; i < LQR_M; i++)
  {
    for (int j = 0; j < LQR_N; j++)
    {
      CHECK(FPEQ(lqr_test.K[i][j], lqr_nom.K[i][j]), "FM_BOOT must fall back to nominal");
    }
  }

  /* FM_SAFE */
  lqr_init(&lqr_test);
  lqr_schedule_apply(&lqr_test, FM_SAFE);
  for (int i = 0; i < LQR_M; i++)
  {
    for (int j = 0; j < LQR_N; j++)
    {
      CHECK(FPEQ(lqr_test.K[i][j], lqr_nom.K[i][j]), "FM_SAFE must fall back to nominal");
    }
  }

  /* Out-of-range sentinel FM_COUNT */
  lqr_init(&lqr_test);
  lqr_schedule_apply(&lqr_test, FM_COUNT);
  for (int i = 0; i < LQR_M; i++)
  {
    for (int j = 0; j < LQR_N; j++)
    {
      CHECK(FPEQ(lqr_test.K[i][j], lqr_nom.K[i][j]), "FM_COUNT must fall back to nominal");
    }
  }

  printf("  PASS T-LQRS-03 unhandled modes fall back to nominal gains\n");
}

/* ========================================================================
 * main
 * ======================================================================== */
int main(void)
{
  printf("=== LQR gain scheduling tests (PR-23) ===\n");

  test_nominal_gains();
  test_detumble_gains();
  test_fallback_gains();

  if (g_failures == 0)
  {
    printf("ALL LQR SCHEDULE TESTS PASSED\n");
    return 0;
  }

  printf("%d TEST(S) FAILED\n", g_failures);
  return 1;
}
