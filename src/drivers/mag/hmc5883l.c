/**
 * @file hmc5883l.c
 * @brief HMC5883L / QMC5883L magnetometer driver — host stub / RP2040 implementation.
 *
 * Supports both HMC5883L (Honeywell) and QMC5883L (QST) magnetometers.
 * Auto-detects which chip is present by probing I2C addresses.
 *
 * Host stub (PICO_BUILD not defined):
 *   hmc5883l_init() is a no-op that returns 0.
 *   hmc5883l_read() returns a fixed representative field vector
 *   {25.0f, 0.0f, 42.0f} µT so that all host unit tests and simulation
 *   runs receive a non-zero, physically plausible field without hardware.
 *
 * RP2040 (PICO_BUILD defined):
 *   Real I2C implementation using i2c_interface.
 *   Auto-detects HMC5883L (0x1E) or QMC5883L (0x0D/0x2C).
 *
 * Spec ref: HMC5883L datasheet Rev D, QMC5883L datasheet
 */

#include "drivers/mag/hmc5883l.h"

#include "drivers/i2c_interface.h"

#ifdef PICO_BUILD
  #include <stdio.h>

  /* I2C addresses */
  #define HMC5883L_ADDR 0x1E
  #define QMC5883L_ADDR 0x0D
  #define QMC5883L_ADDR_ALT 0x2C /* QMC5883L with AD0 pulled high */

  /* HMC5883L registers */
  #define HMC5883L_CRA 0x00
  #define HMC5883L_CRB 0x01
  #define HMC5883L_MODE 0x02
  #define HMC5883L_DATA 0x03
  #define HMC5883L_STATUS 0x09

  #define HMC5883L_CRA_CONFIG 0x70
  #define HMC5883L_CRB_GAIN 0x20
  #define HMC5883L_MODE_CONT 0x00

  #define HMC5883L_GAIN_LSB 1090.0f

  /* QMC5883L registers */
  #define QMC5883L_X_L 0x00
  #define QMC5883L_X_H 0x01
  #define QMC5883L_Y_L 0x02
  #define QMC5883L_Y_H 0x03
  #define QMC5883L_Z_L 0x04
  #define QMC5883L_Z_H 0x05
  #define QMC5883L_STATUS 0x06
  #define QMC5883L_TEMP_L 0x07
  #define QMC5883L_TEMP_H 0x08
  #define QMC5883L_CONFIG 0x09
  #define QMC5883L_CONFIG2 0x0A
  #define QMC5883L_ID 0x0D

  #define QMC5883L_ID_VALUE 0xFF /* QMC5883L identification */
  #define QMC5883L_GAIN_LSB 1200.0f

/* Chip detection */
static uint8_t s_mag_addr = 0;
static uint8_t s_chip_type = 0; /* 0=none, 1=HMC5883L, 2=QMC5883L */

static int probe_hmc5883l(void)
{
  uint8_t data[2];

  /* Try HMC5883L at 0x1E.
   * OI-SW-1: HMC5883L has no ID register — detection is purely behavioral:
   *   1. Write CRA (reg 0x00) = 0x70 (sample rate, range config)
   *   2. Write MODE (reg 0x02) = 0x00 (continuous measurement)
   *   3. Read MODE back — must read 0x00
   * This acts as a register-map fingerprint: QMC5883L register 0x00 is
   * DATA_X_L (not CRA), so a QMC5883L at this address would not respond
   * correctly to CRA writes, and the MODE read-back would differ. */
  data[0] = HMC5883L_CRA;
  data[1] = HMC5883L_CRA_CONFIG;
  if (i2c_bus_write(HMC5883L_ADDR, data, 2u) == 0)
  {
    data[0] = HMC5883L_MODE;
    data[1] = HMC5883L_MODE_CONT;
    if (i2c_bus_write(HMC5883L_ADDR, data, 2u) == 0)
    {
      /* Verify by reading mode register */
      uint8_t mode_reg;
      if (i2c_bus_write_read(HMC5883L_ADDR, (uint8_t[]){HMC5883L_MODE}, 1, &mode_reg, 1) == 0)
      {
        if ((mode_reg & 0x03) == HMC5883L_MODE_CONT)
        {
          return 1; /* HMC5883L detected */
        }
      }
    }
  }
  return 0;
}

