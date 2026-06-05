/* test_telemetry.c — Unit tests for vTelemetryTask_Step() (PR-9)
 *
 * T-TLM-01  test_tlm_full_packet_in_nominal  – Full fields sent in FM_NOMINAL
 * T-TLM-02  test_tlm_hk_only_in_safe         – Attitude/rates zeroed in FM_SAFE, temp kept
 * T-TLM-03  test_tlm_sends_in_all_modes       – csp_sendto called in every FM
 * T-TLM-04  test_tlm_energy_state_in_flags    – 4 energy states encoded in flags[3:2]
 * T-TLM-05  test_tlm_flags_imu_temp           – flags bits 0-1 track imu_valid / temp_valid
 * T-TLM-06  test_tlm_null_buffer              – No csp_sendto when buffer alloc fails
 */

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- FreeRTOS mocks ---------------------------------------------------- */
#include "FreeRTOS.h"
#include "task.h"

static uint32_t s_tick = 1000u;

/* cppcheck-suppress unusedFunction -- test mock, reserved for future hook use */
static uint32_t mock_xTaskGetTickCount(void)
{
  return s_tick;
}
/* cppcheck-suppress unusedFunction -- test mock, reserved for future hook use */
static void mock_vTaskDelayUntil(uint32_t *prev, uint32_t inc)
{
  *prev += inc;
  s_tick += inc;
}

#undef xTaskGetTickCount
#undef vTaskDelayUntil
#define xTaskGetTickCount mock_xTaskGetTickCount
#define vTaskDelayUntil mock_vTaskDelayUntil

/* ---- CSP mocks ---------------------------------------------------------- */
#include <csp/csp.h>

static int s_buffer_fail = 0; /* set to 1 to make csp_buffer_get return NULL */
static csp_packet_t *s_mock_pkt = NULL;

csp_packet_t *csp_buffer_get(size_t size)
{
  (void)size;
  if (s_buffer_fail)
    return NULL;
  if (!s_mock_pkt)
    s_mock_pkt = malloc(sizeof(csp_packet_t) + 256);
  s_mock_pkt->length = 0;
  return s_mock_pkt;
}

static int s_send_calls = 0;
static int s_send_prio = -1;
static int s_send_dest = -1;
static int s_send_dport = -1;
static csp_packet_t *s_send_pkt = NULL;

void csp_sendto(uint8_t prio, uint16_t dest, uint8_t dport, uint8_t sport, uint32_t opts,
                csp_packet_t *packet)
{
  (void)sport;
  (void)opts;
  s_send_calls++;
  s_send_prio = prio;
  s_send_dest = dest;
  s_send_dport = dport;
  s_send_pkt = packet;
}

/* ---- DLA / system_state headers ---------------------------------------- */
#include "data_layer.h"
#include "eps.h"
#include "flight_mode.h"
#include "system_state.h"
#include "telemetry_storage.h"
#include "telemetry_task.h"
#include "../../src/protocols/telemetry_packet.h"

/* ---- Stub globals (from telemetry_storage_stub.c) ---------------------- */
extern telemetry_record_t g_telemetry_storage_last_record;
extern bool g_telemetry_storage_store_called;

/* ---- Helpers ------------------------------------------------------------ */
static int g_failures = 0;

#define CHECK(cond, msg)                                                                           \
  do                                                                                               \
  {                                                                                                \
    if (!(cond))                                                                                   \
    {                                                                                              \
      printf("  FAIL [%s:%d] %s\n", __func__, __LINE__, msg);                                      \
      g_failures++;                                                                                \
    }                                                                                              \
  } while (0)

static void reset_all(void)
{
  data_layer_init();
  s_buffer_fail = 0;
  s_send_calls = 0;
  s_send_prio = -1;
  s_send_dest = -1;
  s_send_dport = -1;
  s_send_pkt = NULL;
  s_tick = 1000u;
}

/* Helper: write a complete snapshot.
 * imu_ok / temp_ok control whether write_imu / write_temp are called;
 * each write auto-sets the corresponding _valid flag.
 * After data_layer_init() both flags start as false, so skipping a write
 * leaves valid=false.
 */
