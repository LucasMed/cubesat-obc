/**
 * @file io_access_test.S
 * @brief Test if IO_BANK0 registers are accessible on RP2350.
 *
 * Signals result on GP0:
 *   0.42V  = PADS write failed (peripheral still in reset)
 *   3.3V   = PADS write OK, but BusFault on IO_BANK0 read
 *   0V     = No BusFault, IO_BANK0 returned 0x1f (accessible)
 */

.syntax unified
.cpu    cortex-m33
.thumb

.equ RESETS_BASE,            0x40020000
.equ RESET_RESET_OFFSET,     0x00
.equ RESET_RESET_DONE_OFFSET,0x08

.equ RESETS_IO_BANK0_BIT,    (1 << 6)
.equ RESETS_PADS_BANK0_BIT,  (1 << 9)

.equ IO_BANK0_PROC0_BASE,    0x40028000
.equ GPIO0_CTRL_OFFSET,      0x04

.equ PADS_BANK0_BASE,        0x40038000
.equ PADS_GPIO0_OFFSET,      0x04

/* Timeout for RESET_DONE wait */
.equ TIMEOUT_LOOPS,          10000
/* Post-reset delay */
.equ DELAY_LOOPS,            1000

/* ------------------------------------------------------------------ */
/* Stack — minimal */
/* ------------------------------------------------------------------ */
.section .stack, "w"
.align  3
_boot_stack_start:
.space  256
_boot_stack_end:

/* ------------------------------------------------------------------ */
/* Vector table — 512-byte aligned */
/* ------------------------------------------------------------------ */
.section .vectors, "a"
.align  9

.global _bootloader_vectors
_bootloader_vectors:
    .word _boot_stack_end           /* SP                          */
    .word _bootloader_reset         /* Reset                       */
    .word _bootloader_hang          /* NMI                         */
    .word _bootloader_hang          /* HardFault                   */
    .rept 12
    .word _bootloader_hang          /* All exceptions -> hang      */
    .endr
    .rept 64
    .word _bootloader_hang          /* Default interrupt handlers  */
    .endr

/* ------------------------------------------------------------------ */
/* data_cpy_table stub — needed by binary_info_header when             */
/* PICO_CRT0_NO_RESET_SECTION=1 excludes crt0.s data_cpy_table        */
/* ------------------------------------------------------------------ */
.global data_cpy_table
.type data_cpy_table,%object
.align 2
data_cpy_table:
    .word 0    /* null terminator = no copy table */

/* ------------------------------------------------------------------ */
/* main() stub — called by crt0 if .reset section is included         */
/* ------------------------------------------------------------------ */
.section .text, "ax"

.global main
.weak   main
.thumb_func
main:
    b main

/* ------------------------------------------------------------------ */
/* Reset handler                                                       */
/* ------------------------------------------------------------------ */

.global _reset_handler
.set _reset_handler, _bootloader_reset

.thumb_func
.global _bootloader_reset
_bootloader_reset:
    /* Step 1: Unreset IO_BANK0 and PADS_BANK0.
     *         Write CLR alias (+0x2000) and also do read-modify-write
     *         to cover both RP2350 alias-aware and non-alias paths. */
    ldr r0, =RESETS_BASE
    ldr r1, =(RESETS_IO_BANK0_BIT | RESETS_PADS_BANK0_BIT)

    /* CLR alias write */
    movw r2, #0x2000
    str r1, [r0, r2]

    /* Read-modify-write fallback */
    ldr r2, [r0, #RESET_RESET_OFFSET]
    bics r2, r2, r1
    str r2, [r0, #RESET_RESET_OFFSET]

    /* Step 2: Wait for RESET_DONE with timeout.
     *         If timeout expires, continue anyway. */
    movw r3, #TIMEOUT_LOOPS
1:  subs r3, r3, #1
    beq 2f
    ldr r2, [r0, #RESET_RESET_DONE_OFFSET]
    tst r2, r1
    beq 1b
2:

    /* Step 3: Small delay for bus fabric to settle */
    ldr r3, =DELAY_LOOPS
3:  subs r3, r3, #1
    bne 3b

    /* Step 4: Write PADS_GPIO0 with PULL-UP.
     *         If PADS_BANK0 is accessible, GP0 → 3.3V */
    ldr r0, =(PADS_BANK0_BASE + PADS_GPIO0_OFFSET)
    movs r1, #0x88                    /* IE=1, PUE=1 */
    str r1, [r0]

    /* Step 5: READ IO_BANK0 GPIO0_CTRL at 0x40028004.
     *         BusFault here if IO_BANK0 is inaccessible. */
    ldr r3, =IO_BANK0_PROC0_BASE + GPIO0_CTRL_OFFSET
    ldr r4, [r3]

    /* Read succeeded — check value */
    ldr r5, =(PADS_BANK0_BASE + PADS_GPIO0_OFFSET)
    mov r6, #0x1f
    cmp r4, r6
    beq io_accessible

    /* Not 0x1f: keep pull-up (3.3V) */
    b test_done

io_accessible:
    /* 0x1f: pull-down (0V) */
    movs r6, #0x04
    str r6, [r5]

test_done:
    b test_done

/* ------------------------------------------------------------------ */
/* Hang handler                                                        */
/* ------------------------------------------------------------------ */
.thumb_func
_bootloader_hang:
    b _bootloader_hang
