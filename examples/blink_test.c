/**
 * @file blink_test.c
 * @brief Pico 2W LED Blink Test
 *
 * Validates Pico 2W SDK integration by toggling onboard LED via CYW43 chip.
 * The LED on Pico 2W is controlled through the CYW43 WiFi chip, not direct GPIO.
 *
 * Compile with: cmake .. && cmake --build .
 * Flash: Hold BOOTSEL, connect USB, drag .uf2 to RPI-RP2 drive
 */

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"

int main(void) {
    stdio_init_all();
    
    printf("\n=== Pico 2W LED Blink Test ===\n");
    printf("Initializing CYW43 (WiFi chip)...\n");
    
    // Initialize CYW43 architecture (required for LED control on Pico W/2W)
    if (cyw43_arch_init()) {
        printf("❌ CYW43 initialization failed!\n");
        return -1;
    }
    
    printf("✅ CYW43 initialized successfully\n");
    printf("Starting LED blink pattern (10 cycles)...\n");
    printf("If the LED is blinking, SDK integration is working!\n\n");
    
    // Blink pattern: 5 cycles of 250ms on/off
    for (int i = 0; i < 10; i++) {
        // LED ON
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
        printf("LED ON  [%2d/10]\n", i + 1);
        sleep_ms(250);
        
        // LED OFF
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
        printf("LED OFF\n");
        sleep_ms(250);
    }
    
    printf("\n✅ Test complete!\n");
    printf("If LED blinked 5 times, Pico 2W API is correctly integrated.\n");
    
    // Cleanup
    cyw43_arch_deinit();
    
    return 0;
}
