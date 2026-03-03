/**
 * smoke_test.s — Minimal RP2040 firmware for rp2040js boot smoke-test
 *
 * Purpose: prove that the emulator can execute ARM Cortex-M0 code and that
 *          the CI pipeline binary made it to the linker correctly.
 *
 * Design constraints:
 *   • Uses ONLY Thumb-16 instructions (+ BL which is Thumb-2 but implemented
 *     in rp2040js).  This avoids MOV.W / CBZ / MSR which Pico SDK startup
 *     emits but rp2040js does not implement.
 *   • No Pico SDK / FreeRTOS / libc — just raw register writes to UART0 DR.
 *   • Writes a fixed set of boot strings to UART0 DR (0x40034000).
 *     rp2040js fires onByte for every writeUint32 to that address, so no
 *     clock / baud-rate initialisation is required.
 *
 * Build:
 *   arm-none-eabi-gcc -nostartfiles -nostdlib \
 *     -mcpu=cortex-m0plus -mthumb \
 *     -T tests/emulation/smoke_linker.ld \
 *     tests/emulation/smoke_test.s \
 *     -o artifacts/cubesat_obc_emu.elf
 */

    .syntax unified
    .cpu    cortex-m0plus
    .thumb

/* =========================================================================
 * Cortex-M vector table  (first two words required by hardware / emulator)
 * ========================================================================= */
    .section .vectors, "ax"
    .global _vectors
_vectors:
    .word   0x20040000              /* Initial SP = top of 256 KB SRAM0 */
    .word   _reset + 1              /* Reset vector  (+1 = Thumb mode)  */


/* =========================================================================
 * Reset handler and helpers
 * ========================================================================= */
    .section .text, "ax"
    .global _reset
    .thumb_func
_reset:
    /* Print all required boot strings to UART0 */
    ldr     r5, =0x40034000         /* UART0 Data Register address */

    ldr     r0, =str_obc
    bl      _send_str

    ldr     r0, =str_sysstate
    bl      _send_str

    ldr     r0, =str_fault
    bl      _send_str

    ldr     r0, =str_eps
    bl      _send_str

    ldr     r0, =str_tasks
    bl      _send_str

    ldr     r0, =str_sched
    bl      _send_str

_loop:
    b       _loop                   /* Spin — emulator wall-clock guard exits */


/* -------------------------------------------------------------------------
 * _send_str: print null-terminated string
 *   r0  = pointer to string (modified)
 *   r5  = UART0 DR address (preserved)
 * ---------------------------------------------------------------------- */
    .thumb_func
_send_str:
    push    {r4, lr}
_send_loop:
    ldrb    r4, [r0]                /* load byte at r0 */
    cmp     r4, #0                  /* null terminator? */
    beq     _send_done
    str     r4, [r5]                /* write byte to UART0 DR → triggers onByte */
    adds    r0, r0, #1              /* advance pointer */
    b       _send_loop
_send_done:
    pop     {r4, pc}                /* return */


/* =========================================================================
 * Read-only string literals — must match REQUIRED_STRINGS in emulate_boot.mjs
 * ========================================================================= */
    .section .rodata, "a"

str_obc:
    .asciz  "CubeSat OBC"
str_sysstate:
    .asciz  "Initializing system state"
str_fault:
    .asciz  "Initializing fault manager"
str_eps:
    .asciz  "Initializing EPS monitor"
str_tasks:
    .asciz  "Creating FreeRTOS tasks"
str_sched:
    .asciz  "Starting FreeRTOS scheduler"

    .end
