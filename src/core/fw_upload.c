/**
 * @file fw_upload.c
 * @brief Firmware upload state machine implementation.
 *
 * Stages firmware images to W25Q64 external flash, CRC-verifies, then
 * programs the inactive internal flash slot.
 *
 * W25Q64 staging layout (offset 0x200000, 1 MB):
 *   [0x000000 – 0x100000]  Sequential chunk storage
 *
 * Spec ref: firmware-update/spec.md, OBC-DES-001 §4.6
 */

#include "fw_upload.h"

#include "boot_info.h"
#include "csp/csp_crc32.h" /* CSP CRC32 implementation */
#include "internal_flash_layout.h"
#include "post.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef PICO_BUILD
  #include "hardware/flash.h"
  #include "pico/stdlib.h"
  #include "w25q64.h"
#endif

/* ------------------------------------------------------------------ */
/* Upload state (static, BSS-initialised)                              */
/* ------------------------------------------------------------------ */

static fw_upload_status_t s_upload;

/* ------------------------------------------------------------------ */
/* Helpers                                                             */
/* ------------------------------------------------------------------ */

#ifdef PICO_BUILD

/**
 * @brief Program internal flash from a buffer.
 *
 * Uses SDK flash_range_erase() + flash_range_program().  The SDK
 * functions are executed from SRAM (core 0 only).
 *
 * @param flash_offset  Internal flash offset relative to XIP_BASE.
 * @param data          Source data (must be 256-byte aligned).
 * @param count         Number of bytes to program (must be 256-byte aligned).
 * @return 0 on success, -1 on failure.
 */
static int program_internal_flash(uint32_t flash_offset, const uint8_t *data, uint32_t count)
{
  if (count == 0 || (count % FLASH_PAGE_SIZE) != 0)
  {
    return -1;
  }

  /* Erase the target region */
  flash_range_erase(flash_offset, count);

  /* Program in page-sized chunks */
  for (uint32_t off = 0; off < count; off += FLASH_PAGE_SIZE)
  {
    flash_range_program(flash_offset + off, data + off, FLASH_PAGE_SIZE);
  }

  return 0;
}

/**
 * @brief Read the entire staging buffer from W25Q64 into a heap buffer.
 *
 * @param[out] buf   Output buffer (must be at least total_size bytes).
 * @param total_size  Number of bytes to read.
 * @return 0 on success, -1 on W25Q64 error.
 */
static int read_staging(uint8_t *buf, uint32_t total_size)
{
  if (w25q64_read(FW_STAGING_OFFSET, buf, total_size) != W25Q64_OK)
  {
    return -1;
  }
  return 0;
}

#endif /* PICO_BUILD */

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */

int fw_upload_start(uint32_t total_size, uint32_t expected_crc, uint8_t target_slot)
{
  if (s_upload.state == FW_STATE_RECEIVING)
  {
    printf("[FW_UPLOAD] ERROR: upload already in progress\n");
    return -1;
  }

  if (total_size == 0 || total_size > FW_UPLOAD_MAX_IMAGE_SIZE)
  {
    printf("[FW_UPLOAD] ERROR: invalid total_size=%lu\n", (unsigned long)total_size);
    return -2;
  }

#ifdef PICO_BUILD
  /* Erase the W25Q64 staging region (1 MB) */
  /* Use 64 KB block erase for speed */
  for (uint32_t off = 0; off < FW_STAGING_SIZE; off += 0x10000u)
  {
    if (w25q64_erase_block64(FW_STAGING_OFFSET + off) != W25Q64_OK)
    {
      printf("[FW_UPLOAD] ERROR: staging erase failed at 0x%08lx\n",
             (unsigned long)(FW_STAGING_OFFSET + off));
      return -1;
    }
  }
#endif

  s_upload.state = FW_STATE_RECEIVING;
  s_upload.total_size = total_size;
  s_upload.expected_crc = expected_crc;
  s_upload.received = 0;
  s_upload.chunk_count = 0;
  s_upload.target_slot = target_slot;

  printf("[FW_UPLOAD] START: size=%lu crc=0x%08lX slot=%u\n", (unsigned long)total_size,
         (unsigned long)expected_crc, target_slot);
  return 0;
}

