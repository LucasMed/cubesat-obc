/**
 * smoke_test.s — Minimal RP2040 firmware for rp2040js boot smoke-test
 *
 * Purpose: prove that the emulator can execute ARM Cortex-M0 code, that the
 *          pipeline binary is valid, and that the "running" system transmits
 *          periodic telemetry STATUS packets over UART.
 *
 * Phases:
 *   1. Boot strings  — 6 required log lines (matched by emulate_boot.mjs)
 *   2. STATUS loop   — 5 telemetry packets sent with a simulated delay
 *      Format: \r\nSTATUS:mode=NOMINAL,bat=3300,temp=2500,uptime=NNN\r\n
 *      emulate_boot.mjs requires ≥ 3 packets to declare PASS.
 *
 * Design constraints:
 *   • Only Thumb-16 instructions + BL (Thumb-2, implemented in rp2040js)
 *   • No Pico SDK / FreeRTOS / libc
 *   • Writes directly to UART0 DR (0x40034000); rp2040js fires onByte
 *     for every writeUint32 to that address.
 */

    .syntax unified
    .cpu    cortex-m0plus
    .thumb

/* =========================================================================
 * Cortex-M vector table
 * ========================================================================= */
    .section .vectors, "ax"
    .global _vectors
_vectors:
    .word   0x20040000              /* Initial SP = top of 256 KB SRAM0 */
    .word   _reset + 1              /* Reset vector (+1 = Thumb mode)   */


/* =========================================================================
 * Reset handler
 * ========================================================================= */
    .section .text, "ax"
    .global _reset
    .thumb_func
_reset:
    ldr     r5, =0x40034000         /* r5 = UART0 Data Register (preserved) */

    /* ── Phase 1: boot log strings ─────────────────────────────────── */
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

    /* ── Phase 2: periodic telemetry STATUS packets ─────────────────── */
    /*  r6 = remaining packet count (5)
     *  r7 = uptime counter (1 .. 5)                                    */
    movs    r6, #5
    movs    r7, #1

_pkt_loop:
    /* Simulated inter-packet delay: ~200 000 tight iterations          */
    ldr     r4, =200000
_delay:
    subs    r4, r4, #1
    bne     _delay

    /* Send packet header */
    ldr     r0, =str_status_hdr
    bl      _send_str

    /* Send uptime as zero-padded 3-digit decimal */
    mov     r0, r7
    bl      _send_dec3

    /* Send packet footer (CRLF) */
    ldr     r0, =str_crlf
    bl      _send_str

    adds    r7, r7, #1              /* uptime++ */
    subs    r6, r6, #1              /* packets-- */
    bne     _pkt_loop

_halt:
    b       _halt                   /* spin — wall-clock guard in JS exits */


/* -------------------------------------------------------------------------
 * _send_str: write null-terminated string to UART0
 *   r0  = pointer to string (clobbered)
 *   r5  = UART0 DR (preserved)
 * ---------------------------------------------------------------------- */
    .thumb_func
_send_str:
    push    {r4, lr}
_ss_loop:
    ldrb    r4, [r0]
    cmp     r4, #0
    beq     _ss_done
    str     r4, [r5]
    adds    r0, r0, #1
    b       _ss_loop
_ss_done:
    pop     {r4, pc}


/* -------------------------------------------------------------------------
 * _send_dec3: print r0 (0–255) as zero-padded 3-digit decimal to UART0
 *   r5  = UART0 DR (preserved)
 * ---------------------------------------------------------------------- */
    .thumb_func
_send_dec3:
    push    {r1, r2, r3, lr}

    /* hundreds */
    movs    r1, #0
    movs    r2, #100
_h:
    cmp     r0, r2
    blo     _h_done
    subs    r0, r0, r2
    adds    r1, r1, #1
    b       _h
_h_done:
    adds    r1, r1, #48             /* '0' */
    str     r1, [r5]

    /* tens */
    movs    r1, #0
    movs    r2, #10
_t:
    cmp     r0, r2
    blo     _t_done
    subs    r0, r0, r2
    adds    r1, r1, #1
    b       _t
_t_done:
    adds    r1, r1, #48
    str     r1, [r5]

    /* ones */
    adds    r0, r0, #48
    str     r0, [r5]

    pop     {r1, r2, r3, pc}


/* =========================================================================
 * String literals
 * ========================================================================= */
    .section .rodata, "a"

/* Boot log strings (must match REQUIRED_STRINGS in emulate_boot.mjs) */
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

/* Telemetry packet — uptime field appended by _send_dec3 */
str_status_hdr:
    .ascii  "\r\nSTATUS:mode=NOMINAL,bat=3300,temp=2500,uptime="
    .byte   0
str_crlf:
    .ascii  "\r\n"
    .byte   0

    .end
