/* test_momentum_dump.c — Unit tests for the momentum dump algorithm (PR-17)
 *
 * T-MTM-01: momentum_dump_init   — init completes, gain stored correctly
 * T-MTM-02: zero B field         — degenerate guard outputs zero dipole
 * T-MTM-03: known B and L_rw    — cross-product dipole matches analytic result
 * T-MTM-04: momentum_dump_needed — false below threshold, true above
 * T-MTM-05: gain scaling         — doubling k_dump doubles dipole magnitude
 *
 * No hardware dependency; momentum_dump.c is purely computational.
 */

#include "momentum_dump.h"

#include "config.h"

#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

/* ---- Test helpers ------------------------------------------------------- */
static int g_failures = 0;

#define CHECK(cond, msg)                                                                           \
  do                                                                                               \
  {                                                                                                \
    if (!(cond))                                                                                   \
    {                                                                                              \
      printf("  FAIL [%s:%d] %s\n", __func__, __LINE__, msg);                                      \
      g_failures++;                                                                                \
    }                                                                                              \
  } while (0)

/** Floating-point equality with a reasonable tolerance. */
#define FPEQ(a, b) (fabsf((a) - (b)) < 1e-5f)

/* ========================================================================
 * T-MTM-01  momentum_dump_init() stores the gain correctly
 * ======================================================================== */
static void test_mtm_init(void)
{
  momentum_dump_t md;
  memset(&md, 0xAB, sizeof(md)); /* poison memory */

  momentum_dump_init(&md, 0.05f);

  CHECK(FPEQ(md.k_dump, 0.05f), "k_dump must be stored as provided");
  printf("  PASS T-MTM-01 momentum_dump_init() stores gain\n");
}

/* ========================================================================
 * T-MTM-02  Zero (degenerate) B field → zero dipole command
 *
 * When |B| < B_EPSILON the algorithm must output a zero dipole rather
 * than dividing by a near-zero value.
 * ======================================================================== */
static void test_mtm_zero_b_field(void)
{
  momentum_dump_t md;
  momentum_dump_init(&md, 0.1f);

  float B[3] = {0.0f, 0.0f, 0.0f};
  float L_rw[3] = {0.1f, 0.2f, 0.3f};
  float dipole[3];

  momentum_dump_step(&md, B, L_rw, dipole);

  CHECK(FPEQ(dipole[0], 0.0f) && FPEQ(dipole[1], 0.0f) && FPEQ(dipole[2], 0.0f),
        "zero B field must produce zero dipole command");
  printf("  PASS T-MTM-02 zero B field -> zero dipole\n");
}

/* ========================================================================
 * T-MTM-03  Known B and L_rw → analytic cross-product result
 *
 * B  = {0, 0, 50} µT  →  B_hat = {0, 0, 1}
 * L  = {0.1, 0, 0}
 * B_hat × L = {0*0 - 1*0, 1*0.1 - 0*0, 0*0 - 0*0.1} = {0, 0.1, 0}
 * dipole = -k_dump * {0, 0.1, 0}  with k_dump = 0.1
 *        = {0, -0.01, 0}
 * ======================================================================== */
static void test_mtm_cross_product(void)
{
  momentum_dump_t md;
  momentum_dump_init(&md, 0.1f);

  float B[3] = {0.0f, 0.0f, 50.0f};
  float L_rw[3] = {0.1f, 0.0f, 0.0f};
  float dipole[3];

  momentum_dump_step(&md, B, L_rw, dipole);

  CHECK(FPEQ(dipole[0], 0.0f), "dipole[0] must be 0");
  CHECK(FPEQ(dipole[1], -0.01f), "dipole[1] must be -0.01");
  CHECK(FPEQ(dipole[2], 0.0f), "dipole[2] must be 0");
  printf("  PASS T-MTM-03 cross-product dipole matches analytic result\n");
}

/* ========================================================================
 * T-MTM-04  momentum_dump_needed() threshold logic
 * ======================================================================== */
