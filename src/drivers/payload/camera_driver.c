/**
 * @file camera_driver.c
 * @brief Arducam OV2640 Camera Driver implementation.
 *
 * Implements register control via I2C0 and data readout via shared SPI0.
 * NOTE: Camera not yet connected/validated - using I2C0 for IMU.
 *
 * Spec ref: ICD-PAYLOAD-001 §11.16
 */

#include "camera_driver.h"

#include "pico_pins.h"
#include "spi_payload.h"

#if defined(PICO_BUILD)
  #include "hardware/gpio.h"
  #include "hardware/i2c.h"
  #include "hardware/spi.h"
  #include "pico/time.h"
#endif

#include <string.h>

/* ------------------------------------------------------------------ */
/* Register Definitions (I2C)                                         */
/* ------------------------------------------------------------------ */

#define OV2640_ADDR 0x30
#define OV2640_REG_PIDH 0x0A
#define OV2640_REG_PIDL 0x0B

/* ------------------------------------------------------------------ */
/* Internal Helpers                                                    */
/* ------------------------------------------------------------------ */

#if defined(PICO_BUILD)
static bool cam_i2c_write(uint8_t reg, uint8_t val)
{
  uint8_t buf[2] = {reg, val};
  return i2c_write_blocking(I2C0_PORT, OV2640_ADDR, buf, 2, false) == 2;
}

static bool cam_i2c_read(uint8_t reg, uint8_t *val)
{
  if (i2c_write_blocking(I2C0_PORT, OV2640_ADDR, &reg, 1, true) != 1)
  {
    return false;
  }
  return i2c_read_blocking(I2C0_PORT, OV2640_ADDR, val, 1, false) == 1;
}

static uint8_t cam_spi_transfer(uint8_t address, uint8_t value)
{
  uint8_t tx[2] = {address, value};
  uint8_t rx[2];
  spi_payload_cs_select(SPI_CS_CAM_PIN);
  spi_write_read_blocking(SPI0_PORT, tx, rx, 2);
  spi_payload_cs_deselect(SPI_CS_CAM_PIN);
  return rx[1];
}
#else
/* Host/Unit Test Mocks */
static bool __attribute__((unused)) cam_i2c_write(uint8_t reg, uint8_t val)
{
  (void)reg;
  (void)val;
  return true;
}
static bool cam_i2c_read(uint8_t reg, uint8_t *val)
{
  if (reg == OV2640_REG_PIDH)
  {
    *val = OV2640_CHIPID_HIGH;
  }
  else if (reg == OV2640_REG_PIDL)
  {
    *val = OV2640_CHIPID_LOW;
  }
  else
  {
    *val = 0;
  }
  return true;
}
static uint8_t cam_spi_transfer(uint8_t address, uint8_t value)
{
  (void)address;
  return value;
}
#endif

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */

bool camera_init(void)
{
  spi_payload_init();

#if defined(PICO_BUILD)
  /* I2C0 Init (shared with MPU-6050) */
  i2c_init(I2C0_PORT, I2C0_SPEED_HZ);
  gpio_set_function(I2C0_SDA_PIN, GPIO_FUNC_I2C);
  gpio_set_function(I2C0_SCL_PIN, GPIO_FUNC_I2C);
  gpio_pull_up(I2C0_SDA_PIN);
  gpio_pull_up(I2C0_SCL_PIN);

  /* GPIO Pins */
  gpio_init(CAM_RESET_PIN);
  gpio_set_dir(CAM_RESET_PIN, GPIO_OUT);
  gpio_put(CAM_RESET_PIN, 1);

  gpio_init(CAM_FIFO_RDY_PIN);
  gpio_set_dir(CAM_FIFO_RDY_PIN, GPIO_IN);

  gpio_init(CAM_TRIGGER_PIN);
  gpio_set_dir(CAM_TRIGGER_PIN, GPIO_OUT);
  gpio_put(CAM_TRIGGER_PIN, 0);
#endif

  /* Verify Chip ID */
  uint8_t pidh;
  uint8_t pidl;
  if (!cam_i2c_read(OV2640_REG_PIDH, &pidh) || pidh != OV2640_CHIPID_HIGH)
  {
    return false;
  }
  if (!cam_i2c_read(OV2640_REG_PIDL, &pidl) || pidl != OV2640_CHIPID_LOW)
  {
    return false;
  }

  /* Arducam SPI test register */
  cam_spi_transfer(ARDUCHIP_TEST1, 0x55);
  // uint8_t test = cam_spi_transfer(ARDUCHIP_TEST1 | 0x80, 0x00); // Read
  // if (test != 0x55) return false;

  return true;
}

bool camera_set_resolution(camera_res_t res)
{
  (void)res;
  /* Placeholder for register sequences for each resolution */
  return true;
}

bool camera_capture(uint32_t timeout_ms)
{
  /* Clear FIFO */
  camera_clear_fifo();

  /* Trigger */
  cam_spi_transfer(ARDUCHIP_FIFO, 0x01);  // Start Capture

#if defined(PICO_BUILD)
  uint32_t start = to_ms_since_boot(get_absolute_time());
  while (gpio_get(CAM_FIFO_RDY_PIN) == 0)
  {
    if (to_ms_since_boot(get_absolute_time()) - start > timeout_ms)
    {
      return false;
    }
    sleep_ms(1);
  }
#else
  (void)timeout_ms;
#endif
  return true;
}

uint32_t camera_get_fifo_length(void)
{
  uint32_t len1 = cam_spi_transfer(FIFO_SIZE_1 | 0x80, 0x00);
  uint32_t len2 = cam_spi_transfer(FIFO_SIZE_2 | 0x80, 0x00);
  uint32_t len3 = cam_spi_transfer(FIFO_SIZE_3 | 0x80, 0x00);
  return (len1 << 16) | (len2 << 8) | len3;
}

bool camera_read_fifo_burst(uint8_t *buffer, size_t length)
{
  if (!buffer || length == 0)
  {
    return false;
  }

#if defined(PICO_BUILD)
  uint8_t cmd = BURST_READ_FIFO;
  spi_payload_cs_select(SPI_CS_CAM_PIN);
  spi_write_blocking(SPI0_PORT, &cmd, 1);
  spi_read_blocking(SPI0_PORT, 0x00, buffer, length);
  spi_payload_cs_deselect(SPI_CS_CAM_PIN);
#else
  memset(buffer, 0xAA, length);
#endif
  return true;
}

void camera_clear_fifo(void)
{
  cam_spi_transfer(ARDUCHIP_FIFO, 0x01);  // Reset FIFO
}
