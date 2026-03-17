// gps_stub.c -- Implementation of GPS stub driver for tests
// All comments in English
#include "gps_stub.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

static GpsFix_t g_last_fix = {.lat = -34.6037f,
                              .lon = -58.3816f,
                              .alt_m = 20.5f,
                              .utc_time = 1713350400,
                              .valid = true,
                              .timestamp_ms = 0,
                              .satellites_in_view = 8};

static GpsMockMode_t g_mock_mode = GPS_MOCK_OK;
static uint32_t g_call_counts[10] = {
    0};  // Index: 0-init, 1-deinit, 2-read_fix, 3-get_last_fix, 4-is_fix_valid, 5-get_sats,
         // 6-set_data, 7-set_mode, 8-get_call_count, 9-reset_counters
static const char *function_names[10] = {"gps_init",
                                         "gps_deinit",
                                         "gps_read_fix",
                                         "gps_get_last_fix",
                                         "gps_is_fix_valid",
                                         "gps_get_satellites_in_view",
                                         "gps_mock_set_data",
                                         "gps_mock_set_mode",
                                         "gps_mock_get_call_count",
                                         "gps_mock_reset_counters"};

// Internal function to match name to index
static int lookup_func_index(const char *name)
{
  for (int i = 0; i < 10; i++)
  {
    if (strcmp(name, function_names[i]) == 0)
    {
      return i;
    }
  }
  return -1;
}

bool gps_init(void)
{
  g_call_counts[0]++;
  // Always succeeds for stub
  return true;
}

void gps_deinit(void)
{
  g_call_counts[1]++;
  memset(&g_last_fix, 0, sizeof(GpsFix_t));
  g_mock_mode = GPS_MOCK_OK;
}

GpsFix_t *gps_read_fix(void)
{
  g_call_counts[2]++;
  // Return fix depending on mock mode
  switch (g_mock_mode)
  {
  case GPS_MOCK_OK:
    g_last_fix.valid = true;
    break;
  case GPS_MOCK_FAULT_TIMEOUT:
    g_last_fix.valid = false;
    break;
  case GPS_MOCK_FAULT_NO_FIX:
    g_last_fix.valid = false;
    g_last_fix.satellites_in_view = 0;
    break;
  case GPS_MOCK_FAULT_CHECKSUM:
  case GPS_MOCK_FAULT_PARTIAL_FRAME:
  case GPS_MOCK_FAULT_STALE_DATA:
    g_last_fix.valid = false;
    break;
  }
  return &g_last_fix;
}

GpsFix_t *gps_get_last_fix(void)
{
  g_call_counts[3]++;
  return &g_last_fix;
}

bool gps_is_fix_valid(void)
{
  g_call_counts[4]++;
  // Simple stale logic for example: timestamp_ms < 5000 is valid
  if (g_mock_mode == GPS_MOCK_FAULT_STALE_DATA || g_last_fix.timestamp_ms >= 5000)
  {
    return false;
  }
  return g_last_fix.valid;
}

uint8_t gps_get_satellites_in_view(void)
{
  g_call_counts[5]++;
  return g_last_fix.satellites_in_view;
}

void gps_mock_set_data(const GpsFix_t *fix)
{
  g_call_counts[6]++;
  memcpy(&g_last_fix, fix, sizeof(GpsFix_t));
}

void gps_mock_set_mode(GpsMockMode_t mode)
{
  g_call_counts[7]++;
  g_mock_mode = mode;
}

uint32_t gps_mock_get_call_count(const char *func_name)
{
  g_call_counts[8]++;
  int idx = lookup_func_index(func_name);
  if (idx >= 0)
  {
    return g_call_counts[idx];
  }
  return 0;
}

void gps_mock_reset_counters(void)
{
  g_call_counts[9]++;
  memset(g_call_counts, 0, sizeof(g_call_counts));
}