static void test_mtm_needed(void)
{
  /* |L_rw| = sqrt(3) ≈ 1.732 */
  float L_above[3] = {1.0f, 1.0f, 1.0f};
  /* |L_rw| = sqrt(0.03) ≈ 0.173 */
  float L_below[3] = {0.1f, 0.1f, 0.1f};

  CHECK(momentum_dump_needed(L_above, 1.0f) == true,
        "needed() must return true when |L| > threshold");
  CHECK(momentum_dump_needed(L_below, 1.0f) == false,
        "needed() must return false when |L| < threshold");
  /* Exactly on the threshold (strictly greater): false */
  float L_exact[3] = {1.0f, 0.0f, 0.0f};
  CHECK(momentum_dump_needed(L_exact, 1.0f) == false,
        "needed() must return false when |L| == threshold (strictly greater required)");
  printf("  PASS T-MTM-04 momentum_dump_needed() threshold logic\n");
}

/* ========================================================================
 * T-MTM-05  Gain scaling: doubling k_dump doubles dipole magnitude
 * ======================================================================== */
static void test_mtm_gain_scaling(void)
{
  float B[3] = {0.0f, 0.0f, 50.0f};
  float L_rw[3] = {0.1f, 0.0f, 0.0f};

  momentum_dump_t md1, md2;
  momentum_dump_init(&md1, 0.05f);
  momentum_dump_init(&md2, 0.10f); /* double the gain */

  float d1[3], d2[3];
  momentum_dump_step(&md1, B, L_rw, d1);
  momentum_dump_step(&md2, B, L_rw, d2);

  float mag1 = sqrtf(d1[0] * d1[0] + d1[1] * d1[1] + d1[2] * d1[2]);
  float mag2 = sqrtf(d2[0] * d2[0] + d2[1] * d2[1] + d2[2] * d2[2]);

  /* mag2 should be exactly 2 * mag1 */
  CHECK(mag1 > 0.0f, "baseline dipole magnitude must be nonzero");
  CHECK(FPEQ(mag2, 2.0f * mag1), "double gain must double dipole magnitude");
  printf("  PASS T-MTM-05 gain scaling doubles dipole magnitude\n");
}

/* ========================================================================
 * T-MTM-06  Threshold gate — momentum above MOMENTUM_DUMP_THRESHOLD
 *
 * rates = {100, 0, 0} → |rates| = 100 > 0.335 → must return true
 * ======================================================================== */
static void test_threshold_gate_above(void)
{
  float rates[3] = {100.0f, 0.0f, 0.0f};
  CHECK(momentum_dump_needed(rates, MOMENTUM_DUMP_THRESHOLD) == true,
        "needed() must return true when |rates| > MOMENTUM_DUMP_THRESHOLD");
  printf("  PASS T-MTM-06 threshold gate above (|rates|=100 > 0.335)\n");
}

/* ========================================================================
 * T-MTM-07  Threshold gate — momentum below MOMENTUM_DUMP_THRESHOLD
 *
 * rates = {0.1, 0.1, 0.1} → |rates| ≈ 0.173 < 0.335 → must return false
 * ======================================================================== */
static void test_threshold_gate_below(void)
{
  float rates[3] = {0.1f, 0.1f, 0.1f};
  CHECK(momentum_dump_needed(rates, MOMENTUM_DUMP_THRESHOLD) == false,
        "needed() must return false when |rates| < MOMENTUM_DUMP_THRESHOLD");
  printf("  PASS T-MTM-07 threshold gate below (|rates|≈0.173 < 0.335)\n");
}

/* ========================================================================
 * T-MTM-08  Threshold gate — zero momentum
 *
 * rates = {0, 0, 0} → |rates| = 0 < 0.335 → must return false
 * ======================================================================== */
static void test_threshold_gate_zero(void)
{
  float rates[3] = {0.0f, 0.0f, 0.0f};
  CHECK(momentum_dump_needed(rates, MOMENTUM_DUMP_THRESHOLD) == false,
        "needed() must return false when rates is zero");
  printf("  PASS T-MTM-08 threshold gate zero momentum\n");
}

/* ========================================================================
 * main
 * ======================================================================== */
int main(void)
{
  printf("=== Momentum Dump tests (PR-17) ===\n");

  test_mtm_init();
  test_mtm_zero_b_field();
  test_mtm_cross_product();
  test_mtm_needed();
  test_mtm_gain_scaling();
  test_threshold_gate_above();
  test_threshold_gate_below();
  test_threshold_gate_zero();

  if (g_failures == 0)
  {
    printf("ALL MOMENTUM DUMP TESTS PASSED\n");
    return 0;
  }
  printf("%d MOMENTUM DUMP TEST(S) FAILED\n", g_failures);
  return 1;
}
