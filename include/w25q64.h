/**
 * @file w25q64.h
 * @brief W25Q64 SPI Flash Memory Driver
 *
 * 8MB SPI flash memory for persistent data storage.
 * Replaces microSD card for space-qualified storage.
 *
 * Specifications:
 * - Capacity: 8MB (64Mbit)
 * - Voltage: 2.7-3.6V
 * - Interface: SPI Mode 0/3
 * - Max clock: 104MHz
 *
 * Spec ref: W25Q64JV datasheet
 */

#ifndef W25Q64_H
#define W25Q64_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  /**
   * @brief W25Q64 flash status codes
   */
  typedef enum
  {
    W25Q64_OK = 0,
    W25Q64_ERR_INIT = -1,
    W25Q64_ERR_ID = -2,
    W25Q64_ERR_WRITE = -3,
    W25Q64_ERR_ERASE = -4,
    W25Q64_ERR_TIMEOUT = -5
  } w25q64_status_t;

  /**
   * @brief Device identification
   */
  typedef struct
  {
    uint8_t manufacturer_id; /**< Expected: 0xEF (Winbond) */
    uint8_t memory_type;     /**< Expected: 0x40 */
    uint8_t capacity;        /**< Expected: 0x17 (W25Q64) */
    bool valid;              /**< True if ID matches expected values */
  } w25q64_id_t;

  /**
   * @brief Initialize the W25Q64 flash memory.
   *
   * Initializes SPI bus and verifies device ID.
   *
   * @return W25Q64_OK on success, error code otherwise
   */
  w25q64_status_t w25q64_init(void);

  /**
   * @brief Read the device JEDEC ID.
   *
   * @param id Pointer to store device identification
   * @return W25Q64_OK on success
   */
  w25q64_status_t w25q64_read_id(w25q64_id_t *id);

  /**
   * @brief Read data from flash memory.
   *
   * @param addr Starting address (0 to 8MB-1)
   * @param buf Buffer to store data
   * @param len Number of bytes to read
   * @return W25Q64_OK on success, error code otherwise
   */
  w25q64_status_t w25q64_read(uint32_t addr, uint8_t *buf, uint32_t len);

  /**
   * @brief Write data to flash memory (single page, 256 bytes).
   *
   * @param addr Starting address (must be page-aligned for >256 bytes)
   * @param buf Data to write
   * @param len Number of bytes to write (max 256)
   * @return W25Q64_OK on success, error code otherwise
   */
  w25q64_status_t w25q64_write_page(uint32_t addr, const uint8_t *buf, uint32_t len);

  /**
   * @brief Erase a 4KB sector.
   *
   * @param addr Sector start address (must be 4KB-aligned)
   * @return W25Q64_OK on success, error code otherwise
   */
  w25q64_status_t w25q64_erase_sector(uint32_t addr);

  /**
   * @brief Erase a 32KB block.
   *
   * @param addr Block start address (must be 32KB-aligned)
   * @return W25Q64_OK on success, error code otherwise
   */
  w25q64_status_t w25q64_erase_block32(uint32_t addr);

  /**
   * @brief Erase a 64KB block.
   *
   * @param addr Block start address (must be 64KB-aligned)
   * @return W25Q64_OK on success, error code otherwise
   */
  w25q64_status_t w25q64_erase_block64(uint32_t addr);

  /**
   * @brief Erase entire chip (takes ~20-50 seconds).
   *
   * @return W25Q64_OK on success, error code otherwise
   */
  w25q64_status_t w25q64_erase_chip(void);

  /**
   * @brief Read the flash status register.
   *
   * @return Status register value
   */
  uint8_t w25q64_read_status(void);

  /**
   * @brief Wait for flash to finish current operation.
   *
   * @param timeout_ms Maximum wait time in milliseconds
   * @return W25Q64_OK when idle, W25Q64_ERR_TIMEOUT on timeout
   */
  w25q64_status_t w25q64_wait_ready(uint32_t timeout_ms);

  /**
   * @brief Get flash capacity in bytes.
   *
   * @return Flash size in bytes
   */
  uint32_t w25q64_get_capacity(void);

  /**
   * @brief Check if flash is present and responding.
   *
   * @return true if flash responds to ID command
   */
  bool w25q64_is_present(void);

#ifdef __cplusplus
}
#endif

#endif /* W25Q64_H */
