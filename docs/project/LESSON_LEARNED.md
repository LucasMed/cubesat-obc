# SDD Deploy Automation — Lessons Learned (2026-05-21)

## Separate the State Machine from the Orchestrator

**Problem**: Initial design considered adding auto-transition logic inside the FMM state machine itself.

**Solution**: Keeping FMM as a pure state machine and placing deploy orchestration in a separate `deploy_monitor` task was the right call. The deploy monitor calls `fmm_request_transition()` — it's an **operator** on FMM, not part of it. This separation made testing easier (deploy monitor tests mock FMM, FMM tests don't need deploy state) and keeps FMM reusable.

## Review Wiring Before Implementation Completion

**Problem**: The `deploy_monitor` task was never created in `obc_main.c`. All 22 other tasks were complete, all tests passed, but 0 of the deploy_monitor logic ran at runtime because neither `#include "deploy_monitor.h"` nor `xTaskCreate()` were added.

**Lesson**: Add a verification step that checks "is the new task created?" and "is the new code path reachable from `main()`?" as part of every task-level acceptance criteria. The verification phase found this (CRITICAL), but better to catch at apply time.

## Float Constants Need Explicit Suffixes in Embedded C

**Problem**: `DEPLOY_DETUMBLE_THRESHOLD 0.05f` without the `f` suffix can trigger double-precision math on ARM Cortex-M33, pulling in `__aeabi_dcmp` and bloating the binary.

**Solution**: All float constants in deploy_monitor.h use explicit `f` suffix.

## Leaky Counter vs Hard Reset

**Problem**: The spec originally said "reset to zero" on any ω ≥ 0.05 sample. With real IMU noise, this would prevent detumble transition from ever completing.

**Solution**: Use leaky counter hysteresis — decrement on borderline samples (0.05–0.10 rad/s), hard reset only on severe excursions (> 0.10 rad/s). Documented in the design as a resolved question.

## ISR Context Limitations (Resolved v0.33.0)

**Problem**: `fmm_force_safe()` can be called from ISR context (e.g., via fault chain), but `xTaskGetTickCount()` is not ISR-safe on all FreeRTOS ports. Originally we skipped `mode_entry_tick` update in ISR context and relied on the watchdog scratch register.

**Solution (v0.33.0)**: Added `data_layer_set_mode_entry_tick_from_isr()` which uses `taskENTER_CRITICAL_FROM_ISR()` / `taskEXIT_CRITICAL_FROM_ISR()` instead of the mutex. `fmm_force_safe()` now calls `data_layer_set_mode_entry_tick_from_isr(xTaskGetTickCountFromISR())` — fully ISR-safe.

**Bonus fix**: `dl_lock_from_isr()` was silently discarding the BASEPRI saved mask from `taskENTER_CRITICAL_FROM_ISR()` and passing 0 to `taskEXIT_CRITICAL_FROM_ISR()`. The lock/unlock pair now properly saves and restores the mask.

## Flash Layout Requires Centralised Management

**Problem**: Multiple subsystems (POST, telemetry storage, fault logs, config) write to W25Q64 flash. Without a centralised `flash_layout.h`, sector collisions are inevitable.

**Solution**: Created `include/flash_layout.h` as single source of truth with `static_assert` guards for non-overlapping regions.

## Test Harness Drift Risk

**Problem**: `process_text_command()` is gated behind `#ifdef PICO_BUILD`, so host tests use a reimplementation (`test_run_text_command`) which can drift from production code.

**Lesson**: Future changes should consider making the text command parser compilable on host by guarding only the hardware-specific parts.

---

# FreeRTOS on Raspberry Pi Pico 2 (Cortex-M33)
## Lesson Learned: Context Switching & Hardware Architecture Mismatch

### Problem Description
During hardware testing of the sub-system on the Raspberry Pi Pico 2W (RP2350), the system consistently crashed when FreeRTOS attempted its first context switch (`vTaskDelay()` or starting the scheduler). The system would reboot silently without hitting any standard HardFault handler.

