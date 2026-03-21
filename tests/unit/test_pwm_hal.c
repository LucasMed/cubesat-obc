/* test_pwm_hal.c — Unit tests for the PWM HAL
 *
 * T-PWM-01 test_pwm_hal_rw_init        – reaction wheel PWM initialization
 * T-PWM-02 test_pwm_hal_rw_set_duty    – setting duty cycle
 * T-PWM-03 test_pwm_hal_rw_duty_bounds – duty cycle clamping (0.0-1.0)
 * T-PWM-04 test_pwm_hal_mtq_init       – magnetorquer PWM initialization
 * T-PWM-05 test_pwm_hal_mtq_set_duty   – setting magnetorquer duty
 * T-PWM-06 test_pwm_hal_mtq_enable     – enable/disable magnetorquer
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <math.h>
#include <assert.h>

#include "pwm_hal.h"

static int s_rw_init_calls = 0;
static int s_rw_set_duty_calls[3] = {0};
static float s_rw_duty_values[3] = {0.0f};
static int s_rw_enable_calls[3] = {0};
static bool s_rw_enable_values[3] = {false};

static int s_mtq_init_calls = 0;
static int s_mtq_set_duty_calls[3] = {0};
static float s_mtq_duty_values[3] = {0.0f};
static int s_mtq_enable_calls[3] = {0};
static bool s_mtq_enable_values[3] = {false};

int pwm_hal_rw_init(void)
{
    s_rw_init_calls++;
    return 0;
}

void pwm_hal_rw_set_duty(uint8_t axis, float duty_cycle)
{
    assert(axis < 3);
    s_rw_set_duty_calls[axis]++;
    if (duty_cycle < 0.0f)
    {
        duty_cycle = 0.0f;
    }
    else if (duty_cycle > 1.0f)
    {
        duty_cycle = 1.0f;
    }
    s_rw_duty_values[axis] = duty_cycle;
}

void pwm_hal_rw_enable(uint8_t axis, bool enable)
{
    assert(axis < 3);
    s_rw_enable_calls[axis]++;
    s_rw_enable_values[axis] = enable;
}

int pwm_hal_mtq_init(void)
{
    s_mtq_init_calls++;
    return 0;
}

void pwm_hal_mtq_set_duty(uint8_t axis, float duty_cycle)
{
    assert(axis < 3);
    s_mtq_set_duty_calls[axis]++;
    if (duty_cycle < 0.0f)
    {
        duty_cycle = 0.0f;
    }
    else if (duty_cycle > 1.0f)
    {
        duty_cycle = 1.0f;
    }
    s_mtq_duty_values[axis] = duty_cycle;
}

void pwm_hal_mtq_enable(uint8_t axis, bool enable)
{
    assert(axis < 3);
    s_mtq_enable_calls[axis]++;
    s_mtq_enable_values[axis] = enable;
}

static void reset_all(void)
{
    s_rw_init_calls = 0;
    for (int i = 0; i < 3; i++)
    {
        s_rw_set_duty_calls[i] = 0;
        s_rw_duty_values[i] = 0.0f;
        s_rw_enable_calls[i] = 0;
        s_rw_enable_values[i] = false;
    }

    s_mtq_init_calls = 0;
    for (int i = 0; i < 3; i++)
    {
        s_mtq_set_duty_calls[i] = 0;
        s_mtq_duty_values[i] = 0.0f;
        s_mtq_enable_calls[i] = 0;
        s_mtq_enable_values[i] = false;
    }
}

static int g_failures = 0;

#define CHECK(cond, msg)                                                                           \
    do                                                                                             \
    {                                                                                              \
        if (!(cond))                                                                               \
        {                                                                                          \
            printf("  FAIL [%s:%d] %s\n", __func__, __LINE__, msg);                               \
            g_failures++;                                                                          \
        }                                                                                          \
    } while (0)

/* ========================================================================
 * T-PWM-01 test_pwm_hal_rw_init — reaction wheel PWM initialization
 * ======================================================================== */
static void test_pwm_hal_rw_init(void)
{
    reset_all();
    int ret = pwm_hal_rw_init();
    CHECK(ret == 0, "rw_init should return 0");
    CHECK(s_rw_init_calls == 1, "rw_init should be called exactly once");
    printf("  PASS T-PWM-01 reaction wheel PWM init\n");
}

/* ========================================================================
 * T-PWM-02 test_pwm_hal_rw_set_duty — setting duty cycle
 * ======================================================================== */
