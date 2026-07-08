/**
 * @file boot_info.h
 * @brief Boot status passed from bootloader to application via SRAM.
 *
 * The bootloader writes a boot_status_t structure to a reserved SRAM
 * region (BOOT_STATUS_ADDR) before jumping to the application.  The
 * application reads this structure to determine which slot it booted
 * from, boot count, golden image validity, and diagnostic info.
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
  /* Boot-status magic + SRAM address                                    */
  /* ------------------------------------------------------------------ */

#define BOOT_STATUS_MAGIC 0xB007B007u /**< Bootloader wrote this        */
#define BOOT_STATUS_ADDR 0x2007FF00u  /**< Reserved at end of SRAM      */

  /* ------------------------------------------------------------------ */
  /* Boot-status flags                                                   */
  /* ------------------------------------------------------------------ */

#define BOOT_STATUS_FLAG_CRC_OK (1u << 0)    /**< Last CRC matched    */
#define BOOT_STATUS_FLAG_WDT_ARMED (1u << 1) /**< Watchdog armed      */
#define BOOT_STATUS_FLAG_FALLBACK (1u << 2)  /**< Fallback slot used  */
#define BOOT_STATUS_FLAG_GOLDEN (1u << 3)    /**< Golden restore done */

  /* ------------------------------------------------------------------ */
  /* Slot identifiers                                                    */
  /* ------------------------------------------------------------------ */

#define BOOT_SLOT_UNKNOWN 0u
#define BOOT_SLOT_A 1u
#define BOOT_SLOT_B 2u

  /* ------------------------------------------------------------------ */
  /* Boot status structure (24 bytes)                                    */
  /* ------------------------------------------------------------------ */

  typedef struct __attribute__((packed))
  {
    uint32_t magic;             /**< BOOT_STATUS_MAGIC for validity    */
    uint32_t boot_count;        /**< Total boot attempts               */
    uint8_t current_slot;       /**< BOOT_SLOT_A / BOOT_SLOT_B        */
    uint8_t boot_reason;        /**< POST_BOOT_* code                 */
    uint8_t golden_valid;       /**< 1 if golden image CRC32 is valid  */
    uint8_t flags;              /**< BOOT_STATUS_FLAG_* bits           */
    uint32_t last_crc_computed; /**< CRC32 computed by bootloader      */
    uint32_t last_crc_expected; /**< CRC32 from FMM slot metadata      */
    uint8_t reset_cause;        /**< RP2350 reset cause:                */
                                /**<  0=unknown, 1=POR, 2=WDT,         */
                                /**<  3=SW, 4=PIN, 5=brownout          */
    uint8_t slot_a_failures;    /**< Consecutive failures slot A       */
    uint8_t slot_b_failures;    /**< Consecutive failures slot B       */
    uint8_t _pad;               /**< Reserved                          */
  } boot_status_t;

  _Static_assert(sizeof(boot_status_t) == 24,
                 "boot_status_t must be 24 bytes for deterministic layout");

  /**
   * @brief Read boot status from the bootloader-reserved SRAM region.
   *
   * Checks magic before returning.  If magic is invalid, zeroes the
   * output and returns false.
   *
   * @param[out] status  Populated boot status (or zeroed on failure).
   * @return true if boot status was valid (written by bootloader).
   */
  bool boot_status_read(boot_status_t *status);

  /**
   * @brief Clear boot status (mark as invalid).
   *
   * Called by the application after reading to prevent stale status
   * from being read again on warm reset.
   */
  void boot_status_clear(void);

  /**
   * @brief Legacy wrapper — reads boot status into boot_info-compatible
   *        subset.  Deprecated, use boot_status_read() directly.
   */
  typedef boot_status_t boot_info_t;
#define BOOT_INFO_MAGIC BOOT_STATUS_MAGIC
#define BOOT_INFO_ADDR BOOT_STATUS_ADDR

  static inline bool boot_info_read(boot_info_t *info)
  {
    return boot_status_read(info);
  }

  static inline void boot_info_clear(void)
  {
    boot_status_clear();
  }

#ifdef __cplusplus
}
#endif

#endif /* BOOT_INFO_H */