static void set_state(flight_mode_t mode, energy_state_t energy, float att0, float att1, float att2,
                      float r0, float r1, float r2, float temp, int imu_ok, int temp_ok)
{
  data_layer_set_flight_mode(mode);
  data_layer_set_energy_state(energy);
  if (imu_ok)
  {
    float att[3] = {att0, att1, att2};
    float rates[3] = {r0, r1, r2};
    data_layer_write_imu(att, rates);
  }
  if (temp_ok)
    data_layer_write_temp(temp);
}

/* ========================================================================
 * T-TLM-01  Full packet in FM_NOMINAL
 * ======================================================================== */
static void test_tlm_full_packet_in_nominal(void)
{
  reset_all();
  set_state(FM_NOMINAL, ENERGY_NOMINAL, 1.5f, -2.0f, 45.0f, 0.1f, 0.0f, -0.5f, 28.5f, 1, 1);

  vTelemetryTask_Step();

  CHECK(s_send_calls == 1, "csp_sendto called once");
  CHECK(s_send_prio == CSP_PRIO_NORM, "priority NORM");
  CHECK(s_send_dest == 1, "dest = GN_ADDRESS");
  CHECK(s_send_dport == TELEMETRY_PORT, "port correct");
  CHECK(s_send_pkt != NULL, "packet not NULL");
  if (s_send_pkt == NULL)
  {
    return;
  } /* guard: CHECK does not abort */

  csp_telemetry_packet_t *tl = (csp_telemetry_packet_t *)s_send_pkt->data;
  CHECK((uint32_t)s_send_pkt->length == sizeof(csp_telemetry_packet_t), "packet length");
  CHECK(tl->timestamp_ms == 0u, "timestamp from host tick stub (=0)");
  CHECK(tl->attitude[0] == 1.5f, "attitude[0]");
  CHECK(tl->attitude[1] == -2.0f, "attitude[1]");
  CHECK(tl->attitude[2] == 45.0f, "attitude[2]");
  CHECK(tl->rates[0] == 0.1f, "rates[0]");
  CHECK(tl->rates[2] == -0.5f, "rates[2]");
  CHECK(tl->temp == 28.5f, "temperature");

  printf("[T-TLM-01] test_tlm_full_packet_in_nominal: %s\n", g_failures == 0 ? "PASS" : "FAIL");
}

/* ========================================================================
 * T-TLM-02  HK-only in FM_SAFE — attitude/rates zeroed, temp kept
 * ======================================================================== */
static void test_tlm_hk_only_in_safe(void)
{
  reset_all();
  int failures_before = g_failures;
  set_state(FM_SAFE, ENERGY_NOMINAL, 3.0f, 4.0f, 5.0f, 1.0f, 2.0f, 3.0f, 22.0f, 1, 1);

  vTelemetryTask_Step();

  CHECK(s_send_calls == 1, "csp_sendto still called in SAFE");
  csp_telemetry_packet_t *tl = (csp_telemetry_packet_t *)s_send_pkt->data;
  CHECK(tl->attitude[0] == 0.0f, "attitude[0] zeroed in SAFE");
  CHECK(tl->attitude[1] == 0.0f, "attitude[1] zeroed in SAFE");
  CHECK(tl->attitude[2] == 0.0f, "attitude[2] zeroed in SAFE");
  CHECK(tl->rates[0] == 0.0f, "rates[0] zeroed in SAFE");
  CHECK(tl->rates[1] == 0.0f, "rates[1] zeroed in SAFE");
  CHECK(tl->rates[2] == 0.0f, "rates[2] zeroed in SAFE");
  CHECK(tl->temp == 22.0f, "temp kept in SAFE");

  printf("[T-TLM-02] test_tlm_hk_only_in_safe: %s\n",
         g_failures == failures_before ? "PASS" : "FAIL");
}

/* ========================================================================
 * T-TLM-03  csp_sendto is called in every flight mode
 * ======================================================================== */
