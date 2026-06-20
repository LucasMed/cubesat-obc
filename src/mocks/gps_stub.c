// gps_stub.c -- Implementation of GPS stub driver for tests
#include "gps_stub.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

static GpsFix_t g_last_fix = {.lat = -34.6037f,
                              .lon = -58.3816f,
                              .alt_m = 20.5f,
                              .hdop = 1.0f,
                              .utc_time = 1713350400,
                              .satellites = 8,
                              .valid = true,
                              .timestamp_ms = 0};

static GpsMockMode_t g_mock_mode = GPS_MOCK_OK;
static GpsStats_t g_stats = {0};
static uint32_t g_call_counts[12] = {0};
static const char *function_names[12] = {"gps_init",
                                         "gps_deinit",
                                         "gps_read_fix",
                                         "gps_get_last_fix",
                                         "gps_is_fix_valid",
                                         "gps_get_satellites_in_view",
                                         "gps_mock_set_data",
                                         "gps_mock_set_mode",
                                         "gps_mock_get_call_count",
                                         "gps_mock_reset_counters",
                                         "gps_get_stats",
                                         "gps_reset_stats"};

// Internal function to match name to index
static int lookup_func_index(const char *name)
{
  for (int i = 0; i < 12; i++)
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
  g_mock_mode = GPS_MOCK_OK;
  memset(g_call_counts, 0, sizeof(g_call_counts));
  g_call_counts[0]++;
  // Always succeeds for stub
  return true;
}

void gps_deinit(void)
{
  g_call_counts[1]++;
  g_mock_mode = GPS_MOCK_OK;
}

GpsFix_t *gps_read_fix(void)
{
  g_call_counts[2]++;
  switch (g_mock_mode)
  {
  case GPS_MOCK_OK:
    break;
  case GPS_MOCK_FAULT_TIMEOUT:
  case GPS_MOCK_FAULT_NO_FIX:
  case GPS_MOCK_FAULT_CHECKSUM:
  case GPS_MOCK_FAULT_PARTIAL_FRAME:
  case GPS_MOCK_FAULT_STALE_DATA:
    g_last_fix.valid = false;
    g_last_fix.satellites = 0;
    break;
  }
  return &g_last_fix;
}

bool gps_get_last_fix(GpsFix_t *out)
{
  g_call_counts[3]++;
  if (out == NULL)
  {
    return false;
  }
  memcpy(out, &g_last_fix, sizeof(GpsFix_t));
  return out->valid;
}

bool gps_is_fix_valid(void)
{
  g_call_counts[4]++;
  // Only stale if timestamp_ms >= 5000
  if (g_last_fix.timestamp_ms >= 5000)
  {
    return false;
  }
  return g_last_fix.valid;
}

uint8_t gps_get_satellites_in_view(void)
{
  g_call_counts[5]++;
  return g_last_fix.satellites;
}

const GpsStats_t *gps_get_stats(void)
{
  g_call_counts[10]++;
  return &g_stats;
}

void gps_reset_stats(void)
{
  g_call_counts[11]++;
  memset(&g_stats, 0, sizeof(g_stats));
}

void gps_mock_set_data(const GpsFix_t *fix)
{
  g_call_counts[6]++;
  if (!fix)
  {
    return;
  }
  memcpy(&g_last_fix, fix, sizeof(GpsFix_t));
}

void gps_mock_set_mode(GpsMockMode_t mode)
{
  g_call_counts[7]++;
  g_mock_mode = mode;
  if (mode == GPS_MOCK_OK)
  {
    g_last_fix.lat = -34.6037f;
    g_last_fix.lon = -58.3816f;
    g_last_fix.alt_m = 20.5f;
    g_last_fix.hdop = 1.0f;
    g_last_fix.utc_time = 1713350400;
    g_last_fix.satellites = 8;
    g_last_fix.valid = true;
    g_last_fix.timestamp_ms = 0;
  }
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
