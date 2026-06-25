# Memory Map — RP2350 Flash & SRAM Layout

**Last Updated**: 2026-06-24

---

## Overview

The CubeSat OBC runs on a Raspberry Pi **Pico 2W (RP2350A)** with:

| Resource | Size | Address Range |
|----------|------|---------------|
| Internal XIP Flash (W25Q32) | **4 MB** | `0x10000000` – `0x103FFFFF` |
| Main SRAM | **512 KB** | `0x20000000` – `0x2007FFFF` |
| Scratch X (core 1 stack) | **4 KB** | `0x20080000` – `0x20080FFF` |
| Scratch Y (core 0 stack) | **4 KB** | `0x20081000` – `0x20081FFF` |
| **Total SRAM** | **520 KB** | `0x20000000` – `0x20081FFF` |

> The RP2350A on Pico 2W has 520 KB of total SRAM. 512 KB are general-purpose;
> the remaining 8 KB are partitioned into two 4 KB scratch regions for
> interrupt/stack isolation.

---

## Flash Layout (4 MB XIP)

The flash is divided into regions for dual-slot boot with golden-image recovery.
Single source of truth: [`include/internal_flash_layout.h`](../../include/internal_flash_layout.h).

```
0x10000000 ┌──────────────────────────────┐
           │  Bootloader  (64 KB)         │  ← CRC32 validation, slot select,
           │                              │    golden restore
0x1000FFFF ├──────────────────────────────┤
0x10010000 ┌──────────────────────────────┐
           │  Slot A  (1 MB)              │  ← Primary firmware image
           │  cubesat_obc_pico            │    (linked at 0x10010000)
0x1010FFFF ├──────────────────────────────┤
0x10110000 ┌──────────────────────────────┐
           │  Slot B  (1 MB)              │  ← Fallback firmware image
           │                              │    (identical layout)
0x1020FFFF ├──────────────────────────────┤
0x10210000 ┌──────────────────────────────┐
           │  Golden Metadata  (4 KB)     │  ← CRC, version, size of golden
0x10210FFF ├──────────────────────────────┤
0x10211000 ┌──────────────────────────────┐
           │  Golden Image  (956 KB)      │  ← Pristine firmware copy for
           │                              │    factory restore (W25Q64 → flash)
0x1030FFFF ├──────────────────────────────┤
0x10310000 ┌──────────────────────────────┐
           │  FMM Metadata  (4 KB)        │  ← Boot record ring, failure
           │                              │    counters, slot metadata
0x10310FFF ├──────────────────────────────┤
0x10311000 ┌──────────────────────────────┐
           │  Reserved  (956 KB)          │  ← Future use (FMM records,
           │                              │    extended logging, etc.)
0x103FFFFF └──────────────────────────────┘
```

### Region Details

| Region | Base | Size | Linker Script | Content |
|--------|------|------|---------------|---------|
| **Bootloader** | `0x10000000` | 64 KB | [`linker/memmap_bootloader.ld`](../../linker/memmap_bootloader.ld) | CRC32 validation, slot select, golden restore, SPI flash driver |
| **Slot A** | `0x10010000` | 1 MB | [`linker/memmap_golden.ld`](../../linker/memmap_golden.ld) | Primary firmware — FreeRTOS tasks, control loops, drivers |
| **Slot B** | `0x10110000` | 1 MB | (same linker, offset adjusted) | Fallback image for A/B update |
| **Golden Metadata** | `0x10210000` | 4 KB | — | CRC32, version, size of golden image |
| **Golden Image** | `0x10211000` | 956 KB | — | Pristine firmware copy on external W25Q64 SPI flash |
| **FMM Metadata** | `0x10310000` | 4 KB | — | Boot metadata (`boot_meta_t`), per-slot metadata (`slot_metadata_t`) |
| **Reserved** | `0x10311000` | 956 KB | — | Future use |

### Boot Metadata Layout (FMM sector, 4 KB at `0x10310000`)

```
Offset 0x0000 ├── boot_meta_t  (~64 B)    ← magic, slot fail counters, POST codes
Offset 0x0040 ├── Slot A metadata (24 B)   ← slot_metadata_t for Slot A
Offset 0x0080 ├── Slot B metadata (24 B)   ← slot_metadata_t for Slot B
Offset 0x00C0 └── (unused / future)        ← remaining 3904 bytes
```

Both `slot_metadata_t` and `boot_meta_t` have CRC32 self-checks.