static void test_pwm_hal_rw_set_duty(void)
{
    reset_all();

    pwm_hal_rw_set_duty(0, 0.5f);
    CHECK(s_rw_set_duty_calls[0] == 1, "set_duty should be called once for axis 0");
    CHECK(fabsf(s_rw_duty_values[0] - 0.5f) < 1e-6f, "duty should be 0.5f for axis 0");

    pwm_hal_rw_set_duty(1, 0.25f);
    CHECK(s_rw_set_duty_calls[1] == 1, "set_duty should be called once for axis 1");
    CHECK(fabsf(s_rw_duty_values[1] - 0.25f) < 1e-6f, "duty should be 0.25f for axis 1");

    pwm_hal_rw_set_duty(2, 0.75f);
    CHECK(s_rw_set_duty_calls[2] == 1, "set_duty should be called once for axis 2");
    CHECK(fabsf(s_rw_duty_values[2] - 0.75f) < 1e-6f, "duty should be 0.75f for axis 2");

    printf("  PASS T-PWM-02 reaction wheel set duty\n");
}

/* ========================================================================
 * T-PWM-03 test_pwm_hal_rw_duty_bounds — duty cycle clamping (0.0-1.0)
 * ======================================================================== */
static void test_pwm_hal_rw_duty_bounds(void)
{
    reset_all();

    pwm_hal_rw_set_duty(0, -0.5f);
    CHECK(s_rw_duty_values[0] == 0.0f, "negative duty should be clamped to 0.0");

    pwm_hal_rw_set_duty(1, 1.5f);
    CHECK(s_rw_duty_values[1] == 1.0f, "duty > 1.0 should be clamped to 1.0");

    pwm_hal_rw_set_duty(2, 0.0f);
    CHECK(s_rw_duty_values[2] == 0.0f, "zero duty should remain 0.0");

    pwm_hal_rw_set_duty(0, 1.0f);
    CHECK(s_rw_duty_values[0] == 1.0f, "max duty should remain 1.0");

    printf("  PASS T-PWM-03 reaction wheel duty bounds\n");
}

/* ========================================================================
 * T-PWM-04 test_pwm_hal_mtq_init — magnetorquer PWM initialization
 * ======================================================================== */
static void test_pwm_hal_mtq_init(void)
{
    reset_all();
    int ret = pwm_hal_mtq_init();
    CHECK(ret == 0, "mtq_init should return 0");
    CHECK(s_mtq_init_calls == 1, "mtq_init should be called exactly once");
    printf("  PASS T-PWM-04 magnetorquer PWM init\n");
}

/* ========================================================================
 * T-PWM-05 test_pwm_hal_mtq_set_duty — setting magnetorquer duty
 * ======================================================================== */
static void test_pwm_hal_mtq_set_duty(void)
{
    reset_all();

    pwm_hal_mtq_set_duty(0, 0.3f);
    CHECK(s_mtq_set_duty_calls[0] == 1, "set_duty should be called once for axis 0");
    CHECK(fabsf(s_mtq_duty_values[0] - 0.3f) < 1e-6f, "duty should be 0.3f for axis 0");

    pwm_hal_mtq_set_duty(1, 0.6f);
    CHECK(s_mtq_set_duty_calls[1] == 1, "set_duty should be called once for axis 1");
    CHECK(fabsf(s_mtq_duty_values[1] - 0.6f) < 1e-6f, "duty should be 0.6f for axis 1");

    pwm_hal_mtq_set_duty(2, 0.9f);
    CHECK(s_mtq_set_duty_calls[2] == 1, "set_duty should be called once for axis 2");
    CHECK(fabsf(s_mtq_duty_values[2] - 0.9f) < 1e-6f, "duty should be 0.9f for axis 2");

    printf("  PASS T-PWM-05 magnetorquer set duty\n");
}

/* ========================================================================
 * T-PWM-06 test_pwm_hal_mtq_enable — enable/disable magnetorquer
 * ======================================================================== */
static void test_pwm_hal_mtq_enable(void)
{
    reset_all();

    pwm_hal_mtq_enable(0, true);
    CHECK(s_mtq_enable_calls[0] == 1, "enable should be called once for axis 0");
    CHECK(s_mtq_enable_values[0] == true, "enable should be true for axis 0");

    pwm_hal_mtq_enable(1, false);
    CHECK(s_mtq_enable_calls[1] == 1, "enable should be called once for axis 1");
    CHECK(s_mtq_enable_values[1] == false, "enable should be false for axis 1");

    pwm_hal_mtq_enable(2, true);
    CHECK(s_mtq_enable_calls[2] == 1, "enable should be called once for axis 2");
    CHECK(s_mtq_enable_values[2] == true, "enable should be true for axis 2");

    printf("  PASS T-PWM-06 magnetorquer enable/disable\n");
}

/* ========================================================================
 * main
 * ======================================================================== */
int main(void)
{
    printf("=== PWM HAL tests ===\n");

    test_pwm_hal_rw_init();
    test_pwm_hal_rw_set_duty();
    test_pwm_hal_rw_duty_bounds();
    test_pwm_hal_mtq_init();
    test_pwm_hal_mtq_set_duty();
    test_pwm_hal_mtq_enable();

    if (g_failures == 0)
    {
        printf("ALL PWM HAL TESTS PASSED\n");
        return 0;
    }
    else
    {
        printf("%d PWM HAL TEST(S) FAILED\n", g_failures);
        return 1;
    }
}
