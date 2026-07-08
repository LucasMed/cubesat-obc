/**
 * @file boot_meta.c
 * @brief Persistent boot metadata write access from FSW.
 *
 * The bootloader owns boot_meta_t in flash.  The FSW only writes the
 * fsw_confirmed flag after a successful boot — the bootloader reads
 * it on the next reset and resets failure counters.
 *
 * Spec ref: PENDING_TASKS.md §9.2, boot_meta.h
 */

#include "boot_meta.h"

#include "crc32.h"

#include <string.h>

#ifdef PICO_BUILD
  #include "hardware/flash.h"
  #include "hardware/sync.h"
#endif

/* ------------------------------------------------------------------ */
/* fsw_confirmed write                                                  */
/* ------------------------------------------------------------------ */

void boot_meta_set_fsw_confirmed(void)
{
#ifdef PICO_BUILD
  /* Read current metadata from flash */
  boot_meta_t meta;
  memcpy(&meta, (const void *)BOOT_META_BASE, sizeof(meta));

  /* Already confirmed?  Skip the expensive flash write (~55 ms). */
  if (meta.fsw_confirmed == 1)
  {
    return;
  }

  /* Update the flag */
  meta.fsw_confirmed = 1;

  /* Recompute CRC (crc32 field must be 0 before compute) */
  meta.crc32 = 0;
  meta.crc32 = crc32_compute(&meta, sizeof(meta) - 4);

  /* Write to flash — Pico SDK handles XIP stall + SRAM copy */
  uint32_t flash_off = BOOT_META_BASE - 0x10000000u;
  uint32_t ints = save_and_disable_interrupts();
  flash_range_erase(flash_off, FLASH_SECTOR_SIZE);
  flash_range_program(flash_off, (const uint8_t *)&meta, FLASH_PAGE_SIZE);
  restore_interrupts(ints);
#else
  /* Host build: no-op */
  (void)0;
#endif
}
