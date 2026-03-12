#include "drivers/i2c_interface.h"
#include "drivers/imu/mpu6050.h"

#include <stdio.h>

#ifdef PICO_BUILD
  #include "config.h"
  #include "pico/stdlib.h"
#endif

void run_i2c_test()
{
  printf("\n=== I2C / MPU6050 Integration Test ===\n");

#ifdef PICO_BUILD
  // Initialize I2C with default pins
  printf("Initializing I2C bus (SDA: %d, SCL: %d)...\n", I2C_SDA_PIN, I2C_SCL_PIN);
  i2c_bus_init(I2C_SDA_PIN, I2C_SCL_PIN, 400000);
#endif

  printf("Initializing MPU6050...\n");
  if (mpu6050_init() < 0)
  {
    printf("FAILED: MPU6050 initialization failed!\n");
    return;
  }
  printf("SUCCESS: MPU6050 initialized.\n");

  printf("Reading sensor data (5 samples)...\n");
  for (int i = 0; i < 5; i++)
  {
    float accel[3], gyro[3];
    if (mpu6050_read_raw(accel, gyro) == 0)
    {
      printf("Sample %d: Accel[%.2f, %.2f, %.2f] G, Gyro[%.2f, %.2f, %.2f] deg/s\n", i, accel[0],
             accel[1], accel[2], gyro[0], gyro[1], gyro[2]);
    }
    else
    {
      printf("Sample %d: FAILED to read\n", i);
    }
#ifdef PICO_BUILD
    sleep_ms(500);
#endif
  }

  printf("=== Test Complete ===\n");
}

int main()
{
#ifdef PICO_BUILD
  stdio_init_all();
  // Wait for USB serial connection
  for (int i = 0; i < 10; i++)
  {
    printf("Waiting for USB... %d\n", 10 - i);
    sleep_ms(1000);
  }
#endif

  run_i2c_test();

  while (1)
  {
#ifdef PICO_BUILD
    tight_loop_contents();
#else
    break;
#endif
  }
  return 0;
}
