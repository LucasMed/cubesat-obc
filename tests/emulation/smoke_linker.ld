/**
 * smoke_linker.ld — Minimal linker script for rp2040js smoke-test firmware
 *
 * Places .vectors at 0x10000000 (RP2040 XIP flash start) and everything
 * else immediately after.  No SDK, no bootloader, no data copy.
 */

MEMORY
{
    FLASH (rx)  : ORIGIN = 0x10000000, LENGTH = 256K
    SRAM  (rwx) : ORIGIN = 0x20000000, LENGTH = 256K
}

SECTIONS
{
    . = ORIGIN(FLASH);

    /* Vector table MUST be first so VTOR=0x10000000 reads SP and PC */
    .vectors :
    {
        *(.vectors)
    } > FLASH

    .text :
    {
        *(.text*)
        . = ALIGN(4);
    } > FLASH

    .rodata :
    {
        *(.rodata*)
        . = ALIGN(4);
    } > FLASH

    /* ARM exception tables — discard (not used in smoke test) */
    /DISCARD/ :
    {
        *(.ARM.exidx*)
        *(.ARM.extab*)
    }
}
