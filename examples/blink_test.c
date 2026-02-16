/**
 * @file blink_test.c
 * @brief Minimal Pico LED Blink Test
 *
 * Validates Pico SDK integration by toggling onboard LED.
 * Compile with: PICO_SDK_PATH=/path/to/pico-sdk cmake -DPICO_SDK_FETCH_FROM_GIT=off ..
 */

#include <stdio.h>
#include "pico/stdlib.h"

int main(void) {
    stdio_init_all();
    printf("\n=== Pico 2W SDK Blink Test ===\n");
    
    // Initialize LED (GPIO25 on Pico/Pico 2W)
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
    
    printf("Blinking LED for 10 seconds...\n");
    printf("LED Pin: %d\n", PICO_DEFAULT_LED_PIN);
    
    // Blink 5 times (1 second on, 1 second off)
    for (int i = 0; i < 10; i++) {
        gpio_put(PICO_DEFAULT_LED_PIN, 1);
        printf("LED ON\n");
        sleep_ms(1000);
        
        gpio_put(PICO_DEFAULT_LED_PIN, 0);
        printf("LED OFF\n");
        sleep_ms(1000);
    }
    
    printf("Test complete! ✅\n");
    printf("If LED blinked 5 times, Pico SDK integration is successful.\n");
    
    return 0;
}
