/**
 * @file data_layer.c
 * @brief Data Layer Abstraction implementation.
 *
 * Single authoritative store for all shared satellite state.
 * A FreeRTOS mutex (PICO_BUILD) or no-op (host unit tests) serialises
 * all accesses.
 *
 * Spec ref: SPEC-2-DLA v1.6, SPEC-2 v2.0 §4.5
 */

#include "data_layer.h"

#include "system_state.h"

#include <string.h>

#ifdef PICO_BUILD
  #include "FreeRTOS.h"
  #include "semphr.h"
static SemaphoreHandle_t g_dl_mutex = NULL;
#endif

/* ------------------------------------------------------------------ */
/* Internal state                                                      */
/* ------------------------------------------------------------------ */

static dl_snapshot_t g_snapshot;

/* ------------------------------------------------------------------ */
/* Mutex helpers                                                       */
/* ------------------------------------------------------------------ */

static void dl_lock(void)
{
#ifdef PICO_BUILD
  if (g_dl_mutex)
  {
    xSemaphoreTake(g_dl_mutex, portMAX_DELAY);
  }
#endif
}

static void dl_unlock(void)
{
#ifdef PICO_BUILD
  if (g_dl_mutex)
  {
    xSemaphoreGive(g_dl_mutex);
  }
#endif
}

/* ------------------------------------------------------------------ */
/* Initialisation                                                      */
/* ------------------------------------------------------------------ */

void data_layer_init(void)
{
  (void)memset(&g_snapshot, 0, sizeof(dl_snapshot_t));
  g_snapshot.mode = FM_BOOT;
  g_snapshot.energy = ENERGY_NOMINAL;

#ifdef PICO_BUILD
  g_dl_mutex = xSemaphoreCreateMutex();
#endif

  /* Keep system_state module consistent — it now delegates here. */
  /* (system_state_init is a thin shim; calling it is a no-op.) */
}

/* ------------------------------------------------------------------ */
/* Read                                                                */
/* ------------------------------------------------------------------ */

void data_layer_read(dl_snapshot_t *out)
{
  if (!out)
  {
    return;
  }
  dl_lock();
  (void)memcpy(out, &g_snapshot, sizeof(dl_snapshot_t));
  dl_unlock();
}

/* ------------------------------------------------------------------ */
/* Write — sensor data                                                 */
/* ------------------------------------------------------------------ */

void data_layer_write_imu(const float att_rad[3], const float rates_rad[3])
{
  dl_lock();
  for (int i = 0; i < 3; i++)
  {
    g_snapshot.state.attitude[i] = att_rad[i];
    g_snapshot.state.rates[i] = rates_rad[i];
  }
  g_snapshot.state.imu_valid = true;
  g_snapshot.seq++;
  dl_unlock();
}

#include "quaternion.h"

void data_layer_write_ekf(const float q[4], const float bias_rad[3], const float cov_diag[7])
{
  dl_lock();

  /* Copy quaternion and gyro bias */
  for (int i = 0; i < 4; i++)
  {
    g_snapshot.state.q[i] = q[i];
  }
  for (int i = 0; i < 3; i++)
  {
    g_snapshot.state.gyro_bias[i] = bias_rad[i];
  }

  /* Copy full diagonal covariance (7 states) */
  for (int i = 0; i < 4; i++)
  {
    /* We reuse att_uncertainty[4] for the quaternion part of the cov diag if we want,
     * but system_state.h has float att_uncertainty[4].
     * Actually, let's just copy exactly what's available. */
    g_snapshot.state.att_uncertainty[i] = cov_diag[i];
  }

  /* Auto-convert quaternion to Euler for telemetry/legacy subsystems */
  quat_t qt = {q[0], q[1], q[2], q[3]};
  q_to_euler(qt, &g_snapshot.state.attitude[0], &g_snapshot.state.attitude[1],
             &g_snapshot.state.attitude[2]);

  g_snapshot.state.imu_ekf_valid = true;
  g_snapshot.seq++;
  dl_unlock();
}

void data_layer_write_temp(float temp_c)
{
  dl_lock();
  g_snapshot.state.temp = temp_c;
  g_snapshot.state.temp_valid = true;
  g_snapshot.seq++;
  dl_unlock();
}

void data_layer_set_sensor_avail(bool imu, bool temp)
{
  dl_lock();
  g_snapshot.state.imu_available = imu;
  g_snapshot.state.temp_available = temp;
  dl_unlock();
}

void data_layer_write_mag(const float field_uT[3])
{
  dl_lock();
  for (int i = 0; i < 3; i++)
  {
    g_snapshot.state.mag_field[i] = field_uT[i];
  }
  g_snapshot.state.mag_valid = true;
  g_snapshot.seq++;
  dl_unlock();
}

void data_layer_set_mag_avail(bool mag)
{
  dl_lock();
  g_snapshot.state.mag_available = mag;
  dl_unlock();
}

void data_layer_write_radiation(float dose)
{
  dl_lock();
  g_snapshot.state.radiation_dose = dose;
  g_snapshot.seq++;
  dl_unlock();
}

void data_layer_write_payload_status(bool rail_enabled, uint16_t img_count)
{
  dl_lock();
  g_snapshot.state.payload_rail_enabled = rail_enabled;
  g_snapshot.state.image_count = img_count;
  g_snapshot.seq++;
  dl_unlock();
}

/* ------------------------------------------------------------------ */
/* Write — flight-level state                                          */
/* ------------------------------------------------------------------ */

void data_layer_set_flight_mode(flight_mode_t mode)
{
  dl_lock();
  g_snapshot.mode = mode;
  g_snapshot.seq++;
  dl_unlock();
}

void data_layer_set_energy_state(energy_state_t energy)
{
  dl_lock();
  g_snapshot.energy = energy;
  g_snapshot.seq++;
  dl_unlock();
}

/* ------------------------------------------------------------------ */
/* Fast single-field accessors                                         */
/* ------------------------------------------------------------------ */

flight_mode_t data_layer_get_flight_mode(void)
{
  dl_lock();
  flight_mode_t m = g_snapshot.mode;
  dl_unlock();
  return m;
}

energy_state_t data_layer_get_energy_state(void)
{
  dl_lock();
  energy_state_t e = g_snapshot.energy;
  dl_unlock();
  return e;
}

uint32_t data_layer_get_seq(void)
{
  dl_lock();
  uint32_t s = g_snapshot.seq;
  dl_unlock();
  return s;
}
