/**
 * @file hmc5883l.c
 * @brief HMC5883L magnetometer driver — host stub / RP2040 implementation.
 *
 * Host stub (PICO_BUILD not defined):
 *   hmc5883l_init() is a no-op that returns 0.
 *   hmc5883l_read() returns a fixed representative field vector
 *   {25.0f, 0.0f, 42.0f} µT so that all host unit tests and simulation
 *   runs receive a non-zero, physically plausible field without hardware.
 *
 * RP2040 (PICO_BUILD defined):
 *   Real I2C implementation using i2c_interface.
 *
 * Spec ref: HMC5883L datasheet Rev D, PHASE5_PLAN PR-18
 */

#include "drivers/mag/hmc5883l.h"
#include "drivers/i2c_interface.h"

#ifdef PICO_BUILD
#include <stdio.h>

#define HMC5883L_ADDR 0x1E

#define HMC5883L_CRA   0x00
#define HMC5883L_CRB   0x01
#define HMC5883L_MODE  0x02
#define HMC5883L_DATA  0x03
#define HMC5883L_STATUS 0x09

#define HMC5883L_CRA_CONFIG  0x70
#define HMC5883L_CRB_GAIN    0x20
#define HMC5883L_MODE_CONT   0x00

#define HMC5883L_GAIN_LSB 1090.0f

int hmc5883l_init(void)
{
  uint8_t data[2];

  data[0] = HMC5883L_CRA;
  data[1] = HMC5883L_CRA_CONFIG;
  if (i2c_bus_write(HMC5883L_ADDR, data, 2u) < 0)
  {
    (void)printf("hmc5883l: Failed to write CRA\n");
    return -1;
  }

  data[0] = HMC5883L_CRB;
  data[1] = HMC5883L_CRB_GAIN;
  if (i2c_bus_write(HMC5883L_ADDR, data, 2u) < 0)
  {
    (void)printf("hmc5883l: Failed to write CRB\n");
    return -1;
  }

  data[0] = HMC5883L_MODE;
  data[1] = HMC5883L_MODE_CONT;
  if (i2c_bus_write(HMC5883L_ADDR, data, 2u) < 0)
  {
    (void)printf("hmc5883l: Failed to write Mode\n");
    return -1;
  }

  uint8_t mode_reg;
  if (i2c_bus_write_read(HMC5883L_ADDR, (uint8_t[]){HMC5883L_MODE}, 1, &mode_reg, 1) < 0)
  {
    (void)printf("hmc5883l: Failed to verify Mode register\n");
    return -1;
  }

  if ((mode_reg & 0x03) != HMC5883L_MODE_CONT)
  {
    (void)printf("hmc5883l: Mode register verification failed: 0x%02X\n", mode_reg);
    return -1;
  }

  (void)printf("hmc5883l: initialized successfully\n");
  return 0;
}

int hmc5883l_read(float field_uT[3])
{
  uint8_t buffer[6];
  uint8_t reg = HMC5883L_DATA;

  if (i2c_bus_write_read(HMC5883L_ADDR, &reg, 1, buffer, 6) < 0)
  {
    return -1;
  }

  int16_t raw_x = (int16_t)(((uint16_t)buffer[0] << 8u) | (uint16_t)buffer[1]);
  int16_t raw_z = (int16_t)(((uint16_t)buffer[2] << 8u) | (uint16_t)buffer[3]);
  int16_t raw_y = (int16_t)(((uint16_t)buffer[4] << 8u) | (uint16_t)buffer[5]);

  field_uT[0] = (float)raw_x / HMC5883L_GAIN_LSB * 100.0f;
  field_uT[1] = (float)raw_y / HMC5883L_GAIN_LSB * 100.0f;
  field_uT[2] = (float)raw_z / HMC5883L_GAIN_LSB * 100.0f;

  return 0;
}
#else
int hmc5883l_init(void)
{
  return 0;
}

int hmc5883l_read(float field_uT[3])
{
  field_uT[0] = 25.0f;
  field_uT[1] = 0.0f;
  field_uT[2] = 42.0f;
  return 0;
}
#endif
