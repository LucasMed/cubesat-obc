/**
 * @file camera_driver.h
 * @brief Arducam OV2640 Camera Driver API (SPI & I2C interface).
 *
 * This driver handles the Arducam Mini OV2640 2MP module.
 * - Bus I2C1 (GPIO 2/3): Used for register configuration.
 * - Bus SPI0 (GPIO 16/18/19): Shared payload bus used for image data readout.
 * - CS on GPIO 14 (SPI_CS_CAM_PIN).
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
#define OV2640_CHIPID_HIGH 0x26
#define OV2640_CHIPID_LOW 0x42

/** Arducam Registers (over SPI) */
#define ARDUCHIP_TEST1 0x00
#define ARDUCHIP_FIFO 0x04
#define ARDUCHIP_FIFO_2 0x01
#define ARDUCHIP_TRIG 0x41
#define FIFO_SIZE_1 0x42
#define FIFO_SIZE_2 0x43
#define FIFO_SIZE_3 0x44
#define BURST_READ_FIFO 0x3C

  /** Image resolution & format */
  typedef enum
  {
    CAM_RES_160x120,
    CAM_RES_320x240,
    CAM_RES_640x480,
    CAM_RES_800x600,
    CAM_RES_1024x768,
    CAM_RES_1280x960,
    CAM_RES_1600x1200
  } camera_res_t;

  /* ------------------------------------------------------------------ */
  /* Public API                                                          */
  /* ------------------------------------------------------------------ */

  /**
   * @brief Initialize the camera hardware.
   *
   * Initializes I2C1 and SPI0 (via spi_payload_init), verifies sensor IDs,
   * and sets the default resolution.
   *
   * @return true if communication is verified and initialization succeeds.
   */
  bool camera_init(void);

  /**
   * @brief Configure the camera resolution and format.
   *
   * @param res  Target resolution.
   * @return true on success.
   */
  bool camera_set_resolution(camera_res_t res);

  /**
   * @brief Trigger an image capture.
   *
   * This pulses the trigger line and monitors the FIFO ready status.
   *
   * @param timeout_ms  Maximum wait time for capture completion.
   * @return true if capture finished successfully.
   */
  bool camera_capture(uint32_t timeout_ms);

  /**
   * @brief Return the number of bytes currently in the camera FIFO.
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

#ifdef __cplusplus
}
#endif

#endif /* CAMERA_DRIVER_H */
