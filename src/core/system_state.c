/**
 * @file system_state.c
 * @brief Thin compatibility shim — delegates to the Data Layer (DLA).
 *
 * All storage and locking now live in data_layer.c.  This file exists
 * only to preserve the legacy system_state API used by older callers
 * while the codebase migrates to data_layer_* calls directly.
 *
 * New code MUST use data_layer.h instead of system_state.h.
 */

#include "system_state.h"

#include "data_layer.h"

void system_state_init(void)
{
  data_layer_init();
}

void system_state_set_available(bool imu, bool temp)
{
  data_layer_set_sensor_avail(imu, temp);
}

void system_state_set_imu(const float att[3], const float rates[3])
{
  /* att[] and rates[] are expected in radians / rad/s (SPEC-2-DLA §2.4). */
  data_layer_write_imu(att, rates);
}

void system_state_set_temp(float temp)
{
  data_layer_write_temp(temp);
}

void system_state_get(system_state_t *out_state)
{
  if (!out_state)
  {
    return;
  }
  dl_snapshot_t snap;
  data_layer_read(&snap);
  *out_state = snap.state;
}
