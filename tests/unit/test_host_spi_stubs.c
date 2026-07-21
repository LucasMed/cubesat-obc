/**
 * @file test_host_spi_stubs.c
 * @brief Unit tests for host_spi_gpio_stubs.c — verify all host stubs
 *        exist and return expected values.
 */

#include "FreeRTOS.h"

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

/* Forward declarations — implemented in host_spi_gpio_stubs.c */
int spi_write_blocking(void *spi, const uint8_t *src, size_t len);
int spi_read_blocking(void *spi, uint8_t filler, uint8_t *dst, size_t len);
void gpio_put(unsigned int pin, int value);
uint16_t adc_read(void);
int gpio_get(unsigned int pin);
BaseType_t xTaskNotifyWait_Mock(uint32_t bits_to_clear_on_entry,
                                uint32_t bits_to_clear_on_exit,
                                uint32_t *notification_value,
                                TickType_t ticks_to_wait);

static void test_spi_write_returns_len(void)
{
    uint8_t data[] = {0xAA, 0xBB, 0xCC};
    int ret = spi_write_blocking(NULL, data, sizeof(data));
    assert(ret == (int)sizeof(data));
    printf("  PASS T-SPI-01 spi_write_blocking returns len\n");
}

static void test_spi_read_fills_zeros(void)
{
    uint8_t buf[4] = {0xFF, 0xFF, 0xFF, 0xFF};
    int ret = spi_read_blocking(NULL, 0x00, buf, sizeof(buf));
    assert(ret == (int)sizeof(buf));
    for (size_t i = 0; i < sizeof(buf); i++)
    {
        assert(buf[i] == 0);
    }
    printf("  PASS T-SPI-02 spi_read_blocking fills with zeros\n");
}

static void test_gpio_put_no_crash(void)
{
    gpio_put(0, 0);
    gpio_put(255, 1);
    printf("  PASS T-SPI-03 gpio_put does not crash\n");
}

static void test_adc_read_returns_zero(void)
{
    uint16_t val = adc_read();
    assert(val == 0);
    printf("  PASS T-SPI-04 adc_read returns 0\n");
}

static void test_gpio_get_returns_true(void)
{
    int val = gpio_get(0);
    assert(val == 1);
    printf("  PASS T-SPI-05 gpio_get returns 1\n");
}

static void test_xtask_notify_wait_returns_pdpass(void)
{
    uint32_t notification_value = 0xDEADBEEF;
    BaseType_t ret = xTaskNotifyWait_Mock(0, 0, &notification_value, 0);
    assert(ret == pdPASS);
    assert(notification_value == 0);
    printf("  PASS T-SPI-06 xTaskNotifyWait_Mock returns pdPASS\n");
}

int main(void)
{
    printf("=== Host SPI Stubs tests ===\n");

    test_spi_write_returns_len();
    test_spi_read_fills_zeros();
    test_gpio_put_no_crash();
    test_adc_read_returns_zero();
    test_gpio_get_returns_true();
    test_xtask_notify_wait_returns_pdpass();

    printf("ALL HOST SPI STUBS TESTS PASSED\n");
    return 0;
}
