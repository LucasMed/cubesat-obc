/**
 * @file telemetry_storage_stub.c
 * @brief Host stub for telemetry storage
 */

#include "telemetry_storage.h"

#include <string.h>

bool telemetry_storage_init(void)
{
  return true;
}

bool telemetry_storage_store(const telemetry_record_t *record)
{
  (void)record;
  return true;
}

bool telemetry_storage_read(uint32_t address, telemetry_record_t *record)
{
  (void)address;
  (void)record;
  return true;
}

void telemetry_storage_get_stats(telemetry_storage_stats_t *stats)
{
  if (stats != NULL)
  {
    memset(stats, 0, sizeof(telemetry_storage_stats_t));
    stats->initialized = true;
  }
}

uint32_t telemetry_storage_available(void)
{
  return 0;
}

bool telemetry_storage_is_available(void)
{
  return true;
}

bool telemetry_storage_clear(void)
{
  return true;
}

uint32_t telemetry_storage_get_base_addr(void)
{
  return 0;
}
