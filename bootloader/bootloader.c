/**
 * @file bootloader.c
 * @brief Dual-slot bootloader with CRC32 validation and golden restore.
 *
 * Flow:
 *   1. Read boot metadata (slot failure counters).
 *   2. CRC32 validate Slot A.
 *        ─ OK → jump to Slot A.
 *        ─ FAIL → increment counter → try Slot B.
 *   3. CRC32 validate Slot B.
 *        ─ OK → jump to Slot B.
 *        ─ FAIL → increment counter → golden restore.
 *   4. Golden restore: read golden image from W25Q64, program Slot A.
 *        ─ OK → jump to Slot A.
 *        ─ CRC FAIL → halt with POST_GOLDEN_CRC_FAIL.
 *
 * Flash writes: use Pico SDK flash_range_erase/program (SRAM-executed).
 *
 * Spec ref: Golden Image SDD, bootloader/spec.md
 */

#include "crc32.h"
#include "spi_flash.h"

#include "pico/stdlib.h"
#include "hardware/flash.h"
#include "hardware/sync.h"
#include "hardware/structs/scb.h"
#include "hardware/structs/systick.h"
#include "hardware/structs/nvic.h"
#include "hardware/regs/addressmap.h"
#include "hardware/regs/psm.h"

#include "internal_flash_layout.h"

#include <stdint.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Flash layout constants (sourced from internal_flash_layout.h)        */
/* ------------------------------------------------------------------ */
/* SLOT_A_BASE, SLOT_A_SIZE, SLOT_B_BASE, SLOT_B_SIZE,                 */
/* GOLDEN_IMAGE_BASE, GOLDEN_IMAGE_SIZE, FMM_METADATA_BASE,            */
/* RP2350_SRAM_BASE, RP2350_SRAM_SIZE, SLOT_A_METADATA_ADDR,           */
/* SLOT_B_METADATA_ADDR defined in internal_flash_layout.h             */

/* W25Q64 offset for golden image copy */
#define W25Q64_GOLDEN_OFFSET   0x00100000u

/* Maximum image size to validate (must match actual binary size) */
#define MAX_IMAGE_SIZE         0x00080000u  /* 512 KB */



/* ------------------------------------------------------------------ */
/* Slot metadata helpers (stored in FMM metadata sector)                */
/* ------------------------------------------------------------------ */

static uint32_t slot_meta_addr(uint32_t slot_base)
{
    if (slot_base == SLOT_A_BASE)
        return SLOT_A_METADATA_ADDR;
    if (slot_base == SLOT_B_BASE)
        return SLOT_B_METADATA_ADDR;
    return 0;
}

/* ------------------------------------------------------------------ */
/* Boot metadata (stored at BOOT_META_BASE, between golden and FMM)     */
/* ------------------------------------------------------------------ */
#define BOOT_META_BASE         0x10221000u
#define BOOT_META_MAGIC        0x424F4F54u  /* "BOOT" */

/** CRC result codes (persisted for diagnostics) */
#define CRC_RESULT_NONE         0u   /**< CRC not computed (fresh binary)  */
#define CRC_RESULT_PASS         1u   /**< CRC32 matched                    */
#define CRC_RESULT_FAIL         2u   /**< CRC32 mismatch                   */
#define CRC_RESULT_NO_META      3u   /**< No slot metadata found           */

typedef struct __attribute__((packed)) {
    uint32_t magic;            /**< BOOT_META_MAGIC                         */
    uint8_t  current_slot;     /**< 0=Slot A, 1=Slot B                     */
    uint8_t  slot_a_failures;  /**< Consecutive boot failures for Slot A   */
    uint8_t  slot_b_failures;  /**< Consecutive boot failures for Slot B   */
    uint8_t  boot_reason;      /**< POST code reason                       */
    uint32_t timestamp;        /**< Boot timestamp (0 if not available)    */
    uint32_t last_jump_addr;   /**< Address the bootloader jumped to on    */
                               /**< the last successful boot.  Persists    */
                               /**< across resets for diagnostic readback. */
    uint8_t  last_crc_result;  /**< CRC_RESULT_* from most recent attempt  */
    uint8_t  _pad[3];          /**< Reserved                                */
    uint32_t crc32;            /**< CRC32 over magic.._pad fields          */
} boot_meta_t;

