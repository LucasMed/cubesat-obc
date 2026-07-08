/**
 * @file internal_flash_layout.h
 * @brief RP2350 Internal XIP Flash Memory Map.
 *
 * Single source of truth for the dual-slot boot layout.
 *
 * The RP2350 on Pico 2W has 4 MB of internal XIP flash (W25Q16JV).
 * Layout:
 *   0x10000000 – 0x1000FFFF: Bootloader  (64 KB)
 *   0x10010000 – 0x1010FFFF: Slot A      (1 MB)
 *   0x10110000 – 0x1020FFFF: Slot B      (1 MB)
 *   0x10210000 – 0x10210FFF: Golden meta (4 KB)
 *   0x10211000 – 0x1030FFFF: Golden img  (956 KB)
 *   0x10310000 – 0x10310FFF: FMM meta    (4 KB)
 *   0x10311000 – 0x10311FFF: Boot log    (4 KB)
 *   0x10312000 – 0x103FFFFF: Reserved     (952 KB)
 *
 * Spec ref: Golden Image + Dual Boot SDD, bootloader/spec.md
 */

#ifndef INTERNAL_FLASH_LAYOUT_H
#define INTERNAL_FLASH_LAYOUT_H

#include <assert.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  /* ------------------------------------------------------------------ */
  /* Flash geometry                                                      */
  /* ------------------------------------------------------------------ */

#define INTERNAL_FLASH_BASE 0x10000000u
#define INTERNAL_FLASH_SIZE (4u * 1024u * 1024u) /* 4 MB */

/* ------------------------------------------------------------------ */
/* Region base addresses and sizes                                     */
/* ------------------------------------------------------------------ */

/* Bootloader — CRC32 validation, slot select, golden restore          */
#define BOOTLOADER_BASE 0x10000000u
#define BOOTLOADER_SIZE 0x00010000u /* 64 KB */

/* Slot A — primary firmware image                                     */
#define SLOT_A_BASE 0x10010000u
#define SLOT_A_SIZE 0x00100000u /* 1 MB */

/* Slot B — fallback firmware image                                    */
#define SLOT_B_BASE 0x10110000u
#define SLOT_B_SIZE 0x00100000u /* 1 MB */

/* Golden metadata — CRC, version, size of golden image                */
#define GOLDEN_METADATA_BASE 0x10210000u
#define GOLDEN_METADATA_SIZE 0x00001000u /* 4 KB */

/* Golden image — pristine firmware copy for restore
 * Size constrained to fit between golden metadata and FMM metadata.   */
#define GOLDEN_IMAGE_BASE 0x10211000u
#define GOLDEN_IMAGE_SIZE 0x000EF000u /* 956 KB */

/* FMM metadata — boot record ring (4 x 1 KB), failure counters        */
#define FMM_METADATA_BASE 0x10310000u
#define FMM_METADATA_SIZE 0x00001000u /* 4 KB */

/* Slot metadata locations within the FMM sector (at known offsets)    */
/* boot_meta_t occupies the first ~64 bytes; slot metadata goes after  */
#define SLOT_A_METADATA_ADDR (FMM_METADATA_BASE + 64u)
#define SLOT_B_METADATA_ADDR (FMM_METADATA_BASE + 128u)

/* RP2350 SRAM bounds (for ARM vector table sanity check)              */
/* RP2350A (Pico 2W) has 520 KB SRAM: 0x20000000 - 0x20081FFF          */
/* Stack pointer is set to top of SRAM (first byte past valid RAM)     */
#define RP2350_SRAM_BASE 0x20000000u
#define RP2350_SRAM_SIZE 0x00082000u /* 520 KB — RP2350A full SRAM */

/* Boot log ring buffer — 128 entries × 32 bytes, one erase block      */
#define BOOT_LOG_BASE 0x10311000u
#define BOOT_LOG_SIZE 0x00001000u /* 4 KB / one sector */

