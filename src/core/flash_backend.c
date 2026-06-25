/**
 * @file flash_backend.c
 * @brief Pico (RP2350) flash backend for persistent event log storage.
 *
 * Implements flash_backend_flush() for PICO_BUILD targets using the Pico SDK
 * hardware_flash API.  The host build is handled separately by
 * flash_backend_stub.c which appends to /tmp/obc_log.bin.
 *
 * Flash layout
 * ────────────
 * The OBC firmware image occupies the lower portion of the 2 MB flash.
 * The persistent log region is placed at the **top** of flash to avoid
 * any overlap with the firmware image regardless of build size:
 *
 *   0x000000  ┌─────────────────────────────┐
 *             │  Firmware (.text + .rodata) │  up to ~500 KB
 *             │  Runtime data (.data/.bss)  │  in SRAM at runtime
 *   0x1FC000  ├─────────────────────────────┤  ← FLASH_LOG_OFFSET
 *             │  Log sector 0  (4 KB)       │  active write region
 *             │  Log sector 1  (4 KB)       │  (rolling)
 *             │  Log sector 2  (4 KB)
 *             │  Log sector 3  (4 KB)
 *   0x200000  └─────────────────────────────┘  ← end of 2 MB device
 *
 * Each call to flash_backend_flush() writes to the next available log
 * sector (round-robin).  Before writing, the target sector is erased.
 * Interrupts are disabled for the duration of the erase + program
 * sequence as required by the Pico SDK (XIP cache flushed internally).
 *
 * Record header (written at start of each sector):
 *   [0..7]  magic  0x4F42_434C_4F47_5631  ("OBCLOGV1")
 *   [8..11] crc32  CRC-32/ISO-HDLC of the data payload
 *   [12..15] len   payload length in bytes (uint32_t, little-endian)
 *
 * Recovery on boot:
 *   On power-up, the health_monitor_task (or a dedicated boot hook) may
 *   call flash_backend_read_sector() to scan sectors for valid headers
 *   and replay Class-A records into the live ring buffer.
 *
 * Safety notes:
 *   - Must be called from task context only; NOT ISR-safe.
 *   - Interrupts are disabled for ≤ ~9 ms (1 × 4 KB erase + ≤ 2.56 KB
 *     program at Pico SDK nominal flash timings).
 *   - FreeRTOS tick will be missed during this window; acceptable for
 *     infrequent Class-A flush events.
 *
 * Spec ref: SYS-F-304 (SyRS-OBC-001); SRR-OBC-001 ACT-16
 */

#ifdef PICO_BUILD

  #include "flash_backend.h"

  #include "hardware/flash.h"
  #include "hardware/sync.h"
  #include "logger.h"

  #include <string.h>

  /* ------------------------------------------------------------------ */
  /* Flash log region layout                                             */
  /* ------------------------------------------------------------------ */

  /**
   * Total XIP flash size on Pico 2W.
   * Board header defines PICO_FLASH_SIZE_BYTES = 4 MB (W25Q32).
   * Use SDK define when available, fallback to known constant for host builds.
   */
  #ifdef PICO_FLASH_SIZE_BYTES
    #define FLASH_TOTAL_BYTES ((uint32_t)PICO_FLASH_SIZE_BYTES)
  #else
    #define FLASH_TOTAL_BYTES (4u * 1024u * 1024u)
  #endif

  /** Number of flash sectors reserved for the persistent log. */
  #define FLASH_LOG_SECTORS 4u

  /** Size of the log region in bytes. */
  #define FLASH_LOG_SIZE (FLASH_LOG_SECTORS * FLASH_SECTOR_SIZE)

  /**
   * Offset of the log region from the start of flash (not XIP base).
   * Placed near the top of flash. FLASH_TOTAL_BYTES covers the whole XIP region.
   */
  #define FLASH_LOG_OFFSET (FLASH_TOTAL_BYTES - FLASH_LOG_SIZE)

  /** Magic number written at the start of every valid log sector. */
  #define LOG_SECTOR_MAGIC UINT64_C(0x4F42434C4F475631) /* "OBCLOGV1" */

  /** Size of the per-sector header (magic + crc32 + payload_len). */
  #define LOG_HEADER_SIZE 16u

  /** Maximum payload that fits in one sector after the header. */
  #define LOG_PAYLOAD_MAX (FLASH_SECTOR_SIZE - LOG_HEADER_SIZE)

/* ------------------------------------------------------------------ */
/* CRC-32/ISO-HDLC (polynomial 0xEDB88320, reflected)                 */
/* ------------------------------------------------------------------ */

static uint32_t crc32_compute(const uint8_t *data, size_t len)
{
  uint32_t crc = 0xFFFFFFFFu;
  for (size_t i = 0u; i < len; i++)
  {
    crc ^= (uint32_t)data[i];
    for (unsigned bit = 0u; bit < 8u; bit++)
    {
      if ((crc & 1u) != 0u)
      {
        crc = (crc >> 1u) ^ 0xEDB88320u;
      }
      else
      {
        crc >>= 1u;
      }
    }
  }
  return crc ^ 0xFFFFFFFFu;
}

/* ------------------------------------------------------------------ */
/* Module state                                                        */
/* ------------------------------------------------------------------ */

/** Index of the next sector to write (0 .. FLASH_LOG_SECTORS-1). */
static uint32_t s_next_sector = 0u;