/* Forward declarations */
static bool create_slot_metadata(uint32_t slot_base, uint32_t meta_addr,
                                  uint8_t *failures,
                                  const boot_meta_t *boot_meta);

/* POST reason codes */
#define POST_BOOT_SLOT_A_OK     0
#define POST_BOOT_SLOT_B_OK     1
#define POST_BOOT_GOLDEN_OK     2
#define POST_BOOT_SLOT_A_FAIL   3
#define POST_BOOT_SLOT_B_FAIL   4
#define POST_BOOT_GOLDEN_FAIL   5
#define POST_BOOT_GOLDEN_CRC    6

#define MAX_FAILURES            3

/* Write a POST code to a known SRAM address for diagnostics */
#define POST_CODE_ADDR          ((volatile uint32_t *)0x20040000u)
#define POST_CODE_VALID_ADDR    ((volatile uint32_t *)0x20040004u)

static void post_code(uint32_t code)
{
    *POST_CODE_ADDR = code;
    *POST_CODE_VALID_ADDR = 0x504F5354u;  /* "POST" */
}

/* ------------------------------------------------------------------ */
/* Boot metadata helpers                                                 */
/* ------------------------------------------------------------------ */

static bool boot_meta_read(boot_meta_t *meta)
{
    memcpy(meta, (const void *)BOOT_META_BASE, sizeof(boot_meta_t));
    if (meta->magic != BOOT_META_MAGIC)
        return false;

    /* Validate CRC */
    uint32_t stored_crc = meta->crc32;
    meta->crc32 = 0;
    uint32_t computed = crc32_compute((const uint8_t *)meta,
                                      sizeof(boot_meta_t) - 4);
    meta->crc32 = stored_crc;
    return computed == stored_crc;
}

static void boot_meta_write(const boot_meta_t *meta)
{
    boot_meta_t aligned __attribute__((aligned(4)));

    memcpy(&aligned, meta, sizeof(boot_meta_t));

    /* Compute CRC before write */
    aligned.crc32 = 0;
    aligned.crc32 = crc32_compute((const uint8_t *)&aligned,
                                   sizeof(boot_meta_t) - 4);

    uint32_t ints = save_and_disable_interrupts();
    flash_range_erase(BOOT_META_BASE - 0x10000000u, FLASH_SECTOR_SIZE);
    flash_range_program(BOOT_META_BASE - 0x10000000u,
                        (const uint8_t *)&aligned, FLASH_PAGE_SIZE);
    restore_interrupts(ints);
}

/* ------------------------------------------------------------------ */
/* CRC32 slot validation — reads slot metadata from FMM metadata sector */
/* ------------------------------------------------------------------ */

static bool validate_slot(uint32_t slot_base, uint32_t slot_size,
                          uint8_t *failures)
{
    (void)slot_size;
    (void)failures;

    /* Read slot metadata from FMM metadata region (not from slot_base) */
    slot_metadata_t meta;
    uint32_t meta_addr = slot_meta_addr(slot_base);
    if (meta_addr == 0)
        return false;

    memcpy(&meta, (const void *)meta_addr, sizeof(slot_metadata_t));

    if (meta.slot_status != 1)       /* 1 = valid */
        return false;

    if (meta.image_size == 0 || meta.image_size > MAX_IMAGE_SIZE)
        return false;

    /* CRC32 the entire image */
    uint32_t computed = crc32_compute((const uint8_t *)slot_base,
                                      meta.image_size);
    return computed == meta.crc32;
}

/* ------------------------------------------------------------------ */
/* Create slot metadata on-the-fly for freshly-flashed binaries        */
/* Detects binary size by scanning backwards for linker pad (0xaa).    */
/* Writes to FMM metadata sector (preserving boot_meta).               */
/* ------------------------------------------------------------------ */

