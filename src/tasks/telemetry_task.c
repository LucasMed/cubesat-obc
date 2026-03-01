#include "telemetry_task.h"

#include "FreeRTOS.h"
#include "config.h"
#include "task.h"

#include <stdio.h>

#ifdef PICO_BUILD
  #include "pico/time.h"
#endif

#include "data_layer.h"

#include <csp/csp.h>

#define GN_ADDRESS 1

/* Flags byte layout:
 *   bit 0 : imu_valid
 *   bit 1 : temp_valid
 *   bits[3:2] : energy_state (ENERGY_NOMINAL=0 .. ENERGY_EMERGENCY=3)
 */
#define TLM_FLAG_IMU_VALID (1u << 0)
#define TLM_FLAG_TEMP_VALID (1u << 1)
#define TLM_FLAG_ENERGY_SHIFT 2u

// Core logic for telemetry (independent of FreeRTOS task loop)
void vTelemetryTask_Step(void)
{
  dl_snapshot_t snap;
  data_layer_read(&snap);

  // 1. Allocate CSP packet
  csp_packet_t *packet = csp_buffer_get(sizeof(csp_telemetry_packet_t));
  if (packet == NULL)
  {
    printf("[telemetry] Warning: No free CSP buffers\n");
    return;
  }

  // 2. Populate packet
  csp_telemetry_packet_t *tlm = (csp_telemetry_packet_t *)packet->data;
#ifdef PICO_BUILD
  tlm->timestamp_ms = to_ms_since_boot(get_absolute_time());
#else
  tlm->timestamp_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
#endif

  /* Flags: validity bits + energy state */
  tlm->flags = 0;
  if (snap.state.imu_valid)
    tlm->flags |= TLM_FLAG_IMU_VALID;
  if (snap.state.temp_valid)
    tlm->flags |= TLM_FLAG_TEMP_VALID;
  tlm->flags |= (uint8_t)((snap.energy & 0x03u) << TLM_FLAG_ENERGY_SHIFT);

  /* Full ADCS telemetry only when not in FM_SAFE.
   * In FM_SAFE send minimal HK: temperature retained, attitude/rates zeroed. */
  if (snap.mode != FM_SAFE)
  {
    tlm->attitude[0] = snap.state.attitude[0];
    tlm->attitude[1] = snap.state.attitude[1];
    tlm->attitude[2] = snap.state.attitude[2];
    tlm->rates[0] = snap.state.rates[0];
    tlm->rates[1] = snap.state.rates[1];
    tlm->rates[2] = snap.state.rates[2];
    tlm->temp = snap.state.temp;
  }
  else
  {
    tlm->attitude[0] = 0.0f;
    tlm->attitude[1] = 0.0f;
    tlm->attitude[2] = 0.0f;
    tlm->rates[0] = 0.0f;
    tlm->rates[1] = 0.0f;
    tlm->rates[2] = 0.0f;
    tlm->temp = snap.state.temp; /* preserve HK temperature */
  }

  packet->length = sizeof(csp_telemetry_packet_t);

  // 3. Send over CSP port connection-less
  csp_sendto(CSP_PRIO_NORM, GN_ADDRESS, TELEMETRY_PORT, TELEMETRY_PORT, CSP_O_NONE, packet);

  printf("[telemetry] Tx mode=%d att=[%.1f,%.1f,%.1f] flags=0x%02X\n", snap.mode, tlm->attitude[0],
         tlm->attitude[1], tlm->attitude[2], tlm->flags);
}

// Telemetry task: sends telemetry at 1 Hz
void vTelemetryTask(void *pvParameters)
{
  (void)pvParameters;
  TickType_t xLastWakeTime = xTaskGetTickCount();
  const TickType_t xFrequency = pdMS_TO_TICKS(1000);  // 1 Hz

  printf("[telemetry_task] Started\n");

  while (1)
  {
    vTelemetryTask_Step();
    vTaskDelayUntil(&xLastWakeTime, xFrequency);
  }
}
