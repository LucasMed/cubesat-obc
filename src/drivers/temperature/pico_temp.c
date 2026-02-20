/**
 * @file pico_temp.c
 * @brief Pico implementation of the onboard temperature sensor using ADC.
 */

#include "drivers/temperature.h"
#include "pico/stdlib.h"
#include "hardware/adc.h"

int temperature_init(void) {
    adc_init();
    adc_set_temp_sensor_enabled(true);
    adc_select_input(4); // Onboard temperature sensor is on ADC4
    return 0;
}

float temperature_read(void) {
    // Select the temp sensor input
    adc_select_input(4);
    
    // Read raw ADC value
    uint16_t raw = adc_read();
    
    // Scale voltage to CELSIUS
    // formula: 27 - (raw_voltage - 0.706) / 0.001721
    // raw_voltage = raw * 3.3 / 4096
    float voltage = (float)raw * 3.3f / 4096.0f;
    float temp = 27.0f - (voltage - 0.706f) / 0.001721f;
    
    return temp;
}
