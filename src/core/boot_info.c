/**
 * @file boot_info.c
 * @brief Boot status access from bootloader-reserved SRAM.
 *
 * The bootloader writes boot_status_t to a dedicated region at
 * BOOT_STATUS_ADDR (top of main SRAM) before jumping to the
 * application.  This module provides read and clear access.
 *
 * Spec ref: bootloader/spec.md, firmware-update/spec.md
 */

#include "boot_info.h"

#include <string.h>

/* ------------------------------------------------------------------ */
/* Boot status pointer (reserved SRAM at BOOT_STATUS_ADDR)              */
/* ------------------------------------------------------------------ */

static volatile boot_status_t *const s_boot_status =
    (volatile boot_status_t *const)BOOT_STATUS_ADDR;

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */

bool boot_status_read(boot_status_t *status)
{
  if (!status)
  {
    return false;
  }

  /* Read with volatile semantics (written by bootloader) */
  boot_status_t local;
  local.magic             = s_boot_status->magic;
  local.boot_count        = s_boot_status->boot_count;
  local.current_slot      = s_boot_status->current_slot;
  local.boot_reason       = s_boot_status->boot_reason;
  local.golden_valid      = s_boot_status->golden_valid;
  local.flags             = s_boot_status->flags;
  local.last_crc_computed = s_boot_status->last_crc_computed;
  local.last_crc_expected = s_boot_status->last_crc_expected;
  local.reset_cause       = s_boot_status->reset_cause;
  local.slot_a_failures   = s_boot_status->slot_a_failures;
  local.slot_b_failures   = s_boot_status->slot_b_failures;

  if (local.magic != BOOT_STATUS_MAGIC)
  {
    memset(status, 0, sizeof(*status));
    return false;
  }

  memcpy(status, (const void *)&local, sizeof(local));
  return true;
}

void boot_status_clear(void)
{
  s_boot_status->magic = 0;
}
