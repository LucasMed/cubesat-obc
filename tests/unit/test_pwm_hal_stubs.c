/**
 * @file test_pwm_hal_stubs.c
 * @brief Unit tests for pwm_hal.c host stubs (non-PICO_BUILD block).
 *
 * Compiles pwm_hal.c directly so the #else host stubs (lines 136-172)
 * are active.  Does NOT link against any mock library — test_pwm_hal
 * already provides strong-symbol overrides; this test must exercise the
 * real host stubs in pwm_hal.c itself.
 */

#include "pwm_hal.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

static void test_rw_init_returns_zero(void)
{
    assert(pwm_hal_rw_init() == 0);
    printf("  PASS T-PWMS-01 pwm_hal_rw_init returns 0\n");
}

static void test_rw_set_duty_no_crash(void)
{
    pwm_hal_rw_set_duty(0, 0.5f);
    pwm_hal_rw_set_duty(1, 0.0f);
    pwm_hal_rw_set_duty(2, 1.0f);
    printf("  PASS T-PWMS-02 pwm_hal_rw_set_duty does not crash\n");
}

static void test_rw_enable_no_crash(void)
{
    pwm_hal_rw_enable(0, true);
    pwm_hal_rw_enable(1, false);
    printf("  PASS T-PWMS-03 pwm_hal_rw_enable does not crash\n");
}

static void test_mtq_init_returns_zero(void)
{
    assert(pwm_hal_mtq_init() == 0);
    printf("  PASS T-PWMS-04 pwm_hal_mtq_init returns 0\n");
}

static void test_mtq_set_duty_no_crash(void)
{
    pwm_hal_mtq_set_duty(0, 0.3f);
    pwm_hal_mtq_set_duty(1, 0.6f);
    pwm_hal_mtq_set_duty(2, 0.9f);
    printf("  PASS T-PWMS-05 pwm_hal_mtq_set_duty does not crash\n");
}

static void test_mtq_enable_no_crash(void)
{
    pwm_hal_mtq_enable(0, true);
    pwm_hal_mtq_enable(1, false);
    printf("  PASS T-PWMS-06 pwm_hal_mtq_enable does not crash\n");
}

int main(void)
{
    printf("=== PWM HAL Stubs tests ===\n");

    test_rw_init_returns_zero();
    test_rw_set_duty_no_crash();
    test_rw_enable_no_crash();
    test_mtq_init_returns_zero();
    test_mtq_set_duty_no_crash();
    test_mtq_enable_no_crash();

    printf("ALL PWM HAL STUBS TESTS PASSED\n");
    return 0;
}
