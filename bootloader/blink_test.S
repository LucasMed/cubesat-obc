/**
 * @file blink_test.S
 * @brief Minimal bootloader blink test — RP2350 (Pico 2W)
 *
 * Offsets y bits verificados contra pico-sdk RP2350 y datasheet.
 *
 * Diferencias clave con RP2040:
 *   - RESET_IO_BANK0   = bit 6 (no bit 5)
 *   - RESET_PADS_BANK0 = bit 9 (no bit 8)
 *   - Aliases atómicos APB: +0x2000 = SET, +0x3000 = CLR
 *   - PADS_BANK0 tiene bit ISO [8] nuevo — hay que limpiarlo
 *   - SIO_GPIO_OE = 0x030, OE_SET = 0x038
 *   - SIO_GPIO_OUT = 0x010, OUT_SET = 0x018, OUT_CLR = 0x020
 *     (GPIO_HI_OUT en 0x014 desplazó SET/CLR vs RP2040)
 */

.syntax unified
.cpu    cortex-m33
.thumb

/* ------------------------------------------------------------------ */
/* Direcciones base                                                    */
/* ------------------------------------------------------------------ */
.equ RESETS_BASE,            0x40020000
.equ IO_BANK0_BASE,          0x40028000
.equ PADS_BANK0_BASE,        0x40038000
.equ SIO_BASE,               0xD0000000

/* Aliates atómicos APB (sección 2.1.3 datasheet RP2350)               */
.equ APB_XOR,                0x1000
.equ APB_SET,                0x2000
.equ APB_CLR,                0x3000

/* ------------------------------------------------------------------ */
/* RESETS (Cap. 7)                                                     */
/* ------------------------------------------------------------------ */
.equ RESET_RESET_OFFSET,     0x00
.equ RESET_RESET_DONE_OFFSET,0x08

.equ RESET_IO_BANK0,         (1 << 6)    /* bit 6 en RP2350 */
.equ RESET_PADS_BANK0,       (1 << 9)    /* bit 9 en RP2350 */

/* ------------------------------------------------------------------ */
/* PADS_BANK0 (Cap. 9.11.3)                                            */
/* ------------------------------------------------------------------ */
.equ PADS_GPIO0_OFFSET,      0x04        /* base + 0x04 = GPIO0 PADS */
.equ PAD_ISO,                (1 << 8)    /* Isolation latch — nuevo */
.equ PAD_OD,                 (1 << 7)    /* Output disable            */
.equ PAD_IE,                 (1 << 6)    /* Input enable              */
.equ PAD_DRIVE_MASK,         (3 << 4)    /* Drive strength bits 5:4  */
.equ PAD_DRIVE_4MA,          (1 << 4)    /* 01 = 4mA                  */

/* ------------------------------------------------------------------ */
/* IO_BANK0 (Cap. 9.11)                                                */
/* ------------------------------------------------------------------ */
.equ GPIO0_CTRL_OFFSET,      0x04        /* base + 0x04 = GPIO0 CTRL  */
.equ GPIO_CTRL_STRIDE,       8           /* cada GPIO ocupa 8 bytes   */

/* RP2350 GPIO0_CTRL fields:                                          */
/*   Bits [4:0]   FUNCSEL  (5 bits, no 3)                             */
/*   Bits [13:12] OUTOVER  (2 bits)                                    */
/*   Bits [15:14] OEOVER   (2 bits)                                    */
/*   Bits [17:16] INOVER   (2 bits)                                    */
/*   Bits [29:28] IRQOVER  (2 bits)                                    */
.equ FUNCSEL_SIO,            5           /* GPIO_FUNC_SIO = 5         */

/* OVER values (2-bit fields):                                        */
.equ OVER_NORMAL,            (0 << 12)
.equ OVER_INVERT,            (1 << 12)
.equ OVER_LOW,               (2 << 12)
.equ OVER_HIGH,              (3 << 12)

/* ------------------------------------------------------------------ */
/* SIO — Single-cycle I/O (Cap. 3.1)                                   */
/* Offsets correctos para RP2350                                       */
/* ------------------------------------------------------------------ */
.equ SIO_GPIO_OUT,           0x10        /* R/W — valor de salida     */
.equ SIO_GPIO_OUT_SET,       0x18        /* WO — set atómico          */
.equ SIO_GPIO_OUT_CLR,       0x20        /* WO — clear atómico        */
.equ SIO_GPIO_OE,            0x30        /* R/W — output enable       */
.equ SIO_GPIO_OE_SET,        0x38        /* WO — OE set atómico       */
.equ SIO_GPIO_OE_CLR,        0x40        /* WO — OE clear atómico     */

/* ------------------------------------------------------------------ */
/* Constantes de delay                                                 */
/* ------------------------------------------------------------------ */
.equ TIMEOUT_LOOPS,          10000
.equ DELAY_LOOPS,            1000

/* ------------------------------------------------------------------ */
/* Stack — minimal, nunca lo usamos pero VTOR lo requiere              */
/* ------------------------------------------------------------------ */
.section .stack, "w"
.align  3
_boot_stack_start:
.space  256
_boot_stack_end:

