/**
 * @file boot_log.c
 * @brief Boot log ring buffer in internal flash.
 *
 * Stores 128 × 32-byte entries in a dedicated 4 KB flash sector
 * (BOOT_LOG_BASE). Written by the bootloader on each boot attempt,
 * readable by the FSW for telemetry / post-mortem.
 *
 * The bootloader calls boot_log_write() before jumping to the selected
 * image. The FSW reads entries via boot_log_read_entry().
 *
 * Spec ref: PENDING_TASKS.md §9.5
 */

#include "boot_log.h"

#include <string.h>

#include "crc32.h"
#if PICO_BUILD
#include "hardware/flash.h"
#include "hardware/sync.h"
#endif

/* ------------------------------------------------------------------ */
/* Helpers                                                               */
/* ------------------------------------------------------------------ */

/** Offset of crc_entry within boot_log_entry_t (bytes [0..27]) */
#define CRC_FIELD_OFFSET offsetof(boot_log_entry_t, crc_entry)

/**
 * @brief Compute the CRC of an entry (all fields before crc_entry).
 */
static uint32_t entry_crc(const boot_log_entry_t *entry)
{
    return crc32_compute(entry, CRC_FIELD_OFFSET);
}

/**
 * @brief Check if an entry is "empty" (all bytes == 0xFF, i.e. erased).
 */
static bool entry_is_empty(const boot_log_entry_t *entry)
{
    const uint8_t *p    = (const uint8_t *)entry;
    const uint8_t *end  = p + sizeof(boot_log_entry_t);
    for (; p < end; p++)
    {
        if (*p != 0xFF)
            return false;
    }
    return true;
}

/** Flash offset from XIP_BASE for the boot log region */
#define BOOT_LOG_FLASH_OFF (BOOT_LOG_BASE - 0x10000000u)

/**
 * @brief Get the flash offset (from XIP_BASE) of entry at given index.
 */
static inline uint32_t entry_flash_offset(uint32_t index)
{
    return BOOT_LOG_FLASH_OFF + index * BOOT_LOG_ENTRY_SIZE;
}

/**
 * @brief Get the flash address (memory-mapped XIP) of entry at index.
 */
static inline const boot_log_entry_t *entry_at_index(uint32_t index)
{
    return (const boot_log_entry_t *)(BOOT_LOG_BASE + index * BOOT_LOG_ENTRY_SIZE);
}

/* ------------------------------------------------------------------ */
/* API implementation                                                    */
/* ------------------------------------------------------------------ */

bool boot_log_write(const boot_log_entry_t *entry_)
{
    boot_log_entry_t local_entry = *entry_;

    /* Compute CRC before write */
    local_entry.crc_entry = entry_crc(&local_entry);

    /* Find write position */
    uint32_t idx = boot_log_find_write_index();

    /* If full, erase and restart */
    if (idx >= BOOT_LOG_MAX_ENTRIES)
    {
        if (!boot_log_clear())
            return false;
        idx = 0;
    }

#if PICO_BUILD
    /* Prepare a full 256-byte flash page buffer.
     * flash_range_program requires multiples of FLASH_PAGE_SIZE. */
    uint8_t page_buf[FLASH_PAGE_SIZE] __attribute__((aligned(4)));
    uint32_t     page_aligned = entry_flash_offset(idx) & ~(FLASH_PAGE_SIZE - 1u);
    uint32_t     entry_offset = entry_flash_offset(idx) - page_aligned;

    /* Read current page content from XIP flash */
    const uint8_t *xip_page = (const uint8_t *)(BOOT_LOG_BASE + (page_aligned - BOOT_LOG_FLASH_OFF));
    memcpy(page_buf, xip_page, FLASH_PAGE_SIZE);

    /* Overwrite entry bytes in the page buffer */
    memcpy(page_buf + entry_offset, &local_entry, sizeof(local_entry));

    /* Program the page */
    uint32_t ints = save_and_disable_interrupts();
    flash_range_program(page_aligned, page_buf, FLASH_PAGE_SIZE);
    restore_interrupts(ints);

    return true;
#else
    (void)idx;
    (void)local_entry;
    return false;
#endif
}

bool boot_log_read_entry(uint32_t index, boot_log_entry_t *entry)
{
    if (index >= BOOT_LOG_MAX_ENTRIES)
        return false;

    const boot_log_entry_t *src = entry_at_index(index);

    /* Check for erased (never written) entry */
    if (entry_is_empty(src))
        return false;

    memcpy(entry, src, sizeof(*entry));

    /* Verify CRC */
    uint32_t stored_crc = entry->crc_entry;
    entry->crc_entry    = 0;
    uint32_t computed   = entry_crc(entry);
    entry->crc_entry    = stored_crc;

    return computed == stored_crc;
}

uint32_t boot_log_find_write_index(void)
{
    for (uint32_t i = 0; i < BOOT_LOG_MAX_ENTRIES; i++)
    {
        const boot_log_entry_t *e = entry_at_index(i);
        if (entry_is_empty(e))
            return i;
    }
    return BOOT_LOG_INDEX_INVALID;
}

uint32_t boot_log_valid_count(void)
{
    uint32_t count = 0;
    for (uint32_t i = 0; i < BOOT_LOG_MAX_ENTRIES; i++)
    {
        boot_log_entry_t temp;
        if (boot_log_read_entry(i, &temp))
            count++;
    }
    return count;
}

uint32_t boot_log_max_sequence(void)
{
    uint32_t max_seq = 0;
    for (uint32_t i = 0; i < BOOT_LOG_MAX_ENTRIES; i++)
    {
        boot_log_entry_t temp;
        if (boot_log_read_entry(i, &temp) && temp.sequence > max_seq)
            max_seq = temp.sequence;
    }
    return max_seq;
}

bool boot_log_clear(void)
{
#if PICO_BUILD
    uint32_t ints = save_and_disable_interrupts();
    flash_range_erase(BOOT_LOG_FLASH_OFF, BOOT_LOG_SIZE);
    restore_interrupts(ints);
    return true;
#else
    (void)0;
    return false;
#endif
}
