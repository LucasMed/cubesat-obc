#include "config.h"
#include "telemetry_task.h"
#include <stdio.h>
#include "FreeRTOS.h"
#include "task.h"

#ifdef PICO_BUILD
#include "pico/time.h"
#endif

#include "system_state.h"
#include <csp/csp.h>

#define GN_ADDRESS 1

// Core logic for telemetry (independent of FreeRTOS task loop)
void vTelemetryTask_Step(void) {
    system_state_t state;
    system_state_get(&state);

    // 1. Allocate CSP packet
    csp_packet_t * packet = csp_buffer_get(sizeof(csp_telemetry_packet_t));
    if (packet == NULL) {
        printf("[telemetry] Warning: No free CSP buffers\n");
        return;
    }

    // 2. Populate packet
    csp_telemetry_packet_t * tlm = (csp_telemetry_packet_t *) packet->data;
    #ifdef PICO_BUILD
    tlm->timestamp_ms = to_ms_since_boot(get_absolute_time());
    #else
    tlm->timestamp_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
    #endif
    
    tlm->attitude[0] = state.attitude[0];
    tlm->attitude[1] = state.attitude[1];
    tlm->attitude[2] = state.attitude[2];
    
    tlm->rates[0] = state.rates[0];
    tlm->rates[1] = state.rates[1];
    tlm->rates[2] = state.rates[2];
    
    tlm->temp = state.temp;
    
    tlm->flags = 0;
    if (state.imu_valid) tlm->flags |= (1 << 0);
    if (state.temp_valid) tlm->flags |= (1 << 1);

    packet->length = sizeof(csp_telemetry_packet_t);

    // 3. Send over CSP port connection-less
    // Priority: CSP_PRIO_NORM, Dest: GN_ADDRESS, DestPort: TELEMETRY_PORT, SrcPort: TELEMETRY_PORT
    csp_sendto(CSP_PRIO_NORM, GN_ADDRESS, TELEMETRY_PORT, TELEMETRY_PORT, CSP_O_NONE, packet);

    // Also print debug info
    printf("[telemetry] Tx: ATT[%.1f, %.1f, %.1f] GYRO[%.1f, %.1f, %.1f] TEMP[%.1f C] flags[0x%02X]\n",
           tlm->attitude[0], tlm->attitude[1], tlm->attitude[2],
           tlm->rates[0], tlm->rates[1], tlm->rates[2],
           tlm->temp, tlm->flags);
}

// Telemetry task: sends telemetry at 1 Hz
void vTelemetryTask(void *pvParameters) {
    (void)pvParameters;
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(1000); // 1 Hz

    printf("[telemetry_task] Started\n");

    while (1) {
        vTelemetryTask_Step();
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}