#### slot_metadata_t (24 bytes, per slot)

```c
typedef struct __attribute__((packed)) {
    uint32_t crc32;        // CRC32 of the entire slot content
    uint32_t image_size;   // Binary size in bytes
    uint32_t version;      // Build version / timestamp
    uint8_t  slot_status;  // 0=empty, 1=valid, 2=pending, 3=failed
    uint8_t  failure_count;// Consecutive boot failures
    uint8_t  reserved[10]; // Pad to 24 bytes
} slot_metadata_t;
```

#### boot_meta_t

```c
typedef struct __attribute__((packed)) {
    uint32_t magic;            // "BOOT" (0x424F4F54)
    uint8_t  current_slot;     // 0=Slot A, 1=Slot B
    uint8_t  slot_a_failures;  // Consecutive boot failures for Slot A
    uint8_t  slot_b_failures;  // Consecutive boot failures for Slot B
    uint8_t  boot_reason;      // POST code reason
    uint32_t timestamp;        // Boot timestamp
    uint32_t last_jump_addr;   // Last address jumped to
    uint8_t  last_crc_result;  // CRC_RESULT_* from most recent attempt
    uint8_t  _pad[3];          // Reserved
    uint32_t crc32;            // CRC32 over magic.._pad fields
} boot_meta_t;
```

### Boot Flow

```
Reset → BootROM → Bootloader @ 0x10000000
                       │
                       ├─ Slot A CRC32 OK?  ──✓──→ Jump to Slot A
                       │     │ FAIL
                       │     ▼
                       ├─ Slot B CRC32 OK?  ──✓──→ Jump to Slot B
                       │     │ FAIL
                       │     ▼
                       ├─ Fresh binary? (valid ARM vector table)
                       │     │ YES → Jump (trust-on-first-boot)
                       │     ▼
                       ├─ Golden restore    ──✓──→ Restore Slot A → jump
                       │     │ FAIL
                       │     ▼
                       └─ HALT (wfi) — POST_GOLDEN_CRC_FAIL
```

---

## SRAM Layout (520 KB)

The firmware linker script (`linker/memmap_golden.ld`) partitions SRAM as:

```
0x20000000 ┌──────────────────────────────┐
           │  .data        (RW)           │  ← Initialized data (copied from flash)
           │  .bss         (ZI)           │  ← Zero-initialized data
           │  .heap        (dyn)          │  ← FreeRTOS heap (pvPortMalloc)
           │  .stack       (growing down) │  ← Main/Core 0 C stack (from __StackLimit)
           │  boot_info    (NOLOAD)       │  ← Boot info at ~0x2007FF00
           │  (unused)                     │
0x2007FFFF ├──────────────────────────────┤
           │  End of main SRAM            │
0x20080000 ┌──────────────────────────────┐
           │  SCRATCH_X  (4 KB)           │  ← Core 1 stack (stack1_dummy)
           │  .scratch_x (NOLOAD)         │  ← Core 1 scratch data
0x20080FFF ├──────────────────────────────┤
0x20081000 ┌──────────────────────────────┐
           │  SCRATCH_Y  (4 KB)           │  ← Core 0 main stack (.stack_dummy)
           │  .scratch_y (NOLOAD)         │  ← Core 0 scratch data
0x20081FFF └──────────────────────────────┘
```

### Key SRAM Symbols

| Symbol | Address | Description |
|--------|---------|-------------|
| `__StackLimit` | `0x20080000` | Bottom of main stack (== end of heap) |
| `__StackTop` | `0x20081FFF` | Top of main stack (Core 0) |
| `__StackOneTop` | `0x20080FFF` | Top of Core 1 stack |
| `__StackBottom` | `0x20080000` + (4 KB – stack1_dummy size) | |
| `__StackOneBottom` | `0x20080000` + (4 KB – stack1_dummy size) | |
| `__HeapLimit` | `0x20080000` | End of heap region |
| `__heap_start` | `__end__` (after .bss) | Start of heap |
| `__flash_binary_start` | `0x10010000` | Slot A base |
| `__flash_binary_end` | varies | End of firmware binary |

> **Important**: `__StackLimit ≡ __HeapLimit ≡ 0x20080000`. This means heap and
> stack grow toward each other. The linker asserts `__StackLimit >= __HeapLimit`.

### Boot Info Region

The bootloader writes POST codes to a known SRAM location for diagnostic readback:

