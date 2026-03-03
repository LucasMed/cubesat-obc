#!/usr/bin/env node
/**
 * emulate_boot.mjs
 * ─────────────────────────────────────────────────────────────────────────────
 * Boot smoke-test for the CubeSat OBC firmware using the rp2040js RP2040
 * emulator.  Loads the cross-compiled ELF, runs the virtual MCU for up to
 * TIMEOUT_MS of simulated wall-time and checks that all REQUIRED_STRINGS
 * appear on UART0 output.
 *
 * Exit code:
 *   0  — all required boot strings received within timeout  (PASS)
 *   1  — timeout or missing string                          (FAIL)
 *   2  — usage / file-not-found error
 *
 * Usage:
 *   node emulate_boot.mjs <path/to/cubesat_obc_emu.elf> [timeout_ms]
 *
 * Note: the emulation target is built for pico_w (RP2040 / Cortex-M0+).
 *       The production flash target (pico2_w / RP2350 / Cortex-M33) uses the
 *       same source code but the rp2040js emulator only supports the RP2040
 *       core.  Architectural differences are negligible for boot validation.
 * ─────────────────────────────────────────────────────────────────────────────
 */

import { createReadStream, existsSync, readFileSync } from 'fs';
import { resolve } from 'path';

// ---------------------------------------------------------------------------
// Configuration
// ---------------------------------------------------------------------------

/** Strings that MUST appear on UART0 for the smoke-test to pass. */
const REQUIRED_STRINGS = [
    'CubeSat OBC',
    'Initializing system state',
    'Initializing fault manager',
    'Initializing EPS monitor',
    'Creating FreeRTOS tasks',
    'Starting FreeRTOS scheduler',
];

/** Maximum emulated wall-time to wait for all strings (milliseconds). */
const DEFAULT_TIMEOUT_MS = 8000;

/** How many CPU instructions to execute per "tick" before checking timers. */
const INSTRUCTIONS_PER_TICK = 10_000;

// ---------------------------------------------------------------------------
// Argument parsing
// ---------------------------------------------------------------------------

const elfPath = process.argv[2];
const timeoutMs = parseInt(process.argv[3] ?? String(DEFAULT_TIMEOUT_MS), 10);

if (!elfPath) {
    console.error('Usage: node emulate_boot.mjs <elf-file> [timeout_ms]');
    process.exit(2);
}

const resolvedPath = resolve(elfPath);
if (!existsSync(resolvedPath)) {
    console.error(`[emulate] ERROR: ELF not found: ${resolvedPath}`);
    process.exit(2);
}

// ---------------------------------------------------------------------------
// Load rp2040js
// ---------------------------------------------------------------------------

let RP2040, loadELF_fn;
try {
    // rp2040js >= 0.19 — ESM named exports
    const mod = await import('rp2040js');
    RP2040 = mod.RP2040;
    loadELF_fn = mod.loadELF ?? null;
} catch (err) {
    console.error('[emulate] ERROR: rp2040js not installed — run: npm install -g rp2040js');
    console.error(err.message);
    process.exit(2);
}

// ---------------------------------------------------------------------------
// Initialise MCU
// ---------------------------------------------------------------------------

const mcu = new RP2040();

// Load ELF into flash
const elfData = readFileSync(resolvedPath);
if (loadELF_fn) {
    loadELF_fn(mcu, elfData);
} else {
    mcu.loadELF(elfData);
}

// ---------------------------------------------------------------------------
// Wire UART0 output → stdout, accumulate for string matching
// ---------------------------------------------------------------------------

let uartOutput = '';
const remaining = new Set(REQUIRED_STRINGS);
let allFound = false;

const onUartByte = (byte) => {
    const ch = String.fromCharCode(byte);
    process.stdout.write(ch);          // mirror to terminal in real-time
    uartOutput += ch;

    // Check each required string once
    for (const str of [...remaining]) {
        if (uartOutput.includes(str)) {
            remaining.delete(str);
            console.error(`  ✅  found: "${str}"`);
        }
    }

    if (remaining.size === 0 && !allFound) {
        allFound = true;
    }
};

// UART0 (GPIO 0/1, console output in obc_main.c)
if (mcu.uart && mcu.uart[0]) {
    mcu.uart[0].onByte = onUartByte;
}
// Some rp2040js builds expose uart0 / uart1 directly
if (mcu.uart0) {
    mcu.uart0.onByte = onUartByte;
}
// USB CDC stdout (pico_enable_stdio_usb — redirects printf → USB)
if (mcu.usbCtrl) {
    mcu.usbCtrl.onCDCByte = onUartByte;
}

// ---------------------------------------------------------------------------
// Run emulation loop
// ---------------------------------------------------------------------------

console.error(`\n[emulate] Loading: ${resolvedPath}`);
console.error(`[emulate] Timeout: ${timeoutMs} ms`);
console.error(`[emulate] Waiting for ${REQUIRED_STRINGS.length} required strings…\n`);
console.error('─'.repeat(60));

const startReal = Date.now();
let simCycles = 0;
const cyclesPerMs = 133_000;          // RP2040 default 133 MHz
const maxCycles = BigInt(Math.round(timeoutMs * cyclesPerMs));

try {
    while (BigInt(simCycles) < maxCycles) {
        mcu.executeInstruction();
        simCycles++;

        // Check wall-clock timeout every INSTRUCTIONS_PER_TICK instructions
        if (simCycles % INSTRUCTIONS_PER_TICK === 0) {
            if (allFound) break;
            if (Date.now() - startReal > timeoutMs + 5000) {
                // Hard wall-clock guard (5 s grace on top of simulated timeout)
                console.error('\n[emulate] Wall-clock guard tripped — terminating.');
                break;
            }
        }
    }
} catch (err) {
    // MCU exceptions (HardFault, WDT reset) caught here
    console.error(`\n[emulate] MCU exception: ${err.message}`);
}

// ---------------------------------------------------------------------------
// Results
// ---------------------------------------------------------------------------

console.error('\n' + '─'.repeat(60));
console.error('[emulate] Boot smoke-test results:');
console.error(`  Simulated cycles : ${simCycles.toLocaleString()}`);
console.error(`  Wall time        : ${Date.now() - startReal} ms`);

if (allFound) {
    console.error('  Status           : ✅  PASS — all required strings received');
    process.exit(0);
} else {
    console.error('  Status           : ❌  FAIL — missing required strings:');
    for (const s of remaining) {
        console.error(`      • "${s}"`);
    }
    process.exit(1);
}
