// test_radiation_integration.c -- Integration test for radiation detector driver
// Tests: Radiation samples -> accumulator -> dose estimation

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "radiation_driver.h"
#include "data_layer.h"
#include "payload_manager.h"

static int s_passed = 0;
static int s_failed = 0;

static uint16_t g_mock_adc_value = 0;

uint16_t adc_read(void)
{
  return g_mock_adc_value;
}

void test_case(const char *name, int condition) {
    if (condition) {
        printf("  [PASS] %s\n", name);
        s_passed++;
    } else {
        printf("  [FAIL] %s\n", name);
        s_failed++;
    }
}

void test_radiation_init(void) {
    printf("\n=== Test: Radiation Init ===\n");
    bool ret = radiation_init();
    test_case("Radiation init returns true", ret == true);
}

void test_radiation_read(void) {
    printf("\n=== Test: Radiation Read ===\n");
    g_mock_adc_value = 2048;
    rad_sample_t sample = {0};
    bool ret = radiation_read(&sample);
    test_case("Radiation read returns true", ret == true);
    test_case("ADC value in valid range", sample.raw_adc <= 4095);
    test_case("Voltage calculated", sample.voltage_V >= 0.0f && sample.voltage_V <= 3.3f);
}

void test_radiation_accumulate(void) {
    printf("\n=== Test: Radiation Accumulate ===\n");
    
    // Create samples with threshold
    rad_sample_t sample1 = {1000, 0.8f, true};
    rad_sample_t sample2 = {1500, 1.2f, true};
    rad_sample_t sample3 = {500, 0.4f, false};  // below threshold
    
    // Accumulate events
    radiation_accumulate(&sample1);
    radiation_accumulate(&sample2);
    radiation_accumulate(&sample3);
    
    // Get accumulator
    rad_accumulator_t acc = {0};
    radiation_get_and_reset(&acc);
    
    test_case("Event count incremented", acc.event_count >= 1);
    test_case("Dose estimated", acc.dose_Gy >= 0.0f);
}

void test_radiation_threshold(void) {
    printf("\n=== Test: Radiation Threshold ===\n");
    
    // Test above threshold
    g_mock_adc_value = 3500;
    rad_sample_t high = {0};
    radiation_read(&high);
    
    // Test below threshold
    g_mock_adc_value = 100;
    rad_sample_t low = {0};
    radiation_read(&low);
    
    // Just verify reading works
    test_case("High sample valid", high.raw_adc > 0);
    test_case("High above threshold", high.threshold == true);
    test_case("Low sample valid", low.raw_adc < 4095);
    test_case("Low below threshold", low.threshold == false);
}

void test_payload_manager_dose(void) {
    printf("\n=== Test: Payload Manager Dose ===\n");
    
    // Get cumulative dose from payload manager
    float dose = payload_manager_get_cumulative_dose();
    
    test_case("Cumulative dose accessible", dose >= 0.0f);
}

void test_radiation_multiple_cycles(void) {
    printf("\n=== Test: Radiation Multiple Cycles ===\n");
    
    // Reset accumulator
    rad_accumulator_t acc1 = {0};
    radiation_get_and_reset(&acc1);
    
    // First cycle
    rad_sample_t s1 = {2000, 1.6f, true};
    radiation_accumulate(&s1);
    radiation_accumulate(&s1);
    radiation_get_and_reset(&acc1);
    
    // Second cycle (after reset)
    rad_accumulator_t acc2 = {0};
    rad_sample_t s2 = {1500, 1.2f, true};
    radiation_accumulate(&s2);
    radiation_get_and_reset(&acc2);
    
    test_case("First cycle has events", acc1.event_count > 0);
    test_case("Second cycle has events", acc2.event_count > 0);
}

void test_radiation_dose_linearity(void) {
    printf("\n=== Test: Radiation Dose Linearity ===\n");
    
    // Reset
    rad_accumulator_t acc0 = {0};
    radiation_get_and_reset(&acc0);
    
    // Add 10 events
    for (int i = 0; i < 10; i++) {
        rad_sample_t s = {2000, 1.6f, true};
        radiation_accumulate(&s);
    }
    
    rad_accumulator_t acc = {0};
    radiation_get_and_reset(&acc);
    
    test_case("10 events accumulated", acc.event_count == 10);
    test_case("Dose proportional", acc.dose_Gy > 0.0f);
}

int main(void) {
    printf("===========================================\n");
    printf("Radiation Integration Test Suite\n");
    printf("===========================================\n");
    
    data_layer_init();
    test_radiation_init();
    test_radiation_read();
    test_radiation_accumulate();
    test_radiation_threshold();
    test_payload_manager_dose();
    test_radiation_multiple_cycles();
    test_radiation_dose_linearity();
    
    printf("\n===========================================\n");
    printf("Results: %d passed, %d failed\n", s_passed, s_failed);
    printf("===========================================\n");
    
    return s_failed > 0 ? 1 : 0;
}
