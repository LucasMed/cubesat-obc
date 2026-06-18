/**
 * @file camera_driver.h
 * @brief Arducam OV2640 Camera Driver API (SPI & I2C interface).
 *
 * This driver handles the 8-pin Arducam Mini OV2640 2MP module (SPI+I2C only).
 * - Bus I2C1 (GPIO 2/3): Used for register configuration (SCCB).
 * - Bus SPI0 (GPIO 14/17/18/19): CS + MISO + SCK + MOSI for image data readout.
 * - I2C address 0x30 (7-bit) / 0x60 (8-bit).
 * - No RESET, TRIG, or FIFO_RDY pins — handled via SCCB/SPI registers.
 *
 * Spec ref: ICD-PAYLOAD-001 §11.4, §11.16
 */

#ifndef CAMERA_DRIVER_H
#define CAMERA_DRIVER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

/* ------------------------------------------------------------------ */
/* Constants                                                           */
/* ------------------------------------------------------------------ */

/** OV2640 Sensor IDs */
#define OV2640_CHIPID_HIGH 0x26 /* Manufacturer ID (OmniVision) */
#define OV2640_CHIPID_LOW 0x42  /* Product ID — some clones return 0x41, also accepted */

/** OV2640 SCCB (I2C) address — 7-bit */
#define OV2640_I2C_ADDR 0x30

/** Arducam Registers (over SPI) */
#define ARDUCHIP_TEST1 0x00
#define ARDUCHIP_FIFO 0x04
#define ARDUCHIP_GPIO_DIR                                                                          \
  0x05 /**< GPIO direction: bit[0]=sensor RST, bit[1]=sensor PD, bit[2]=sensor PWR_EN */
#define ARDUCHIP_GPIO_WR                                                                           \
  0x06 /**< GPIO write: bit[0]=sensor RST val, bit[1]=PD val, bit[2]=PWR_EN val */
#define ARDUCHIP_STATUS 0x07 /**< bit 0 = FIFO_RDY (capture complete), bit 2 = FIFO_EMPTY */
#define ARDUCHIP_FIFO_2 0x01
#define ARDUCHIP_GPIO_RD 0x45 /**< GPIO read back */

/** GPIO bit masks for sensor control via CPLD */
#define GPIO_SENSOR_RST 0x01
#define GPIO_SENSOR_PD 0x02
#define GPIO_SENSOR_PWR 0x04
#define ARDUCHIP_TRIG 0x41
#define CAP_DONE_MASK 0x08 /**< bit 3 = capture complete in ARDUCHIP_TRIG */
#define FIFO_SIZE_1 0x42
#define FIFO_SIZE_2 0x43
#define FIFO_SIZE_3 0x44
#define BURST_READ_FIFO 0x3C

/** Arducam timing control register (SPI register 0x03 — NOT FIFO_SIZE3 at 0x44) */
#define ARDUCHIP_TIM 0x03
#define VSYNC_LEVEL_MASK 0x02 /**< 1 = VSYNC active low, 0 = active high */
#define VSYNC_EDGE_POS 0x01
#define HREF_LEVEL_MASK 0x01 /**< 1 = HREF active low, 0 = active high */

/** SPI protocol: bit 7 = 1 for write, bit 7 = 0 for read */
#define ARDUCAM_SPI_WRITE 0x80

/** Arducam FIFO status bits */
#define STATUS_FIFO_RDY 0x01
#define STATUS_FIFO_FULL 0x02
#define STATUS_FIFO_EMPTY 0x04

  /** Image resolution & format */
  typedef enum
  {
    CAM_RES_160x120,
    CAM_RES_176x144,
    CAM_RES_320x240,
    CAM_RES_352x288,
    CAM_RES_640x480,
    CAM_RES_800x600,
    CAM_RES_1024x768,
    CAM_RES_1280x1024,
    CAM_RES_1600x1200
  } camera_res_t;

  /* ------------------------------------------------------------------ */
  /* Public API                                                          */
  /* ------------------------------------------------------------------ */

  /**
   * @brief Initialize the camera hardware.
   *
   * Initializes I2C1 (GPIO2/3) and SPI0 (via spi_payload_init), verifies
   * sensor IDs, writes the OV2640 JPEG init register sequence, and sets
   * the default resolution (UXGA 1600x1200 JPEG).
   *
   * @return true if communication is verified and initialization succeeds.
   */
  bool camera_init(void);

  /**
   * @brief Configure the camera resolution and format.
   *
   * Writes the resolution-specific OV2640 DSP register sequence over I2C.
   * Must be called after camera_init().
   *
   * @param res  Target resolution.
   * @return true on success.
   */
  bool camera_set_resolution(camera_res_t res);

  /**
   * @brief Trigger an image capture.
   *
   * Clears the FIFO, starts a capture via SPI, and polls the
   * ARDUCHIP_STATUS register (0x07, bit 0 = FIFO_RDY) for completion.
   * No external FIFO_RDY pin needed.
   *
   * @param timeout_ms  Maximum wait time for capture completion.
   * @return true if capture finished successfully.
   */
  bool camera_capture(uint32_t timeout_ms);

  /**
   * @brief Return the number of bytes currently in the camera FIFO.
   *
   * Reads three 8-bit FIFO size registers over SPI and combines them.
   *
   * @return FIFO length in bytes.
   */
  uint32_t camera_get_fifo_length(void);

  /**
   * @brief Read a chunk of data from the camera FIFO.
   *
   * This uses the SPI burst read mode for high-speed data transfer.
   *
   * @param[out] buffer  Destination buffer.
   * @param      length  Number of bytes to read.
   * @return true if the SPI read succeeded.
   */
  bool camera_read_fifo_burst(uint8_t *buffer, size_t length);

  /**
   * @brief Clear the camera FIFO and reset for the next capture.
   */
  void camera_clear_fifo(void);

  /**
   * @brief Write a SENSOR bank register on the OV2640 via I2C.
   *
   * Writes 0xFF=0x01 (ensure SENSOR bank) then the register.
   * Safe to call at any time — does not switch banks if already in SENSOR.
   *
   * @param reg  Register address.
   * @param val  Value to write.
   * @return true if write acknowledged.
   */
  bool camera_write_sensor_reg(uint8_t reg, uint8_t val);

  /**
   * @brief Read a SENSOR bank register on the OV2640 via I2C.
   *
   * Ensures SENSOR bank before reading.  Does NOT switch to DSP bank.
   *
   * @param reg  Register address.
   * @param[out] val  Read value.
   * @return true if read succeeded.
   */
  bool camera_read_sensor_reg(uint8_t reg, uint8_t *val);

#ifdef __cplusplus
}
#endif

#endif /* CAMERA_DRIVER_H */
