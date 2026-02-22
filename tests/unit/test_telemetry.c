#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

// Mock FreeRTOS
#include "FreeRTOS.h"
#include "task.h"

uint32_t mock_tick_count = 1000;
uint32_t mock_xTaskGetTickCount(void) {
    return mock_tick_count;
}
void mock_vTaskDelayUntil(uint32_t *pxPreviousWakeTime, uint32_t xTimeIncrement) {
    *pxPreviousWakeTime += xTimeIncrement;
    mock_tick_count += xTimeIncrement;
}

#undef xTaskGetTickCount
#undef vTaskDelayUntil
#define xTaskGetTickCount mock_xTaskGetTickCount
#define vTaskDelayUntil mock_vTaskDelayUntil

// Mock libcsp
#include <csp/csp.h>

// Simple mock buffers
csp_packet_t *mock_packet = NULL;

csp_packet_t *csp_buffer_get(size_t size) {
    if (mock_packet == NULL) {
        mock_packet = malloc(sizeof(csp_packet_t) + 256);
    }
    mock_packet->length = 0;
    return mock_packet;
}

// Capture arguments passed to csp_sendto
int last_prio = -1;
int last_dest = -1;
int last_dport = -1;
int last_sport = -1;
uint32_t last_opts = 0;
csp_packet_t *last_sent_packet = NULL;

void csp_sendto(uint8_t prio, uint16_t dest, uint8_t dport, uint8_t sport, uint32_t opts, csp_packet_t *packet) {
    last_prio = prio;
    last_dest = dest;
    last_dport = dport;
    last_sport = sport;
    last_opts = opts;
    last_sent_packet = packet;
}


// System state definitions
#include "system_state.h"

// Actual file to test
#include "telemetry_task.h"

void reset_mocks() {
    last_prio = -1;
    last_dest = -1;
    last_dport = -1;
    last_sport = -1;
    last_opts = 0;
    last_sent_packet = NULL;
    mock_tick_count = 1000;
}

void test_telemetry_packet_packing() {
    reset_mocks();

    // 1. Setup mock system state
    system_state_init();
    float att[3] = {1.5f, -2.0f, 45.0f};
    float rates[3] = {0.1f, 0.0f, -0.5f};
    system_state_set_imu(att, rates);
    system_state_set_temp(28.5f);
    system_state_set_available(true, true); // marks imu and temp valid

    // 2. Call the step function directly
    vTelemetryTask_Step();

    // 3. Verify csp_sendto was called
    assert(last_sent_packet != NULL);
    assert(last_prio == CSP_PRIO_NORM);
    assert(last_dest == 1); // GN_ADDRESS
    assert(last_dport == TELEMETRY_PORT);
    assert(last_sport == TELEMETRY_PORT);
    assert(last_opts == CSP_O_NONE);

    // 4. Verify packet payload
    assert(last_sent_packet->length == sizeof(csp_telemetry_packet_t));
    csp_telemetry_packet_t *tlm = (csp_telemetry_packet_t *)last_sent_packet->data;
    
    // Check values
    assert(tlm->timestamp_ms == 1000);
    assert(tlm->attitude[0] == 1.5f);
    assert(tlm->attitude[1] == -2.0f);
    assert(tlm->attitude[2] == 45.0f);
    assert(tlm->rates[0] == 0.1f);
    assert(tlm->rates[1] == 0.0f);
    assert(tlm->rates[2] == -0.5f);
    assert(tlm->temp == 28.5f);
    
    // Check flags: imu_valid (bit 0) | temp_valid (bit 1) = 3
    assert(tlm->flags == 0x03);

    printf("test_telemetry_packet_packing: PASS\n");
}

int main() {
    printf("Running Telemetry Task tests...\n");
    test_telemetry_packet_packing();
    printf("All tests passed!\n");
    return 0;
}