static void test_tlm_sends_in_all_modes(void)
{
  int failures_before = g_failures;
  flight_mode_t modes[] = {FM_BOOT, FM_SAFE, FM_NOMINAL, FM_DETUMBLE, FM_DIAGNOSTIC};
  const int N = (int)(sizeof(modes) / sizeof(modes[0]));

  for (int i = 0; i < N; i++)
  {
    reset_all();
    set_state(modes[i], ENERGY_NOMINAL, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 20.0f, 1, 1);
    vTelemetryTask_Step();
    CHECK(s_send_calls == 1, "sendto called in this mode");
  }

  printf("[T-TLM-03] test_tlm_sends_in_all_modes: %s\n",
         g_failures == failures_before ? "PASS" : "FAIL");
}

/* ========================================================================
 * T-TLM-04  Energy state encoded in flags bits[5:3]
 * ======================================================================== */
static void test_tlm_energy_state_in_flags(void)
{
  int failures_before = g_failures;
  energy_state_t states[] = {ENERGY_NOMINAL, ENERGY_LOW, ENERGY_CRITICAL, ENERGY_EMERGENCY};
  const int N = (int)(sizeof(states) / sizeof(states[0]));

  for (int i = 0; i < N; i++)
  {
    reset_all();
    set_state(FM_NOMINAL, states[i], 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 20.0f, 0, 0);
    vTelemetryTask_Step();
    csp_telemetry_packet_t *tl = (csp_telemetry_packet_t *)s_send_pkt->data;
    /* Energy state in bits [7:5] */
    uint8_t extracted = (tl->flags >> 5) & 0x07u;
    CHECK((int)extracted == (int)states[i], "energy state in flags[7:5]");
  }

  printf("[T-TLM-04] test_tlm_energy_state_in_flags: %s\n",
         g_failures == failures_before ? "PASS" : "FAIL");
}

/* ========================================================================
 * T-TLM-05  flags bits 0-1 track imu_valid and temp_valid
 * ======================================================================== */
static void test_tlm_flags_imu_temp(void)
{
  int failures_before = g_failures;

  /* neither valid */
  reset_all();
  set_state(FM_NOMINAL, ENERGY_NOMINAL, 0, 0, 0, 0, 0, 0, 20.0f, 0, 0);
  vTelemetryTask_Step();
  CHECK((((csp_telemetry_packet_t *)s_send_pkt->data)->flags & 0x03u) == 0x00u,
        "flags=0x00 when neither valid");

  /* imu valid only */
  reset_all();
  set_state(FM_NOMINAL, ENERGY_NOMINAL, 0, 0, 0, 0, 0, 0, 20.0f, 1, 0);
  vTelemetryTask_Step();
  CHECK((((csp_telemetry_packet_t *)s_send_pkt->data)->flags & 0x03u) == 0x01u,
        "flags bit0 set when imu_valid");

  /* temp valid only */
  reset_all();
  set_state(FM_NOMINAL, ENERGY_NOMINAL, 0, 0, 0, 0, 0, 0, 20.0f, 0, 1);
  vTelemetryTask_Step();
  CHECK((((csp_telemetry_packet_t *)s_send_pkt->data)->flags & 0x03u) == 0x02u,
        "flags bit1 set when temp_valid");

  /* both valid */
  reset_all();
  set_state(FM_NOMINAL, ENERGY_NOMINAL, 0, 0, 0, 0, 0, 0, 20.0f, 1, 1);
  vTelemetryTask_Step();
  CHECK((((csp_telemetry_packet_t *)s_send_pkt->data)->flags & 0x03u) == 0x03u,
        "flags=0x03 when both valid");

  printf("[T-TLM-05] test_tlm_flags_imu_temp: %s\n",
         g_failures == failures_before ? "PASS" : "FAIL");
}

/* ========================================================================
 * T-TLM-07  Payload HK in CSP packet — DLA fields match packet fields
 * ======================================================================== */