- **POST code**: `*(volatile uint32_t *)0x20040000`
- **POST valid marker**: `*(volatile uint32_t *)0x20040004` (written as `0x504F5354` = `"POST"`)

This survives soft reset (SRAM is retained) and can be read by a debugger or
a subsequent boot stage to determine why the last boot failed.

### FreeRTOS Heap

Configured in [`config/FreeRTOSConfig.h`](../../config/FreeRTOSConfig.h):

| Parameter | Value | Description |
|-----------|-------|-------------|
| `configTOTAL_HEAP_SIZE` | 128 KB | Heap available via `pvPortMalloc`/`pvPortCalloc` |
| `configMINIMAL_STACK_SIZE` | 1024 words (4 KB) | Idle task stack |
| `configTIMER_TASK_STACK_DEPTH` | 1024 words (4 KB) | Timer service task stack |
| `configCHECK_FOR_STACK_OVERFLOW` | 2 (method 2) | Hardware stack overflow detection |

FreeRTOS uses **heap_4** (or `pvPortCalloc`-based allocator), which coalesces
adjacent freed blocks. With 128 KB heap, all 8 tasks plus driver buffers fit
comfortably (typical peak usage ~30–40 KB).

### Stack Configuration

The RP2350 has **three stack regions**:

| Region | Core | Size | Used For |
|--------|------|------|----------|
| Main stack (SCRATCH_Y) | Core 0 | 4 KB | `main()`, interrupts, C runtime |
| Core 1 stack (SCRATCH_X) | Core 1 | 4 KB | Core 1 startup (when SMP is re-enabled) |
| Task stacks (via `pvPortMalloc`) | Any | Per-task (4 KB default) | FreeRTOS task stacks |

Each FreeRTOS task gets its own stack allocated from the heap via
`xTaskCreate()`. The default is `configMINIMAL_STACK_SIZE` (4 KB), but
sensor/comms tasks may request larger stacks.

**Stack overflow detection**: `configCHECK_FOR_STACK_OVERFLOW == 2` uses
method 2 (comparing stack pointer against verified bounds on every context
switch). The hook `vApplicationStackOverflowHook()` is called on detection.

---

## Bootloader SRAM Layout

The bootloader uses a simpler memory map (`linker/memmap_bootloader.ld`):

| Region | Address | Size |
|--------|---------|------|
| RAM | `0x20000000` | **128 KB** (not the full 512 KB) |
| SCRATCH_X | `0x20080000` | 4 KB |
| SCRATCH_Y | `0x20081000` | 4 KB |

The bootloader restricts itself to 128 KB of RAM to minimise resource usage
(it only needs CRC32 buffers and the SPI flash golden-copy path). The scratch
regions remain at the same absolute addresses for core isolation.

During **golden restore**, the bootloader uses a 256-byte SRAM buffer
(`SRAM_BUF_SIZE`) to copy the golden image from W25Q64 → internal flash
in page-sized chunks.

---

## Build Outputs

| Artifact | From | Maps To | Description |
|----------|------|---------|-------------|
| `cubesat_obc_bootloader.uf2` | bootloader-build | `0x10000000` – `0x1000FFFF` | Bootloader binary |
| `cubesat_obc_pico.uf2` | pico-build | `0x10010000` – slot limit | Slot A firmware |
| `cubesat_obc_combined.uf2` | Both (via `combine_uf2.py`) | `0x10000000` – slot end | **Combined: bootloader + firmware**, ready to flash |

The combined UF2 is the **recommended deployment artifact**. It contains
both the bootloader and firmware in a single flash operation, placed at their
correct XIP addresses.

---

## References

- [`include/internal_flash_layout.h`](../../include/internal_flash_layout.h) — Single source of truth for flash region bases/sizes
- [`linker/memmap_golden.ld`](../../linker/memmap_golden.ld) — Firmware linker script
- [`linker/memmap_bootloader.ld`](../../linker/memmap_bootloader.ld) — Bootloader linker script
- [`config/FreeRTOSConfig.h`](../../config/FreeRTOSConfig.h) — Heap and stack config
- [`bootloader/bootloader.c`](../../bootloader/bootloader.c) — Boot flow and golden restore
- [`scripts/pico_ci.sh`](../../scripts/pico_ci.sh) — CI pipeline that builds both UF2s and combines them
- [`scripts/combine_uf2.py`](../../scripts/combine_uf2.py) — UF2 combiner tool