int fw_upload_write_chunk(uint32_t seq, const uint8_t *data, uint32_t len)
{
  if (s_upload.state != FW_STATE_RECEIVING)
  {
    printf("[FW_UPLOAD] ERROR: not in RECEIVING state (state=%d)\n", s_upload.state);
    return -1;
  }

  if (seq != s_upload.chunk_count)
  {
    printf("[FW_UPLOAD] ERROR: seq mismatch got=%lu expected=%lu\n", (unsigned long)seq,
           (unsigned long)s_upload.chunk_count);
    return -2;
  }

  if (len > FW_UPLOAD_CHUNK_SIZE)
  {
    printf("[FW_UPLOAD] ERROR: chunk too large (%lu > %u)\n", (unsigned long)len,
           FW_UPLOAD_CHUNK_SIZE);
    return -2;
  }

  (void)data; /* Used only inside PICO_BUILD (W25Q64 write) */

#ifdef PICO_BUILD
  /* Write to W25Q64 staging */
  if (w25q64_write_page(FW_STAGING_OFFSET + seq * FW_UPLOAD_CHUNK_SIZE, data, len) != W25Q64_OK)
  {
    printf("[FW_UPLOAD] ERROR: staging write failed at chunk %lu\n", (unsigned long)seq);
    s_upload.state = FW_STATE_FAILED;
    return -1;
  }
#endif

  s_upload.received += len;
  s_upload.chunk_count++;

  /* Check if upload is complete */
  if (s_upload.received >= s_upload.total_size)
  {
    s_upload.state = FW_STATE_COMPLETE;
    printf("[FW_UPLOAD] COMPLETE: %lu/%lu bytes, %lu chunks\n", (unsigned long)s_upload.received,
           (unsigned long)s_upload.total_size, (unsigned long)s_upload.chunk_count);
  }

  return 0;
}

int fw_upload_verify(void)
{
  if (s_upload.state != FW_STATE_COMPLETE)
  {
    printf("[FW_UPLOAD] ERROR: verify requires COMPLETE state (state=%d)\n", s_upload.state);
    return -2;
  }

#ifdef PICO_BUILD
  /* Allocate scratch buffer for CRC computation */
  /* For large images, compute CRC in chunks to minimise heap use */
  uint8_t *buf = (uint8_t *)malloc(FW_UPLOAD_CHUNK_SIZE);
  if (!buf)
  {
    printf("[FW_UPLOAD] ERROR: OOM for CRC buffer\n");
    return -1;
  }

  csp_crc32_t crc_ctx;
  csp_crc32_init(&crc_ctx);
  uint32_t remain = s_upload.total_size;
  uint32_t off = 0;

  while (remain > 0)
  {
    uint32_t chunk = (remain > FW_UPLOAD_CHUNK_SIZE) ? FW_UPLOAD_CHUNK_SIZE : remain;
    if (w25q64_read(FW_STAGING_OFFSET + off, buf, chunk) != W25Q64_OK)
    {
      printf("[FW_UPLOAD] ERROR: staging read failed at offset %lu\n", (unsigned long)off);
      free(buf);
      return -1;
    }
    csp_crc32_update(&crc_ctx, buf, chunk);
    off += chunk;
    remain -= chunk;
  }

  free(buf);

  uint32_t crc = csp_crc32_final(&crc_ctx);

  if (crc != s_upload.expected_crc)
  {
    printf("[FW_UPLOAD] CRC MISMATCH: computed=0x%08lX expected=0x%08lX\n", (unsigned long)crc,
           (unsigned long)s_upload.expected_crc);
    s_upload.state = FW_STATE_FAILED;
    return -1;
  }

  printf("[FW_UPLOAD] CRC MATCH: 0x%08lX\n", (unsigned long)crc);
  s_upload.state = FW_STATE_VERIFIED;
  return 0;
#else
  printf("[FW_UPLOAD] Host: CRC verification skipped (no W25Q64)\n");
  s_upload.state = FW_STATE_VERIFIED;
  return 0;
#endif
}