static void test_tlm_payload_hk_in_csp(void)
{
  /* Case 1: nominal values */
  {
    int failures_before = g_failures;
    reset_all();

    float mag[3] = {12.5f, -3.2f, 45.8f};
    data_layer_write_mag(mag);
    data_layer_write_radiation(0.05f);
    data_layer_write_payload_status(true, 42);

    set_state(FM_NOMINAL, ENERGY_NOMINAL, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 22.0f, 1, 1);
    vTelemetryTask_Step();

    csp_telemetry_packet_t *tl = (csp_telemetry_packet_t *)s_send_pkt->data;
    CHECK(s_send_calls == 1, "csp_sendto called once");
    CHECK(tl->mag_field[0] == 12.5f, "mag_field[0]");
    CHECK(tl->mag_field[1] == -3.2f, "mag_field[1]");
    CHECK(tl->mag_field[2] == 45.8f, "mag_field[2]");
    CHECK(tl->radiation_dose == 0.05f, "radiation_dose");
    CHECK(tl->image_count == 42, "image_count");
    CHECK(tl->payload_rail_enabled == 1, "payload_rail_enabled");
  }

  /* Case 2: edge values — negative mag, zero image count, rail disabled */
  {
    int failures_before = g_failures;
    reset_all();

    float mag[3] = {-0.1f, 0.0f, 32768.0f};
    data_layer_write_mag(mag);
    data_layer_write_radiation(999.9f);
    data_layer_write_payload_status(false, 0);

    set_state(FM_NOMINAL, ENERGY_NOMINAL, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 20.0f, 0, 0);
    vTelemetryTask_Step();

    csp_telemetry_packet_t *tl = (csp_telemetry_packet_t *)s_send_pkt->data;
    CHECK(tl->mag_field[0] == -0.1f, "mag_field[0] negative");
    CHECK(tl->mag_field[2] == 32768.0f, "mag_field[2] large");
    CHECK(tl->radiation_dose == 999.9f, "radiation_dose large");
    CHECK(tl->image_count == 0, "image_count zero");
    CHECK(tl->payload_rail_enabled == 0, "payload_rail_enabled disabled");
  }

  printf("[T-TLM-07] test_tlm_payload_hk_in_csp: %s\n",
         g_failures == 0 ? "PASS" : "FAIL");
}

/* ========================================================================
 * T-TLM-09  Compile-time and runtime sizeof assertions for FR-17 structs
 * ======================================================================== */
/* Compile-time checks are at file scope above.  Runtime function reports. */
static void test_tlm_sizeof_assertions(void)
{
  int failures_before = g_failures;

  CHECK(sizeof(csp_telemetry_packet_t) == 98, "sizeof(csp_telemetry_packet_t) == 98");
  CHECK(sizeof(telemetry_packet_t) == 64, "sizeof(telemetry_packet_t) == 64");
  CHECK(sizeof(telemetry_record_t) == 76, "sizeof(telemetry_record_t) == 76");
  CHECK(TLM_PACKET_SIZE == 64, "TLM_PACKET_SIZE == 64");

  printf("[T-TLM-09] test_tlm_sizeof_assertions: %s\n",
         g_failures == failures_before ? "PASS" : "FAIL");
}

/* ========================================================================
 * T-TLM-08  Defaults when DLA not set — payload HK fields zero
 * ======================================================================== */
static void test_tlm_payload_hk_defaults_zero(void)
{
  int failures_before = g_failures;
  reset_all();

  /* Do NOT write payload HK — DLA is zero-initialised by data_layer_init() */
  set_state(FM_NOMINAL, ENERGY_NOMINAL, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 22.0f, 1, 1);

  vTelemetryTask_Step();

  csp_telemetry_packet_t *tl = (csp_telemetry_packet_t *)s_send_pkt->data;
  CHECK(tl->mag_field[0] == 0.0f, "mag_field[0] defaults to 0");
  CHECK(tl->mag_field[1] == 0.0f, "mag_field[1] defaults to 0");
  CHECK(tl->mag_field[2] == 0.0f, "mag_field[2] defaults to 0");
  CHECK(tl->radiation_dose == 0.0f, "radiation_dose defaults to 0");
  CHECK(tl->image_count == 0, "image_count defaults to 0");
  CHECK(tl->payload_rail_enabled == 0, "payload_rail_enabled defaults to 0");

  printf("[T-TLM-08] test_tlm_payload_hk_defaults_zero: %s\n",
         g_failures == failures_before ? "PASS" : "FAIL");
}

/* ========================================================================
 * T-TLM-10  Flash record populated — store captures payload HK from DLA
 * ======================================================================== */
