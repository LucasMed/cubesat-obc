/**
 * @file boot_info.h
 * @brief Boot information passed from bootloader to application.
 *
 * The bootloader writes a boot_info_t structure to a NOLOAD section in
 * SRAM before jumping to the application.  The application reads this
 * structure to determine which slot it booted from, boot count, and
 * golden image validity.
 *
 * Spec ref: bootloader/spec.md, firmware-update/spec.md
 */

#ifndef BOOT_INFO_H
#define BOOT_INFO_H

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  /* ------------------------------------------------------------------ */
  /* Boot-info magic + SRAM address                                      */
  /* ------------------------------------------------------------------ */

#define BOOT_INFO_MAGIC 0x494E464F /**< "INFO" in ASCII           */
#define BOOT_INFO_ADDR 0x2007FF00u /**< Reserved at end of SRAM   */

  /* ------------------------------------------------------------------ */
  /* Slot identifiers                                                    */
  /* ------------------------------------------------------------------ */

#define BOOT_SLOT_UNKNOWN 0u
#define BOOT_SLOT_A 1u
#define BOOT_SLOT_B 2u

  /* ------------------------------------------------------------------ */
  /* Boot info structure                                                 */
  /* ------------------------------------------------------------------ */

  typedef struct __attribute__((packed))
  {
    uint32_t magic;           /**< BOOT_INFO_MAGIC for validity check   */
    uint32_t boot_count;      /**< Total boot attempts (from POST)      */
    uint32_t slot_a_failures; /**< Consecutive boot failures on slot A  */
    uint32_t slot_b_failures; /**< Consecutive boot failures on slot B  */
    uint8_t current_slot;     /**< BOOT_SLOT_A / BOOT_SLOT_B           */
    uint8_t boot_reason;      /**< POST_BOOT_* code from bootloader    */
    uint8_t golden_valid;     /**< 1 if golden image CRC32 is valid     */
    uint8_t reserved[5];      /**< Pad to 24 bytes                     */
  } boot_info_t;

  _Static_assert(sizeof(boot_info_t) == 24,
                 "boot_info_t must be 24 bytes for deterministic layout");

  /**
   * @brief Read boot info from the bootloader-reserved SRAM region.
   *
   * Checks magic before returning.  If magic is invalid, zeroes the
   * output and returns false.
   *
   * @param[out] info  Populated boot info (or zeroed on failure).
   * @return true if boot info was valid.
   */
  bool boot_info_read(boot_info_t *info);

  /**
   * @brief Clear boot info (mark as invalid).
   *
   * Called by the application after reading to prevent stale info
   * from being read again on warm reset.
   */
  void boot_info_clear(void);

#ifdef __cplusplus
}
#endif

#endif /* BOOT_INFO_H */