int fw_upload_commit(void)
{
  if (s_upload.state != FW_STATE_VERIFIED)
  {
    printf("[FW_UPLOAD] ERROR: commit requires VERIFIED state (state=%d)\n", s_upload.state);
    return -2;
  }

#ifdef PICO_BUILD
  s_upload.state = FW_STATE_COMMITTING;

  /* Determine the target internal flash slot */
  uint32_t slot_base;
  if (s_upload.target_slot == BOOT_SLOT_A)
  {
    slot_base = SLOT_A_BASE;
  }
  else if (s_upload.target_slot == BOOT_SLOT_B)
  {
    slot_base = SLOT_B_BASE;
  }
  else
  {
    printf("[FW_UPLOAD] ERROR: invalid target slot %u\n", s_upload.target_slot);
    s_upload.state = FW_STATE_FAILED;
    return -1;
  }

  /* Allocate SRAM buffer for the image */
  uint32_t image_size = s_upload.total_size;
  /* Round up to FLASH_PAGE_SIZE (256 B) alignment */
  uint32_t aligned_size = (image_size + FLASH_PAGE_SIZE - 1) & ~(FLASH_PAGE_SIZE - 1);

  uint8_t *buf = (uint8_t *)malloc(aligned_size);
  if (!buf)
  {
    printf("[FW_UPLOAD] ERROR: OOM for commit buffer (%lu bytes)\n", (unsigned long)aligned_size);
    s_upload.state = FW_STATE_FAILED;
    return -1;
  }

  /* Read staging from W25Q64 */
  if (read_staging(buf, image_size) != 0)
  {
    printf("[FW_UPLOAD] ERROR: staging read failed\n");
    free(buf);
    s_upload.state = FW_STATE_FAILED;
    return -1;
  }

  /* Zero-pad to aligned size */
  if (aligned_size > image_size)
  {
    memset(buf + image_size, 0xFF, aligned_size - image_size);
  }

  printf("[FW_UPLOAD] COMMIT: programming slot at 0x%08lX (%lu bytes)\n", (unsigned long)slot_base,
         (unsigned long)aligned_size);

  /* Program internal flash (blocking, interrupts disabled inside SDK) */
  uint32_t flash_offset = slot_base - INTERNAL_FLASH_BASE;
  int ret = program_internal_flash(flash_offset, buf, aligned_size);

  if (ret != 0)
  {
    printf("[FW_UPLOAD] ERROR: flash programming failed\n");
    free(buf);
    s_upload.state = FW_STATE_FAILED;
    return -1;
  }

  /* Update slot metadata with new CRC and size */
  {
    csp_crc32_t crc_ctx;
    csp_crc32_init(&crc_ctx);
    csp_crc32_update(&crc_ctx, buf, image_size);
    uint32_t crc = csp_crc32_final(&crc_ctx);

    slot_metadata_t sm;
    memset(&sm, 0, sizeof(sm));
    sm.crc32 = crc;
    sm.image_size = image_size;
    sm.version = 1;
    sm.slot_status = 1;
    sm.failure_count = 0;

    uint32_t meta_addr = (s_upload.target_slot == BOOT_SLOT_A)   ? SLOT_A_METADATA_ADDR
                         : (s_upload.target_slot == BOOT_SLOT_B) ? SLOT_B_METADATA_ADDR
                                                                 : 0;

    if (meta_addr != 0)
    {
      uint8_t sector_buf[FLASH_SECTOR_SIZE] __attribute__((aligned(4)));
      memcpy(sector_buf, (const void *)FMM_METADATA_BASE, FLASH_SECTOR_SIZE);
      memcpy(sector_buf + (meta_addr - FMM_METADATA_BASE), &sm, sizeof(sm));

      uint32_t flash_off = FMM_METADATA_BASE - INTERNAL_FLASH_BASE;
      flash_range_erase(flash_off, FLASH_SECTOR_SIZE);
      for (uint32_t off = 0; off < FLASH_SECTOR_SIZE; off += FLASH_PAGE_SIZE)
      {
        flash_range_program(flash_off + off, sector_buf + off, FLASH_PAGE_SIZE);
      }
    }

    printf("[FW_UPLOAD] COMMIT: slot %u programmed, CRC=0x%08lX, size=%lu\n", s_upload.target_slot,
           (unsigned long)crc, (unsigned long)image_size);
  }

  free(buf);
#endif

  s_upload.state = FW_STATE_IDLE;
  return 0;
}

void fw_upload_abort(void)
{
  printf("[FW_UPLOAD] ABORT: %lu bytes received\n", (unsigned long)s_upload.received);
  memset(&s_upload, 0, sizeof(s_upload));
  /* W25Q64 staging is preserved for diagnostics */
}

void fw_upload_get_status(fw_upload_status_t *status)
{
  if (status)
  {
    memcpy(status, &s_upload, sizeof(s_upload));
  }
}

bool fw_upload_is_busy(void)
{
  return s_upload.state == FW_STATE_RECEIVING;
}