/* Reserved for future use                                             */
#define INTERNAL_FLASH_RESERVED (BOOT_LOG_BASE + BOOT_LOG_SIZE)
#define INTERNAL_FLASH_RESERVED_SZ 0x000EE000u /* 952 KB */

  /* ------------------------------------------------------------------ */
  /* Image slot metadata structure (per slot)                             */
  /* ------------------------------------------------------------------ */

  typedef struct __attribute__((packed))
  {
    uint32_t crc32;        /**< CRC32 of the entire slot content    */
    uint32_t image_size;   /**< Binary size in bytes                */
    uint32_t version;      /**< Build version / timestamp           */
    uint8_t slot_status;   /**< 0=empty, 1=valid, 2=pending, 3=failed */
    uint8_t failure_count; /**< Consecutive boot failures           */
    uint8_t reserved[10];  /**< Pad to 24 bytes                     */
  } slot_metadata_t;

  _Static_assert(sizeof(slot_metadata_t) == 24,
                 "slot_metadata_t must be 24 bytes for deterministic metadata sector layout");

  /* ------------------------------------------------------------------ */
  /* Compile-time overlap assertions                                     */
  /* ------------------------------------------------------------------ */

  _Static_assert(INTERNAL_FLASH_BASE + INTERNAL_FLASH_SIZE == 0x10400000u,
                 "internal_flash_layout: total flash size does not match expected end");

  _Static_assert(BOOTLOADER_BASE == INTERNAL_FLASH_BASE,
                 "internal_flash_layout: bootloader must be at flash base");

  _Static_assert(BOOTLOADER_BASE + BOOTLOADER_SIZE == SLOT_A_BASE,
                 "internal_flash_layout: bootloader region must end where slot A begins");

  _Static_assert(SLOT_A_BASE + SLOT_A_SIZE == SLOT_B_BASE,
                 "internal_flash_layout: slot A must end where slot B begins");

  _Static_assert(SLOT_B_BASE + SLOT_B_SIZE <= GOLDEN_METADATA_BASE,
                 "internal_flash_layout: slot B must not overlap golden metadata");

  _Static_assert(GOLDEN_METADATA_BASE + GOLDEN_METADATA_SIZE == GOLDEN_IMAGE_BASE,
                 "internal_flash_layout: golden metadata must directly precede golden image");

  _Static_assert(GOLDEN_IMAGE_BASE + GOLDEN_IMAGE_SIZE <= FMM_METADATA_BASE,
                 "internal_flash_layout: golden image must not overlap FMM metadata");

  _Static_assert(FMM_METADATA_BASE + FMM_METADATA_SIZE == BOOT_LOG_BASE,
                 "internal_flash_layout: FMM metadata must directly precede boot log");
  _Static_assert(BOOT_LOG_BASE + BOOT_LOG_SIZE == INTERNAL_FLASH_RESERVED,
                 "internal_flash_layout: boot log must directly precede reserved region");

  _Static_assert(INTERNAL_FLASH_RESERVED + INTERNAL_FLASH_RESERVED_SZ <=
                     INTERNAL_FLASH_BASE + INTERNAL_FLASH_SIZE,
                 "internal_flash_layout: reserved region exceeds flash size");

  /* Verify each region fits within the flash                             */
  _Static_assert(BOOTLOADER_SIZE == 0x00010000u, "internal_flash_layout: bad BOOTLOADER_SIZE");
  _Static_assert(SLOT_A_SIZE == 0x00100000u, "internal_flash_layout: bad SLOT_A_SIZE");
  _Static_assert(SLOT_B_SIZE == 0x00100000u, "internal_flash_layout: bad SLOT_B_SIZE");
  _Static_assert(GOLDEN_METADATA_SIZE == 0x00001000u,
                 "internal_flash_layout: bad GOLDEN_METADATA_SIZE");
  _Static_assert(GOLDEN_IMAGE_SIZE == 0x000EF000u, "internal_flash_layout: bad GOLDEN_IMAGE_SIZE");
  _Static_assert(FMM_METADATA_SIZE == 0x00001000u, "internal_flash_layout: bad FMM_METADATA_SIZE");
  _Static_assert(BOOT_LOG_SIZE == 0x00001000u, "internal_flash_layout: bad BOOT_LOG_SIZE");

#ifdef __cplusplus
}
#endif

#endif /* INTERNAL_FLASH_LAYOUT_H */