/* ------------------------------------------------------------------ */
/* Vector table — 512-byte aligned para Cortex-M33 VTOR                */
/* ------------------------------------------------------------------ */
.section .vectors, "a"
.align  9

.global _bootloader_vectors
_bootloader_vectors:
    .word _boot_stack_end           /* SP initial value              */
    .word _bootloader_reset         /* Reset handler                 */
    .word _bootloader_hang          /* NMI                           */
    .word _bootloader_hang          /* HardFault                     */
    .word 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0   /* reserved        */
    .rept 64
    .word _bootloader_hang          /* Default interrupt handlers    */
    .endr

/* ------------------------------------------------------------------ */
/* data_cpy_table stub — necesario para .binary_info_header            */
/* ------------------------------------------------------------------ */
.global data_cpy_table
.type data_cpy_table,%object
.align 2
data_cpy_table:
    .word 0    /* null terminator = no copy table */

/* ------------------------------------------------------------------ */
/* main() stub — weak, en caso crt0 lo referencie                      */
/* ------------------------------------------------------------------ */
.section .text.main_stub, "ax"
.global main
.weak   main
.thumb_func
main:
    b main

/* ------------------------------------------------------------------ */
/* Reset handler — parpadeo GP0                                        */
/* ------------------------------------------------------------------ */
.section .text, "ax"

.global _reset_handler
.set _reset_handler, _bootloader_reset

.thumb_func
.global _bootloader_reset
_bootloader_reset:

    /* =============================================================== */
    /* Step 1: De-assert RESETS usando CLR alias atómico (+0x3000)     */
    /* =============================================================== */
    ldr r0, =RESETS_BASE
    ldr r1, =(RESET_IO_BANK0 | RESET_PADS_BANK0)

    /* CLR alias (AND NOT) sobre RESETS_RESET — des-assert atómico     */
    movw r2, #APB_CLR
    str r1, [r0, r2]

    /* Esperar RESET_DONE con timeout                                  */
    movw r3, #TIMEOUT_LOOPS
1:  subs r3, r3, #1
    beq 2f
    ldr r2, [r0, #RESET_RESET_DONE_OFFSET]
    tst r2, r1
    beq 1b
2:

    /* Pequeño delay para que el bus fabric se asiente                  */
    ldr r3, =DELAY_LOOPS
3:  subs r3, r3, #1
    bne 3b

    /* =============================================================== */
    /* Step 2: Configurar PADS_GPIO0                                    */
    /*   - ISO=0  (bit 8) — desaislar pad                              */
    /*   - OD=0   (bit 7) — habilitar output driver                    */
    /*   - IE=1   (bit 6) — habilitar input buffer                     */
    /*   - DRIVE=01 (bits 5:4) = 4mA                                   */
    /*   - Sin pulls, sin schmitt                                       */
    /* =============================================================== */
    ldr r0, =(PADS_BANK0_BASE + PADS_GPIO0_OFFSET)
    movs r1, #(PAD_IE | PAD_DRIVE_4MA)   /* 0b0101_0000 = 0x50 */
    str r1, [r0]

    /* =============================================================== */
    /* Step 3: GPIO0_CTRL — FUNCSEL=SIO, sin overrides                  */
    /* Escribimos a las 3 vistas: PROC0, PROC1, CHIP                    */
    /* =============================================================== */
    ldr r4, =(IO_BANK0_BASE + GPIO0_CTRL_OFFSET)
    ldr r5, =(IO_BANK0_BASE + 0x2000 + GPIO0_CTRL_OFFSET)   /* PROC1 */
    ldr r6, =(IO_BANK0_BASE + 0x4000 + GPIO0_CTRL_OFFSET)   /* CHIP  */
    movw r7, #FUNCSEL_SIO                /* 0x0005                    */
    str r7, [r4]
    str r7, [r5]
    str r7, [r6]

    /* =============================================================== */
    /* Step 4: SIO — habilitar output enable para GPIO0                 */
    /*   OE_SET (0x038) — set atómico, no afecta otros GPIOs           */
    /* =============================================================== */
    ldr r0, =(SIO_BASE + SIO_GPIO_OE_SET)
    movs r1, #1                          /* bit 0 = GPIO0 OE */
    str r1, [r0]

    /* =============================================================== */
    /* Step 5: Blink — SIO_OUT_SET / SIO_OUT_CLR atómicos               */
    /* =============================================================== */
    ldr r4, =(SIO_BASE + SIO_GPIO_OUT_SET)
    ldr r5, =(SIO_BASE + SIO_GPIO_OUT_CLR)
    movs r6, #1                          /* máscara GPIO0 */

blink_loop:
    /* Prender GP0 */
    str r6, [r4]

    /* Delay ~500ms (a ~48MHz: ~24M ciclos) */
    ldr r0, =24000000
4:  subs r0, r0, #1
    bne 4b

    /* Apagar GP0 */
    str r6, [r5]

    /* Delay ~500ms */
    ldr r0, =24000000
5:  subs r0, r0, #1
    bne 5b

    b blink_loop

/* ------------------------------------------------------------------ */
/* Hang handler — loop infinito para excepciones                        */
/* ------------------------------------------------------------------ */
.thumb_func
_bootloader_hang:
    b _bootloader_hang
