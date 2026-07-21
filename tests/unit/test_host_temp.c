/**
 * @file test_host_temp.c
 * @brief Unit tests for host_temp.c — verify host temperature stubs
 *        return expected values.
 */

#include "drivers/temperature.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>

static void test_temperature_init_returns_zero(void)
{
    int ret = temperature_init();
    assert(ret == 0);
    printf("  PASS T-HTMP-01 temperature_init returns 0\n");
}

static void test_temperature_read_returns_25(void)
{
    float temp = temperature_read();
    assert(fabsf(temp - 25.0f) < 1e-6f);
    printf("  PASS T-HTMP-02 temperature_read returns 25.0\n");
}

int main(void)
{
    printf("=== Host Temp tests ===\n");

    test_temperature_init_returns_zero();
    test_temperature_read_returns_25();

    printf("ALL HOST TEMP TESTS PASSED\n");
    return 0;
}