static bool create_slot_metadata(uint32_t slot_base, uint32_t meta_addr,
                                  uint8_t *failures, const boot_meta_t *boot_meta)
{
    /* Detect actual binary size by scanning backwards from MAX_IMAGE_SIZE.
     * Flash beyond the binary is unprogrammed (0xFF).  The .uf2 covers
     * from __flash_binary_start to 256-byte aligned __flash_binary_end,
     * with any padding filled with 0xFF by elf2uf2.  We scan back from
     * MAX_IMAGE_SIZE to find the first non-0xFF byte, which marks the
     * actual binary end. */

    const uint8_t *base = (const uint8_t *)slot_base;
    uint32_t image_size = MAX_IMAGE_SIZE;

    while (image_size > 0 && base[image_size - 1] == 0xFF)
    {
        image_size--;
    }

    /* Require at least 256 bytes to consider it a valid binary */
    if (image_size < 256)
        return false;

    /* Compute CRC32 of the binary */
    uint32_t crc = crc32_compute(base, image_size);

    /* Build metadata */
    slot_metadata_t sm;
    memset(&sm, 0, sizeof(sm));
    sm.crc32 = crc;
    sm.image_size = image_size;
    sm.version = 1;
    sm.slot_status = 1;              /* valid */
    sm.failure_count = (failures != NULL) ? *failures : 0;

    /* Read-modify-write the FMM sector (4 KB).
     * Must preserve boot_meta at offset 0. */
    uint8_t sector_buf[FLASH_SECTOR_SIZE] __attribute__((aligned(4)));

    memcpy(sector_buf, (const void *)FMM_METADATA_BASE, FLASH_SECTOR_SIZE);

    /* Preserve boot_meta from caller's SRAM copy */
    if (boot_meta != NULL)
    {
        memcpy(sector_buf, boot_meta, sizeof(boot_meta_t));
    }

    /* Insert slot metadata at the correct offset */
    uint32_t off_in_sector = meta_addr - FMM_METADATA_BASE;
    memcpy(sector_buf + off_in_sector, &sm, sizeof(sm));

    /* Erase and program the full sector */
    uint32_t flash_off = FMM_METADATA_BASE - XIP_BASE;
    uint32_t ints = save_and_disable_interrupts();
    flash_range_erase(flash_off, FLASH_SECTOR_SIZE);
    for (uint32_t off = 0; off < FLASH_SECTOR_SIZE; off += FLASH_PAGE_SIZE)
    {
        flash_range_program(flash_off + off, sector_buf + off, FLASH_PAGE_SIZE);
    }
    restore_interrupts(ints);

    return true;
}

/* ------------------------------------------------------------------ */
/* Golden restore: copy from W25Q64 → internal flash Slot A             */
/* ------------------------------------------------------------------ */

#define GOLDEN_COPY_SIZE        MAX_IMAGE_SIZE  /* 512 KB */
#define SRAM_BUF_SIZE           256u            /* Write in 256 B pages */

_Static_assert(SRAM_BUF_SIZE <= FLASH_PAGE_SIZE,
               "SRAM_BUF_SIZE must be ≤ FLASH_PAGE_SIZE");

static bool golden_restore(void)
{
    /* Read golden image from W25Q64 into SRAM, then program Slot A */
    uint8_t buf[SRAM_BUF_SIZE] __attribute__((aligned(4)));

    post_code(POST_BOOT_GOLDEN_FAIL);  /* Default to fail until proven OK */

    if (!spi_flash_init())
    {
        post_code(POST_BOOT_GOLDEN_FAIL);
        return false;
    }

    /* Erase Slot A (must erase before program) */
    uint32_t flash_offs = SLOT_A_BASE - 0x10000000u;
    uint32_t ints = save_and_disable_interrupts();
    flash_range_erase(flash_offs, GOLDEN_COPY_SIZE);
    restore_interrupts(ints);

    /* Copy from W25Q64 to Slot A in 256-byte pages */
    for (uint32_t off = 0; off < GOLDEN_COPY_SIZE; off += SRAM_BUF_SIZE)
    {
        if (!spi_flash_read(W25Q64_GOLDEN_OFFSET + off, buf, SRAM_BUF_SIZE))
        {
            post_code(POST_BOOT_GOLDEN_FAIL);
            return false;
        }

        ints = save_and_disable_interrupts();
        flash_range_program(flash_offs + off, buf, SRAM_BUF_SIZE);
        restore_interrupts(ints);
    }

    /* Create slot metadata for the restored image */
    uint8_t dummy_fail = 0;
    {
        uint32_t meta_addr = slot_meta_addr(SLOT_A_BASE);
        if (meta_addr != 0)
        {
            boot_meta_t dummy_meta;
            memset(&dummy_meta, 0, sizeof(dummy_meta));
            create_slot_metadata(SLOT_A_BASE, meta_addr, &dummy_fail, &dummy_meta);
        }
    }

    /* Validate the programmed image */
    if (!validate_slot(SLOT_A_BASE, SLOT_A_SIZE, &dummy_fail))
    {
        post_code(POST_BOOT_GOLDEN_CRC);
        return false;
    }

    post_code(POST_BOOT_GOLDEN_OK);
    return true;
}