/* ------------------------------------------------------------------ */
/* Internal helpers                                                    */
/* ------------------------------------------------------------------ */

/**
 * Build a 256-byte-aligned, FLASH_PAGE_SIZE-padded page buffer
 * containing the log sector header and `len` bytes of payload,
 * then erase + program the target sector.
 *
 * Must be called with interrupts already disabled.
 */
static void write_log_sector(uint32_t sector_idx, const uint8_t *payload, uint32_t payload_len)
{
  /* Sector erase/program buffer — must be a multiple of FLASH_PAGE_SIZE. */
  /* Header (16) + payload (≤ FLASH_SECTOR_SIZE-16) padded to 4096.       */
  static uint8_t s_prog_buf[FLASH_SECTOR_SIZE];

  uint32_t flash_offs = FLASH_LOG_OFFSET + (sector_idx * FLASH_SECTOR_SIZE);

  /* --- Build header --------------------------------------------------- */
  (void)memset(s_prog_buf, 0xFF, sizeof(s_prog_buf)); /* flash erased state */

  /* magic (8 bytes, little-endian) */
  uint64_t magic = LOG_SECTOR_MAGIC;
  (void)memcpy(&s_prog_buf[0], &magic, sizeof(magic));

  /* crc32 of payload (4 bytes) */
  uint32_t crc = crc32_compute(payload, (size_t)payload_len);
  (void)memcpy(&s_prog_buf[8], &crc, sizeof(crc));

  /* payload length (4 bytes) */
  (void)memcpy(&s_prog_buf[12], &payload_len, sizeof(payload_len));

  /* payload */
  (void)memcpy(&s_prog_buf[LOG_HEADER_SIZE], payload, (size_t)payload_len);

  /* --- Erase sector then program --------------------------------------- */
  flash_range_erase(flash_offs, FLASH_SECTOR_SIZE);
  flash_range_program(flash_offs, s_prog_buf, FLASH_SECTOR_SIZE);
}

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */

void flash_backend_flush(const uint8_t *buf, size_t len)
{
  if ((buf == NULL) || (len == 0u))
  {
    return;
  }

  /* Clamp to the maximum payload that fits in one sector. */
  uint32_t write_len = (len > LOG_PAYLOAD_MAX) ? LOG_PAYLOAD_MAX : (uint32_t)len;

  /*
   * Disable interrupts for the duration of erase + program.
   * flash_range_erase / flash_range_program copy the second-stage
   * bootloader to SRAM and execute from there, so the XIP bus is
   * unavailable during programming.  Any interrupt handler that
   * executes flash-resident code would hard-fault.
   *
   * The FreeRTOS tick ISR is held off.  At 9 ms worst-case this
   * causes at most one missed tick (configTICK_RATE_HZ = 1000).
   */
  uint32_t irq_state = save_and_disable_interrupts();

  write_log_sector(s_next_sector, buf, write_len);

  restore_interrupts(irq_state);

  /* Advance sector index (round-robin). */
  s_next_sector = (s_next_sector + 1u) % FLASH_LOG_SECTORS;
}

/**
 * @brief Scan the log region and invoke @p cb for each valid sector.
 *
 * Called during boot (e.g., from system_init or health_monitor_task) to
 * recover Class-A events that survived a power cycle.
 *
 * For each sector whose header passes validation (magic match + CRC32),
 * @p cb is invoked with a pointer to the raw payload bytes and the
 * payload length.  The callback may call log_event() to replay events
 * into the live ring buffer.
 *
 * @param cb  Recovery callback.  Must not be NULL.
 */
void flash_backend_recover(void (*cb)(const uint8_t *payload, uint32_t len))
{
  if (cb == NULL)
  {
    return;
  }

  for (uint32_t s = 0u; s < FLASH_LOG_SECTORS; s++)
  {
    /* Read sector from XIP window (read-only, no erase/program). */
    const uint8_t *sector_ptr =
        (const uint8_t *)(XIP_BASE + FLASH_LOG_OFFSET + s * FLASH_SECTOR_SIZE);

    /* Validate magic. */
    uint64_t magic_read = 0u;
    (void)memcpy(&magic_read, &sector_ptr[0], sizeof(magic_read));
    if (magic_read != LOG_SECTOR_MAGIC)
    {
      continue;
    }

    /* Read stored CRC and length. */
    uint32_t stored_crc = 0u;
    uint32_t stored_len = 0u;
    (void)memcpy(&stored_crc, &sector_ptr[8], sizeof(stored_crc));
    (void)memcpy(&stored_len, &sector_ptr[12], sizeof(stored_len));

    if ((stored_len == 0u) || (stored_len > LOG_PAYLOAD_MAX))
    {
      continue;
    }

    /* Verify CRC. */
    const uint8_t *payload_ptr = &sector_ptr[LOG_HEADER_SIZE];
    uint32_t computed_crc = crc32_compute(payload_ptr, (size_t)stored_len);
    if (computed_crc != stored_crc)
    {
      continue;
    }

    /* Valid sector — invoke recovery callback. */
    cb(payload_ptr, stored_len);
  }
}

#endif /* PICO_BUILD */

/* Suppress ISO C "empty translation unit" warning on host builds. */
typedef int flash_backend_c_not_empty_;
