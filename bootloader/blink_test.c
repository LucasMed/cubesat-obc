/**
 * @file blink_test.c
 * @brief RP2350 GP0 blink — solo SDK gpio, sin stdio, sin sleep
 *
 * Propósito: aislar el problema. Sin stdio_init_all (que configura UART en GP0),
 * sin sleep_ms (que necesita timer). Solo gpio_init + delay por software.
 *
 * Si este tampoco funciona, el problema NO está en la secuencia de init.
 */

#include <stdint.h>
#include "hardware/gpio.h"

#define LED_PIN 0

static void spin_delay(void) {
    volatile uint32_t n = 5000000;
    while (n--) { __asm volatile("nop"); }
}

int main(void) {
    /* Solo gpio_init — sin stdio, sin nada más */
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);

    while (1) {
        gpio_put(LED_PIN, 1);
        spin_delay();
        gpio_put(LED_PIN, 0);
        spin_delay();
    }

    return 0;
}