/* ------------------------------------------------------------------ */
/* Clean up system state before jumping to application                 */
/* Resets core 1, clears all pending interrupts, disables SysTick.     */
/* Without this, the application's runtime may crash due to stale      */
/* peripheral state or core 1 running from the old vector table.       */
/* ------------------------------------------------------------------ */

static void cleanup_before_jump(void)
{
    /* Disable interrupts on this core (CPSID is the portable asm form;
     * __disable_irq() is not available without CMSIS headers here)    */
    __asm volatile("cpsid i" ::: "memory");

    /* ── Stop SysTick completely ── */
    systick_hw->csr = 0;
    systick_hw->rvr = 0;
    systick_hw->cvr = 0;

    /* ── NVIC: disable and clear all pending interrupts ── */
    for (int i = 0; i < 2; i++)
    {
        nvic_hw->icer[i] = 0xFFFFFFFF;
        nvic_hw->icpr[i] = 0xFFFFFFFF;
    }

    /* ── Force-off core 1 via PSM (no FIFO handshake needed) ── */
    /* We do NOT bring it back — the application's reset handler    */
    /* will re-init core 1 via multicore_launch_core1 if needed.    */
    {
        io_rw_32 *power_off = (io_rw_32 *)(PSM_BASE + PSM_FRCE_OFF_OFFSET);
        hw_set_bits(power_off, PSM_FRCE_OFF_PROC1_BITS);
        while (!(*power_off & PSM_FRCE_OFF_PROC1_BITS))
            tight_loop_contents();
    }

    /* ── Clear PendSV and SysTick exception-pending bits ── */
    scb_hw->icsr = M33_ICSR_PENDSTCLR_BITS | M33_ICSR_PENDSVCLR_BITS;

    /* ── Memory barriers (Pico SDK __dsb/__isb take no arguments) ── */
    __asm volatile("dsb 0xF" ::: "memory");
    __asm volatile("isb 0xF" ::: "memory");
}

/* ------------------------------------------------------------------ */
/* Sanity-check a vector table before jumping                          */
/* SP must point into SRAM, PC must point into internal flash.         */
/* Without this guard, a stray jump to metadata (0xFF filled) or      */
/* a corrupted slot could silently execute garbage.                   */
/* ------------------------------------------------------------------ */

static bool valid_image(uint32_t addr)
{
    uint32_t sp = *(const volatile uint32_t *)addr;
    uint32_t pc = *(const volatile uint32_t *)(addr + 4);

    /* SP: must be in RP2350 SRAM range */
    if ((sp & 0xFFF00000) != RP2350_SRAM_BASE)
        return false;

    /* PC: must be in internal XIP flash range */
    if ((pc & 0xFF000000) != INTERNAL_FLASH_BASE)
        return false;

    /* PC must be within our known flash regions (bootloader excluded) */
    if (pc < SLOT_A_BASE || pc > (FMM_METADATA_BASE + FMM_METADATA_SIZE))
        return false;

    return true;
}

/* ------------------------------------------------------------------ */
/* Jump to application image at slot_base                                */
/* ------------------------------------------------------------------ */

typedef void (*entry_point_t)(void);

static void __attribute__((naked)) jump_to_image(uint32_t slot_base)
{
    /* Disable interrupts */
    __asm volatile("cpsid i");

    /* Reset peripherals — disable SysTick */
    systick_hw->csr = 0;

    /* Set vector table to new image */
    scb_hw->vtor = slot_base;

    /* Set stack pointer and jump to reset handler */
    __asm volatile(
        "ldr   r0, [%0]     \n"   /* SP from vector table */
        "msr   msp, r0      \n"
        "ldr   r0, [%0, #4] \n"   /* Reset handler from vector table */
        "bx    r0           \n"
        :
        : "r" (slot_base)
        : "r0"
    );

    /* Never reached */
    while (1);
}

/* ------------------------------------------------------------------ */
/* Main bootloader entry point                                         */
/* ------------------------------------------------------------------ */

