/**
 * @file boot_log.h
 * @brief Boot log ring buffer in internal flash.
 *
 * Stores 128 entries of 32 bytes each in a dedicated 4 KB flash sector.
 * Written by the bootloader at each boot event, readable by the FSW for
 * post-mortem / telemetry.
 *
 * Layout per entry (32 bytes):
 *   [0..3]   sequence       — monotonically increasing, starts at 1
 *   [4..7]   boot_count     — from boot_status_t
 *   [8..11]  reset_cause    — 1=POR, 2=WDT, 3=SW forced
 *   [12]     image_used     — 0=Slot A, 1=Slot B, 2=Golden
 *   [13]     crc_ok         — 1 if CRC passed
 *   [14]     fallback_used  — 1 if golden restore triggered
 *   [15]     _pad[1]        — reserved
 *   [16..19] bl_duration_ms — bootloader execution time
 *   [20..23] last_crc_computed
 *   [24..27] last_crc_expected
 *   [28..31] crc_entry      — CRC32 of bytes [0..27]
 *
 * Ring buffer: sequential writes, no overwrite. When the sector is full
 * (all 128 entries written), the sector is erased and writing restarts
 * at index 0. No wear-leveling needed — at 128 boots per cycle and
 * 100k erase cycles, this lasts ~12.8 million boots.
 *
 * Spec ref: PENDING_TASKS.md §9.5
 */

#ifndef BOOT_LOG_H
#define BOOT_LOG_H

#include <stdint.h>
#include <stdbool.h>

#include "internal_flash_layout.h"

/* ------------------------------------------------------------------ */
/* Constants                                                            */
/* ------------------------------------------------------------------ */

#define BOOT_LOG_ENTRY_SIZE    32u
#define BOOT_LOG_MAX_ENTRIES   (BOOT_LOG_SIZE / BOOT_LOG_ENTRY_SIZE)  /* 128 */

/* Sentinel for unused/full scan result */
#define BOOT_LOG_INDEX_INVALID 0xFFFFFFFFu

/* Image-used constants (matches BOOT_SLOT_A/B convention) */
#define BOOT_LOG_SLOT_A        0u
#define BOOT_LOG_SLOT_B        1u
#define BOOT_LOG_GOLDEN        2u

/* ------------------------------------------------------------------ */
/* Entry structure                                                      */
/* ------------------------------------------------------------------ */

typedef struct __attribute__((packed))
{
    uint32_t sequence;         /**< 1-based, monotonically increasing   */
    uint32_t boot_count;       /**< From boot_status_t.boot_count      */
    uint32_t reset_cause;      /**< 1=POR, 2=WDT, 3=SW forced          */
    uint8_t  image_used;       /**< BOOT_LOG_SLOT_A/B/GOLDEN           */
    uint8_t  crc_ok;           /**< 1 if image CRC passed              */
    uint8_t  fallback_used;    /**< 1 if golden restore was triggered   */
    uint8_t  _pad[1];          /**< Reserved                           */
    uint32_t bl_duration_ms;   /**< Bootloader execution time in ms    */
    uint32_t last_crc_computed;/**< CRC computed by bootloader          */
    uint32_t last_crc_expected;/**< Expected CRC from slot metadata     */
    uint32_t crc_entry;        /**< CRC32 of bytes [0..27]             */
} boot_log_entry_t;

_Static_assert(sizeof(boot_log_entry_t) == BOOT_LOG_ENTRY_SIZE,
               "boot_log_entry_t must be 32 bytes");

/* ------------------------------------------------------------------ */
/* API                                                                   */
/* ------------------------------------------------------------------ */

/**
 * @brief Write a boot log entry into the ring buffer.
 *
 * Finds the next free slot (first all-0xFF entry) and programs it.
 * If the sector is full, erases it and writes at index 0.
 * The crc_entry field is computed internally before writing.
 *
 * @param entry  Populated entry (crc_entry will be overwritten).
 * @return true on success, false if flash operations fail.
 */
bool boot_log_write(const boot_log_entry_t *entry);

/**
 * @brief Read a boot log entry by index.
 *
 * Simple XIP memory read — no locks, no side effects.
 *
 * @param index  0..BOOT_LOG_MAX_ENTRIES-1.
 * @param entry  [out] Filled with the entry data.
 * @return true if the entry has a valid CRC, false otherwise.
 */
bool boot_log_read_entry(uint32_t index, boot_log_entry_t *entry);

/**
 * @brief Find the next write index in the ring buffer.
 *
 * Scans entries sequentially. Returns the index of the first entry
 * in erased (all 0xFF) state, or BOOT_LOG_INDEX_INVALID if the
 * sector is full.
 *
 * @return Index 0..BOOT_LOG_MAX_ENTRIES-1, or BOOT_LOG_INDEX_INVALID.
 */
uint32_t boot_log_find_write_index(void);

/**
 * @brief Count valid entries in the boot log.
 *
 * @return Number of entries with a matching CRC.
 */
uint32_t boot_log_valid_count(void);

/**
 * @brief Get the highest sequence number in the log.
 *
 * @return 0 if the log is empty, otherwise the highest sequence found.
 */
uint32_t boot_log_max_sequence(void);

/**
 * @brief Erase the entire boot log sector.
 *
 * On PICO_BUILD: uses flash_range_erase (must run from SRAM).
 * On host builds: no-op (returns false).
 *
 * @return true on success, false on host build.
 */
bool boot_log_clear(void);

#endif /* BOOT_LOG_H */
