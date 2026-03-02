/**
 * @file hmc5883l.c
 * @brief HMC5883L magnetometer driver — host stub / RP2040 placeholder.
 *
 * Host stub (PICO_BUILD not defined):
 *   hmc5883l_init() is a no-op that returns 0.
 *   hmc5883l_read() returns a fixed representative field vector
 *   {25.0f, 0.0f, 42.0f} µT so that all host unit tests and simulation
 *   runs receive a non-zero, physically plausible field without hardware.
 *
 * RP2040 (PICO_BUILD defined):
 *   The real I2C implementation will replace the stub bodies in a future
 *   hardware-integration PR.  The PICO_BUILD guard is present here as a
 *   placeholder to keep the file compiling in both environments.
 *
 * Spec ref: HMC5883L datasheet Rev D, PHASE5_PLAN PR-18
 */

#include "drivers/mag/hmc5883l.h"

#ifdef PICO_BUILD
  /* TODO: include pico I2C headers and implement real init/read. */
  #include "pico/stdlib.h"
#endif

int hmc5883l_init(void)
{
#ifdef PICO_BUILD
  /* TODO: configure continuous-measurement mode via I2C. */
  return 0;
#else
  /* Host stub — always succeeds. */
  return 0;
#endif
}

int hmc5883l_read(float field_uT[3])
{
#ifdef PICO_BUILD
  /* TODO: burst-read 6 bytes from register 0x03, scale to µT. */
  field_uT[0] = 0.0f;
  field_uT[1] = 0.0f;
  field_uT[2] = 0.0f;
  return 0;
#else
  /* Host stub: representative low-Earth-orbit magnetic field. */
  field_uT[0] = 25.0f;
  field_uT[1] = 0.0f;
  field_uT[2] = 42.0f;
  return 0;
#endif
}
