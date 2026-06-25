/**
 * @file boot_info.c
 * @brief Boot information access from bootloader-reserved SRAM.
 *
 * The bootloader writes boot_info_t to a dedicated NOLOAD section in
 * SRAM (top of main RAM) before jumping to the application.  This
 * module provides read and clear access.
 *
 * Spec ref: bootloader/spec.md, firmware-update/spec.md
 */

#include "boot_info.h"

#include <string.h>

/* ------------------------------------------------------------------ */
/* Boot info pointer (reserved SRAM at BOOT_INFO_ADDR)                 */
/* ------------------------------------------------------------------ */

static volatile boot_info_t *const s_boot_info = (volatile boot_info_t *const)BOOT_INFO_ADDR;

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */

bool boot_info_read(boot_info_t *info)
{
  if (!info)
  {
    return false;
  }

  /* Read with volatile semantics (may be written by bootloader) */
  boot_info_t local;
  local.magic = s_boot_info->magic;
  local.boot_count = s_boot_info->boot_count;
  local.slot_a_failures = s_boot_info->slot_a_failures;
  local.slot_b_failures = s_boot_info->slot_b_failures;
  local.current_slot = s_boot_info->current_slot;
  local.boot_reason = s_boot_info->boot_reason;
  local.golden_valid = s_boot_info->golden_valid;

  if (local.magic != BOOT_INFO_MAGIC)
  {
    memset(info, 0, sizeof(*info));
    return false;
  }

  memcpy(info, (const void *)&local, sizeof(local));
  return true;
}

void boot_info_clear(void)
{
  s_boot_info->magic = 0;
}
