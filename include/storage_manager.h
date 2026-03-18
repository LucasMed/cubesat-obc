/**
 * @file storage_manager.h
 * @brief High-level file system API for payload data.
 *
 * Provides a simplified interface for storing periodic sensor data
 * and captured images on the microSD card using FatFs.
 */

#ifndef STORAGE_MANAGER_H
#define STORAGE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  /** Storage Status */
  typedef enum
  {
    STORAGE_OK = 0,
    STORAGE_ERR_INIT,
    STORAGE_ERR_MOUNT,
    STORAGE_ERR_OPEN,
    STORAGE_ERR_WRITE,
    STORAGE_ERR_FULL
  } storage_status_t;

  /**
   * @brief Initialize the storage system.
   *
   * Mounts the FATFS volume and creates base directories (LOGS, IMAGES).
   *
   * @return STORAGE_OK on success.
   */
  storage_status_t storage_init(void);

  /**
   * @brief Append a log entry to a file.
   *
   * Automatically opens, seeks to end, writes, and closes/syncs.
   *
   * @param filename  Path to the file (e.g., "/LOGS/mag.dat").
   * @param data      Pointer to the data block.
   * @param size      Size of the data block.
   * @return STORAGE_OK on success.
   */
  storage_status_t storage_append_log(const char *filename, const void *data, size_t size);

  /**
   * @brief Write an image file to disk.
   *
   * @param filename  Path to the file (e.g., "/IMAGES/cam01.jpg").
   * @param data      Pointer to the JPEG data.
   * @param size      Size of the image in bytes.
   * @return STORAGE_OK on success.
   */
  storage_status_t storage_write_image(const char *filename, const uint8_t *data, uint32_t size);

  /**
   * @brief Return free space in bytes.
   */
  uint64_t storage_get_free_space(void);

#ifdef __cplusplus
}
#endif

#endif /* STORAGE_MANAGER_H */