static void test_tlm_flash_record_payload_hk(void)
{
  /* Case 1: nominal values */
  {
    int failures_before = g_failures;
    reset_all();
    g_telemetry_storage_store_called = false;

    float mag[3] = {1.0f, 2.0f, 3.0f};
    data_layer_write_mag(mag);
    data_layer_write_radiation(0.99f);
    data_layer_write_payload_status(true, 7);

    set_state(FM_NOMINAL, ENERGY_NOMINAL, 10.0f, 20.0f, 30.0f, 0.1f, 0.2f, 0.3f, 25.0f, 1, 1);
    vTelemetryTask_Step();

    CHECK(g_telemetry_storage_store_called, "store called");
    CHECK(g_telemetry_storage_last_record.radiation_dose == 0.99f, "radiation_dose");
    CHECK(g_telemetry_storage_last_record.image_count == 7, "image_count");
    CHECK(g_telemetry_storage_last_record.payload_rail_enabled == 1, "rail enabled");
  }

  /* Case 2: rail disabled, zero image count, zero radiation */
  {
    int failures_before = g_failures;
    reset_all();
    g_telemetry_storage_store_called = false;

    float mag[3] = {0.0f, 0.0f, 0.0f};
    data_layer_write_mag(mag);
    data_layer_write_radiation(0.0f);
    data_layer_write_payload_status(false, 0);

    set_state(FM_NOMINAL, ENERGY_NOMINAL, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 20.0f, 0, 0);
    vTelemetryTask_Step();

    CHECK(g_telemetry_storage_store_called, "store called");
    CHECK(g_telemetry_storage_last_record.radiation_dose == 0.0f, "no radiation");
    CHECK(g_telemetry_storage_last_record.image_count == 0, "no images");
    CHECK(g_telemetry_storage_last_record.payload_rail_enabled == 0, "rail disabled");
  }

  printf("[T-TLM-10] test_tlm_flash_record_payload_hk: %s\n",
         g_failures == 0 ? "PASS" : "FAIL");
}

/* ========================================================================
 * T-TLM-06  No csp_sendto when csp_buffer_get returns NULL
 * ======================================================================== */
static void test_tlm_null_buffer(void)
{
  int failures_before = g_failures;
  reset_all();
  s_buffer_fail = 1;
  set_state(FM_NOMINAL, ENERGY_NOMINAL, 1, 2, 3, 4, 5, 6, 20.0f, 1, 1);

  vTelemetryTask_Step();

  CHECK(s_send_calls == 0, "csp_sendto not called when buffer NULL");

  printf("[T-TLM-06] test_tlm_null_buffer: %s\n", g_failures == failures_before ? "PASS" : "FAIL");
}

/* ========================================================================
 * T-TLM-09  sizeof assertions for FR-17 payload HK structs
 * ======================================================================== */
_Static_assert(sizeof(csp_telemetry_packet_t) == 98,
               "csp_telemetry_packet_t must be 98 bytes after FR-17");
_Static_assert(sizeof(telemetry_packet_t) == 64,
               "telemetry_packet_t must be 64 bytes after FR-17");
_Static_assert(TLM_PACKET_SIZE == 64,
               "TLM_PACKET_SIZE must be 64 after FR-17");
_Static_assert(sizeof(telemetry_record_t) == 76,
               "telemetry_record_t must be 76 bytes after FR-17");

/* ======================================================================== */
int main(void)
{
  printf("=== Telemetry Task Unit Tests ===\n");
  test_tlm_full_packet_in_nominal();
  test_tlm_hk_only_in_safe();
  test_tlm_sends_in_all_modes();
  test_tlm_energy_state_in_flags();
  test_tlm_flags_imu_temp();
  test_tlm_null_buffer();
  test_tlm_payload_hk_in_csp();
  test_tlm_payload_hk_defaults_zero();
  test_tlm_sizeof_assertions();
  test_tlm_flash_record_payload_hk();
  printf("=================================\n");
  if (g_failures == 0)
  {
    printf("All tests passed!\n");
    return 0;
  }
  else
  {
    printf("%d test(s) FAILED.\n", g_failures);
    return 1;
  }
}
