/**
 * @file boot_meta.h
 * @brief Persistent boot metadata in internal flash.
 *
 * The bootloader reads and writes boot_meta_t at BOOT_META_BASE
 * (0x10221000, a dedicated 4 KB sector in the golden-image reserved area).
 * The FSW writes fsw_confirmed = 1 after a successful boot so the
 * bootloader can reset failure counters on the next boot.
 *
 * Spec ref: PENDING_TASKS.md §9.2 (fsw_confirmed)
 */

#ifndef BOOT_META_H
#define BOOT_META_H

#include <assert.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  /* ------------------------------------------------------------------ */
  /* Constants                                                           */
  /* ------------------------------------------------------------------ */

#define BOOT_META_BASE 0x10221000u  /**< Dedicated sector in flash      */
#define BOOT_META_MAGIC 0x424F4F54u /**< "BOOT"                         */

  /** CRC result codes (persisted for diagnostics) */
#define CRC_RESULT_NONE 0u
#define CRC_RESULT_PASS 1u
#define CRC_RESULT_FAIL 2u
#define CRC_RESULT_NO_META 3u

  /* ------------------------------------------------------------------ */
  /* Boot metadata structure (24 bytes)                                  */
  /* ------------------------------------------------------------------ */

  typedef struct __attribute__((packed))
  {
    uint32_t magic;          /**< BOOT_META_MAGIC                    */
    uint8_t current_slot;    /**< 0=Slot A, 1=Slot B                */
    uint8_t slot_a_failures; /**< Consecutive boot failures Slot A  */
    uint8_t slot_b_failures; /**< Consecutive boot failures Slot B  */
    uint8_t boot_reason;     /**< POST code reason                  */
    uint32_t timestamp;      /**< Boot timestamp (0 if not avail)   */
    uint32_t last_jump_addr; /**< Last successful jump address      */
    uint8_t last_crc_result; /**< CRC_RESULT_*                      */
    uint8_t fsw_confirmed;   /**< 1 if FSW confirmed successful boot*/
    uint8_t reset_cause;     /**< RP2350 reset cause (RAW reason)   */
    uint8_t _pad[1];         /**< Reserved                          */
    uint32_t crc32;          /**< CRC32 over magic.._pad fields     */
  } boot_meta_t;

  _Static_assert(sizeof(boot_meta_t) == 24, "boot_meta_t must be 24 bytes");

  /* ------------------------------------------------------------------ */
  /* FSW-side write: set fsw_confirmed = 1 after a successful boot.      */
  /* Pico build only — host build provides a no-op stub.                 */
  /* ------------------------------------------------------------------ */

  /**
   * @brief Write fsw_confirmed = 1 to the boot metadata in flash.
   *
   * Must be called AFTER POST + all task creation succeed.  The flag
   * persists across resets and tells the bootloader the last boot was
   * successful, so it should reset failure counters on the next boot.
   *
   * Safe to call multiple times (idempotent after first write).
   * On host (non-Pico) builds this is a no-op.
   */
  void boot_meta_set_fsw_confirmed(void);

#ifdef __cplusplus
}
#endif

#endif /* BOOT_META_H */