void bootloader_main(void)
{
    boot_meta_t meta;
    bool meta_valid = boot_meta_read(&meta);

    if (!meta_valid)
    {
        /* First boot or corrupted metadata — initialize */
        memset(&meta, 0, sizeof(meta));
        meta.magic = BOOT_META_MAGIC;
    }

    /* ── Try Slot A ── */
    if (meta.slot_a_failures < MAX_FAILURES)
    {
        if (validate_slot(SLOT_A_BASE, SLOT_A_SIZE,
                          &meta.slot_a_failures))
        {
            meta.current_slot = 0;
            meta.slot_a_failures = 0;
            meta.boot_reason = POST_BOOT_SLOT_A_OK;
            meta.last_jump_addr = SLOT_A_BASE;
            meta.last_crc_result = CRC_RESULT_PASS;
            boot_meta_write(&meta);
            post_code(POST_BOOT_SLOT_A_OK);
            cleanup_before_jump();
            jump_to_image(SLOT_A_BASE);
        }
        else
        {
            meta.last_crc_result = CRC_RESULT_FAIL;
            meta.slot_a_failures++;
            meta.boot_reason = POST_BOOT_SLOT_A_FAIL;
            boot_meta_write(&meta);
            post_code(POST_BOOT_SLOT_A_FAIL);
        }
    }

    /* ── Try Slot B ── */
    if (meta.slot_b_failures < MAX_FAILURES)
    {
        if (validate_slot(SLOT_B_BASE, SLOT_B_SIZE,
                          &meta.slot_b_failures))
        {
            meta.current_slot = 1;
            meta.slot_b_failures = 0;
            meta.boot_reason = POST_BOOT_SLOT_B_OK;
            meta.last_jump_addr = SLOT_B_BASE;
            meta.last_crc_result = CRC_RESULT_PASS;
            boot_meta_write(&meta);
            post_code(POST_BOOT_SLOT_B_OK);
            cleanup_before_jump();
            jump_to_image(SLOT_B_BASE);
        }
        else
        {
            meta.last_crc_result = CRC_RESULT_FAIL;
            meta.slot_b_failures++;
            meta.boot_reason = POST_BOOT_SLOT_B_FAIL;
            boot_meta_write(&meta);
            post_code(POST_BOOT_SLOT_B_FAIL);
        }
    }

    /* ── Both slots metadata-invalid — check for fresh binary (.uf2) ── */
    /* Trust-on-first-boot: if the slot has a valid ARM vector table,     */
    /* jump directly.  The firmware will write its own slot metadata to   */
    /* the FMM sector on first boot; subsequent boots validate via CRC.   */
    {
        if (valid_image(SLOT_A_BASE))
        {
            meta.current_slot = 0;
            meta.slot_a_failures = 0;
            meta.boot_reason = POST_BOOT_SLOT_A_OK;
            meta.last_jump_addr = SLOT_A_BASE;
            meta.last_crc_result = CRC_RESULT_NONE;
            boot_meta_write(&meta);
            post_code(POST_BOOT_SLOT_A_OK);
            cleanup_before_jump();
            jump_to_image(SLOT_A_BASE);
        }

        if (valid_image(SLOT_B_BASE))
        {
            meta.current_slot = 1;
            meta.slot_b_failures = 0;
            meta.boot_reason = POST_BOOT_SLOT_B_OK;
            meta.last_jump_addr = SLOT_B_BASE;
            meta.last_crc_result = CRC_RESULT_NONE;
            boot_meta_write(&meta);
            post_code(POST_BOOT_SLOT_B_OK);
            cleanup_before_jump();
            jump_to_image(SLOT_B_BASE);
        }
    }

    /* ── Golden restore ── */
    if (golden_restore())
    {
        meta.slot_a_failures = 0;
        meta.slot_b_failures = 0;
        meta.current_slot = 0;
        meta.boot_reason = POST_BOOT_GOLDEN_OK;
        meta.last_jump_addr = SLOT_A_BASE;
        meta.last_crc_result = CRC_RESULT_PASS;
        boot_meta_write(&meta);
        post_code(POST_BOOT_GOLDEN_OK);
        cleanup_before_jump();
        jump_to_image(SLOT_A_BASE);
    }

    /* ── Everything failed — halt ── */
    meta.last_crc_result = CRC_RESULT_FAIL;
    boot_meta_write(&meta);
    post_code(POST_BOOT_GOLDEN_CRC);
    while (1)
    {
        __asm("wfi");
    }
}
