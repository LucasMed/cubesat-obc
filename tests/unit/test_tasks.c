#include "attitude_control_task.h"
#include "health_monitor_task.h"
#include "sensor_read_task.h"
#include "system_state.h"
#include "telemetry_task.h"

#include <assert.h>
#include <csp/csp.h>
#include <stdio.h>

/* SHT31 stub globals (defined in sht31_stub.c, linked via drivers_lib) */
extern bool   s_sht31_fetch_ret;
extern float  s_sht31_fetch_temp;
extern float  s_sht31_fetch_humid;

/* BH1750 stub — satisfies sensor_read_task.c link */
// NOLINTNEXTLINE(readability-non-const-parameter)
bool bh1750_read(float *lux)
{
  (void)lux;
  return false;
}

void test_system_integration(void)
{
  printf("Running test_system_integration...\n");

  // 1. Initialize State
  system_state_init();
  csp_init();

  // Mark sensors as available so vSensorReadTask_Step() is not gated out
  system_state_set_available(true, true);

  // 2. Initial state check (data valid flags still false before first read)
  system_state_t state;
  system_state_get(&state);
  assert(state.imu_valid == false);
  assert(state.temp_valid == false);

  // 3. Configure SHT31 stub to simulate a successful temperature read
  s_sht31_fetch_ret  = true;
  s_sht31_fetch_temp = 25.0f;
  s_sht31_fetch_humid = 0.0f;

  // 4. Run sensor read step (uses host i2c/stub mocks)
  vSensorReadTask_Step();

  // 5. Verify state updated
  system_state_get(&state);
  assert(state.imu_valid == true);
  assert(state.temp_valid == true);
  // Based on host mocks: IMU ID 0x68 means mock is "working"; temp comes from
  // SHT31 stub which we configured to return 25.0f
  assert(state.temp == 25.0f);

  // 5. Run control and telemetry steps
  vAttitudeControlTask_Step();
  vTelemetryTask_Step();
  vHealthMonitorTask_Step();

  printf("test_system_integration passed\n");
}

int main(void)
{
  printf("=== OBC Tasks Unit Tests ===\n");

  test_system_integration();

  printf("All tasks tests passed!\n");
  return 0;
}