static int probe_qmc5883l(uint8_t addr)
{
  uint8_t data[2];

  /* Configure QMC5883L */
  data[0] = QMC5883L_CONFIG;
  data[1] = 0x01; /* Continuous mode, 50Hz, 2G range */
  if (i2c_bus_write(addr, data, 2u) == 0)
  {
    data[0] = QMC5883L_CONFIG2;
    data[1] = 0x01; /* Enable */
    if (i2c_bus_write(addr, data, 2u) == 0)
    {
      /* OI-SW-1: Read and validate ID register — warn on mismatch */
      uint8_t id;
      if (i2c_bus_write_read(addr, (uint8_t[]){QMC5883L_ID}, 1, &id, 1) == 0)
      {
        (void)printf("    qmc5883l: probe addr 0x%02X, ID=0x%02X", addr, id);
        /* OI-SW-1: Validate ID register — warn on mismatch but accept
         * for compatibility with non-genuine QMC5883L clones used
         * during development/hardware testing. */
        if (id != QMC5883L_ID_VALUE)
        {
          (void)printf(" — WARNING: expected 0x%02X, using anyway\r\n", QMC5883L_ID_VALUE);
        }
        else
        {
          (void)printf(" — matched\r\n");
        }
        return 1;
      }
    }
  }
  return 0;
}

int hmc5883l_init(void)
{
  (void)printf("    hmc5883l: probing...\r\n");

  /* Try HMC5883L first */
  if (probe_hmc5883l())
  {
    s_mag_addr = HMC5883L_ADDR;
    s_chip_type = 1;
    (void)printf("hmc5883l: detected HMC5883L at 0x%02X\r\n", HMC5883L_ADDR);
    return 0;
  }

  /* Try QMC5883L at 0x0D */
  if (probe_qmc5883l(QMC5883L_ADDR))
  {
    s_mag_addr = QMC5883L_ADDR;
    s_chip_type = 2;
    (void)printf("hmc5883l: detected QMC5883L at 0x%02X\r\n", QMC5883L_ADDR);
    return 0;
  }

  /* Try QMC5883L at 0x2C (AD0 = VCC) */
  if (probe_qmc5883l(QMC5883L_ADDR_ALT))
  {
    s_mag_addr = QMC5883L_ADDR_ALT;
    s_chip_type = 2;
    (void)printf("hmc5883l: detected QMC5883L at 0x%02X\r\n", QMC5883L_ADDR_ALT);
    return 0;
  }

  (void)printf("hmc5883l: No magnetometer detected\r\n");
  return -1;
}

int hmc5883l_read(float field_uT[3])
{
  if (s_mag_addr == 0)
  {
    return -1;
  }

  uint8_t buffer[6];

  if (s_chip_type == 1)
  {
    /* HMC5883L */
    uint8_t reg = HMC5883L_DATA;
    if (i2c_bus_write_read(s_mag_addr, &reg, 1, buffer, 6) < 0)
    {
      return -1;
    }

    int16_t raw_x = (int16_t)(((uint16_t)buffer[0] << 8u) | (uint16_t)buffer[1]);
    int16_t raw_z = (int16_t)(((uint16_t)buffer[2] << 8u) | (uint16_t)buffer[3]);
    int16_t raw_y = (int16_t)(((uint16_t)buffer[4] << 8u) | (uint16_t)buffer[5]);

    field_uT[0] = (float)raw_x / HMC5883L_GAIN_LSB * 100.0f;
    field_uT[1] = (float)raw_y / HMC5883L_GAIN_LSB * 100.0f;
    field_uT[2] = (float)raw_z / HMC5883L_GAIN_LSB * 100.0f;
  }
  else if (s_chip_type == 2)
  {
    /* QMC5883L */
    uint8_t reg = QMC5883L_X_L;
    if (i2c_bus_write_read(s_mag_addr, &reg, 1, buffer, 6) < 0)
    {
      return -1;
    }

    /* QMC5883L has different byte order */
    int16_t raw_x = (int16_t)(((uint16_t)buffer[1] << 8u) | (uint16_t)buffer[0]);
    int16_t raw_y = (int16_t)(((uint16_t)buffer[3] << 8u) | (uint16_t)buffer[2]);
    int16_t raw_z = (int16_t)(((uint16_t)buffer[5] << 8u) | (uint16_t)buffer[4]);

    field_uT[0] = (float)raw_x / QMC5883L_GAIN_LSB * 100.0f;
    field_uT[1] = (float)raw_y / QMC5883L_GAIN_LSB * 100.0f;
    field_uT[2] = (float)raw_z / QMC5883L_GAIN_LSB * 100.0f;
  }
  else
  {
    return -1;
  }

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
