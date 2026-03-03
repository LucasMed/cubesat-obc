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
// Load rp2040js  (local install at docker/node_modules — ESM resolves from
//                 the script's directory, no global install needed)
// ---------------------------------------------------------------------------

let RP2040;
try {
    const mod = await import('rp2040js');
    RP2040 = mod.RP2040;
} catch (err) {
    console.error('[emulate] ERROR: rp2040js not found.');
    console.error('  Run: npm install --prefix docker');
    console.error(err.message);
    process.exit(2);
}

// ---------------------------------------------------------------------------
// Minimal ELF loader  (rp2040js v0.19 removed its built-in loadELF helper)
// Parses 32-bit LE ELF, writes PT_LOAD segments that fall inside XIP flash
// (0x10000000–0x13FFFFFF) directly into mcu.flash.
// ---------------------------------------------------------------------------

function loadELFIntoMCU(mcu, data) {
    const FLASH_BASE = 0x10000000;
    const FLASH_SIZE = 16 * 1024 * 1024;  // 16 MB
    const view = new DataView(data.buffer, data.byteOffset, data.byteLength);

    // Verify ELF magic
    if (data[0] !== 0x7f || data[1] !== 0x45 || data[2] !== 0x4c || data[3] !== 0x46) {
        throw new Error('Not a valid ELF file (bad magic bytes)');
    }

    const phOff = view.getUint32(28, true);  // e_phoff
    const phEntSize = view.getUint16(42, true);  // e_phentsize
    const phNum = view.getUint16(44, true);  // e_phnum

    let segmentsLoaded = 0;
    for (let i = 0; i < phNum; i++) {
        const base = phOff + i * phEntSize;
        const pType = view.getUint32(base, true); // p_type
        const pOff = view.getUint32(base + 4, true); // p_offset
        const pVaddr = view.getUint32(base + 8, true); // p_vaddr
        const pFilesz = view.getUint32(base + 16, true); // p_filesz

        if (pType !== 1 /* PT_LOAD */ || pFilesz === 0) continue;

        if (pVaddr >= FLASH_BASE && pVaddr < FLASH_BASE + FLASH_SIZE) {
            const flashOff = pVaddr - FLASH_BASE;
            const segment = data.slice(pOff, pOff + pFilesz);
            mcu.flash.set(segment, flashOff);
            segmentsLoaded++;
        }
    }

    if (segmentsLoaded === 0) {
        throw new Error('ELF has no PT_LOAD segments in XIP flash range (0x10000000)');
    }
    return segmentsLoaded;
}

// ---------------------------------------------------------------------------
// Initialise MCU
// ---------------------------------------------------------------------------

const mcu = new RP2040();

// Load ELF into flash
const rawBuf = readFileSync(resolvedPath);
const elfData = new Uint8Array(rawBuf.buffer, rawBuf.byteOffset, rawBuf.byteLength);
const segsLoaded = loadELFIntoMCU(mcu, elfData);
console.error(`[emulate] Loaded ${segsLoaded} ELF segment(s) into flash.`);

// Point VTOR at XIP flash so the core reads the vector table from the firmware.
// IMPORTANT: use mcu.core.reset() NOT mcu.reset() — the latter fills flash with
// 0xFF, wiping our loaded ELF.
mcu.core.VTOR = 0x10000000;
mcu.core.reset();  // reads SP + PC from flash[0] / flash[4]

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
// mcu.uart[0] always exists in rp2040js v0.19
mcu.uart[0].onByte = onUartByte;

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
const maxCycles = timeoutMs * cyclesPerMs;

try {
    for (let i = 0; i < maxCycles; i++) {
        mcu.step();

        // Check wall-clock timeout every INSTRUCTIONS_PER_TICK steps
        if (i % INSTRUCTIONS_PER_TICK === 0) {
            if (allFound) {
                simCycles = i;
                break;
            }
            if (Date.now() - startReal > timeoutMs + 5000) {
                // Hard wall-clock guard (5 s grace on top of simulated timeout)
                simCycles = i;
                console.error('\n[emulate] Wall-clock guard tripped — terminating.');
                break;
            }
        }
    }
    if (simCycles === 0) simCycles = maxCycles;
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