### Investigation
Initial thoughts suspected issues with FPU usage or incorrect hardware dividers (specifically, the RP2040 uses custom hardware SIO dividers which don't map identically in RP2350). While disabling hardware SIO dividers in `FreeRTOSConfig.h` solved a BusFault, the context switch crash persisted.

A deeper architectural analysis revealed a fundamental conflict in exception handling between the FreeRTOS ports:
1.  **Original Configuration:** The build system imported `portable/ThirdParty/GCC/RP2040/library.cmake`. This port was explicitly designed for the RP2040 (Cortex-M0+ / ARMv6-M architecture).
2.  **Cortex-M0+ vs. Cortex-M33 Exception Handling:**
    *   **Cortex-M0+:** Exception returns (`EXC_RETURN`) are static (typically `0xFFFFFFFD` to return to Thread Mode using PSP). The M0+ port hardcoded this assumption in `xPortPendSVHandler`, restoring registers manually and jumping blindly via `bx r3`.
    *   **Cortex-M33 (ARMv8-M):** Features hardware Floating Point Units (FPU) and Security Extensions (TrustZone). `EXC_RETURN` dynamic values are structurally integral. If the FPU was used, `EXC_RETURN` changes (e.g., `0xFFFFFFBC`) to instruct the processor to execute an extended 104-byte unstacking procedure instead of the standard 32 bytes.
3.  **The Crash:** Executing a Cortex-M0+ `PendSV` context swap on a Cortex-M33 hardware corrupted the CPU stack unwinding phase. The CPU read an invalid or incompatible `EXC_RETURN` value combined with hardware stack-limit boundary registers (`PSPLIM`) missing from M0+ but enforced in M33, triggering recurrent unhandled faults.

### Solution
The FreeRTOS port implementation had to match the physical architecture of the RP2350 (Cortex-M33).

1.  **CMake Adjustments:**
    Removed the hardcoded RP2040 port import. Instead, defined a custom `config/library.cmake` that compiles the official FreeRTOS `ARM_CM33_NTZ` (Cortex-M33 Non-TrustZone) port.
    ```cmake
    target_sources(FreeRTOS-Kernel INTERFACE
        ${FREERTOS_KERNEL_PATH}/portable/GCC/ARM_CM33_NTZ/non_secure/port.c
        ${FREERTOS_KERNEL_PATH}/portable/GCC/ARM_CM33_NTZ/non_secure/portasm.c
    )
    ```

2.  **FreeRTOSConfig.h Variables:**
    Added specific Cortex-M33 FreeRTOS macros to enable the FPU and define priority levels:
    ```c
    #define configENABLE_FPU 1
    #define configENABLE_MPU 0
    #define configENABLE_TRUSTZONE 0
    #define configRUN_FREERTOS_SECURE_ONLY 1
    ```

3.  **Interrupt Re-Routing (Pico SDK compatibility):**
    The Pico SDK initialization headers globally rename standard CMSIS interrupt names (e.g., `PendSV_Handler` to `isr_pendsv` as weak assembly aliases) inside `crt0.S`. To properly override these with FreeRTOS implementations, the configuration must explicitly define the mappings:
    ```c
    #define vPortSVCHandler isr_svcall
    #define xPortPendSVHandler isr_pendsv
    #define xPortSysTickHandler isr_systick
    #define PendSV_Handler isr_pendsv
    #define SVC_Handler isr_svcall
    #define SysTick_Handler isr_systick
    ```

### Results
After pointing the build system to the M33 architecture port and aligning the alias definitions, the FreeRTOS scheduler operates perfectly, ticking correctly with stable stack contexts.

---

# FR-17 Payload HK in Telemetry — Lessons Learned (2026-06-05)

## Verify Documentation Fixes Separately

**Problem**: During verify, the SoftwareSerial buffer comment in `telemetry_packet.h` flagged as WARNING — the apply-progress claimed it was fixed but the file still contained the old claim ("Fits entirely in SoftwareSerial 64-byte buffer — no overflow").

**Root cause**: No dedicated verification step for post-implementation documentation cleanup. The comment fix was mentioned in a dev note but never actually applied.

**Lesson**: When documentation cleanup is listed in apply-progress, create a subtask for it and verify the fix in the changed file before marking complete. A post-apply grep for known stale comments catches this.

## PICO_BUILD Guards Are a Testability Debt

**Problem**: Binary packet and flash storage assembly code is PICO_BUILD guarded and cannot run in host unit tests. T-TLM-10 captures call-site correctness via stubs but the real storage logic is only exercised on target hardware.

**Lesson**: For future changes, extract pure data-transformation functions (e.g., float-to-int16 ×100 conversion) that are compilable on both host and target. Keep only the HAL/peripheral calls behind PICO_BUILD. This reduces the untested code surface at no firmware cost.

## Design File Paths Drift from Real Paths

**Problem**: The design document referenced `src/services/telemetry/telemetry_storage.c` but the actual path was `src/drivers/payload/telemetry_storage.c`. Implementation found and used the correct path.

**Lesson**: Design documents should reference file paths from a known baseline (e.g., the `git ls-tree` listing at session start) rather than assumed paths. Alternatively, note paths as "illustrative" and state they will be resolved during apply.
