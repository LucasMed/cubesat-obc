/**
 * @file bootloader_asm.S
 * @brief Bootloader entry point and vector table.
 *
 * The RP2350 bootrom loads this image at 0x10000000 (XIP flash base)
 * and jumps to the reset handler. We set up a minimal C environment
 * and call bootloader_main().
 *
 * Spec ref: Golden Image SDD, bootloader/spec.md
 */

.syntax unified
.cpu    cortex-m33
.thumb

/* Stack size for bootloader — 2 KB is plenty */
.equ    BOOT_STACK_SIZE, 2048

.section .stack, "w"
.align  3
_boot_stack_start:
.space  BOOT_STACK_SIZE
_boot_stack_end:

/* ------------------------------------------------------------------ */
/* Vector table — must be at flash base (0x10000000)                    */
/* ------------------------------------------------------------------ */
.section .vectors, "a"
.align  9      /* 512-byte alignment for Cortex-M33 VTOR requirement */

.global _bootloader_vectors
_bootloader_vectors:
    .word _boot_stack_end           /* SP initial value      */
    .word _bootloader_reset         /* Reset handler         */
    .word _bootloader_nmi           /* NMI                   */
    .word _bootloader_hardfault     /* HardFault             */
    .word 0                         /* MemManage (unused)    */
    .word 0                         /* BusFault (unused)     */
    .word 0                         /* UsageFault (unused)   */
    .word 0                         /* Reserved              */
    .word 0                         /* Reserved              */
    .word 0                         /* Reserved              */
    .word 0                         /* SVCall (unused)       */
    .word 0                         /* DebugMonitor (unused) */
    .word 0                         /* Reserved              */
    .word 0                         /* PendSV (unused)       */
    .word 0                         /* SysTick (unused)      */
    /* External interrupts — all default handler */
    .rept 64
    .word _bootloader_default_isr
    .endr

/* ------------------------------------------------------------------ */
/* Default handlers — infinite loop with PC in LR for debug            */
/* ------------------------------------------------------------------ */
.section .text.default_handlers, "ax"

.thumb_func
_bootloader_default_isr:
    b _bootloader_default_isr

.thumb_func
_bootloader_nmi:
    b _bootloader_nmi

.thumb_func
_bootloader_hardfault:
    b _bootloader_hardfault

/* ------------------------------------------------------------------ */
/* data_cpy_table stub — needed by SDK .binary_info_header when        */
/* PICO_CRT0_NO_RESET_SECTION=1 excludes crt0's data_cpy_table        */
/* ------------------------------------------------------------------ */
.global data_cpy_table
.type data_cpy_table,%object
.align 2
data_cpy_table:
    .word 0    /* null terminator = no copy table */

/* Provide _reset_handler alias for Pico SDK crt0 compatibility.
 * The SDK's crt0.S defines a .vectors section that references this
 * symbol; we alias it to our custom reset handler so the linker
 * doesn't fail on unresolved references.                         */
.thumb_func
.global _reset_handler
.set _reset_handler, _bootloader_reset

/* ------------------------------------------------------------------ */
/* Reset handler — entry point called by bootrom                       */
/* ------------------------------------------------------------------ */
.section .text.reset, "ax"

.equ RESETS_BASE,         0x40020000
.equ RESET_RESET_OFFSET,  0x00
.equ RESET_RESET_DONE_OFFSET, 0x08
/* Bits for peripherals used by the bootloader:
   IO_BANK0(6) | PADS_BANK0(9) | SPI0(16) | TIMER0(12) | UART0(14) */
.equ RESETS_BOOTLOADER_BITS, (0x40 | 0x200 | 0x10000 | 0x1000 | 0x4000)

.thumb_func
.global _bootloader_reset
_bootloader_reset:
    /* Set stack pointer */
    ldr r0, =_boot_stack_end
    msr msp, r0

    /* De-assert RESETS for peripherals used by the bootloader.
       On RP2350, most peripherals start in reset. The SDK's crt0
       normally handles this in its .reset section, which we skip
       via PICO_CRT0_NO_RESET_SECTION=1.                              */
    ldr r0, =RESETS_BASE
    ldr r1, [r0, #RESET_RESET_OFFSET]
    ldr r2, =RESETS_BOOTLOADER_BITS
    bics r1, r1, r2                 /* clear bits = de-assert reset */
    str r1, [r0, #RESET_RESET_OFFSET]
    /* Wait for peripherals to come out of reset */
1:  ldr r1, [r0, #RESET_RESET_DONE_OFFSET]
    tst r1, r2
    beq 1b

    /* Copy .data from FLASH to RAM.
     * .data contains .time_critical functions (flash_range_erase/program)
     * that MUST run from RAM because they disable XIP. Without this copy
     * the flash write functions would crash on first call.              */
    ldr r0, =__data_start__
    ldr r1, =__data_end__
    ldr r2, =__etext
    b .L_data_test
.L_data_loop:
    ldr r3, [r2], #4
    str r3, [r0], #4
.L_data_test:
    cmp r0, r1
    blo .L_data_loop

    /* Clear BSS */
    ldr r0, =__bss_start__
    ldr r1, =__bss_end__
    mov r2, #0
    b .L_bss_test
.L_bss_loop:
    str r2, [r0], #4
.L_bss_test:
    cmp r0, r1
    blo .L_bss_loop

    /* Call C entry point */
    bl bootloader_main

    /* If bootloader_main returns, loop forever */
.L_halt:
    b .L_halt
