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

## Flash Layout Requires Centralized Management

**Problem**: Multiple subsystems (POST, telemetry storage, fault logs, config) write to W25Q64 flash. Without a centralized `flash_layout.h`, sector collisions are inevitable.

**Solution**: Created `include/flash_layout.h` as single source of truth with `static_assert` guards for non-overlapping regions.

## Test Harness Drift Risk

**Problem**: `process_text_command()` is gated behind `#ifdef PICO_BUILD`, so host tests use a reimplementation (`test_run_text_command`) which can drift from production code.

**Lesson**: Future changes should consider making the text command parser compilable on host by guarding only the hardware-specific parts.

---

# Core Module Refactor — Lessons Learned (2026-07-02)

## PICO_BUILD Guards Create a Coverage Blind Spot

**Problem**: `command_task.c` and `telemetry_task.c` are PICO_BUILD-guarded, so gcovr reports 0% coverage on host even though behavioral tests pass through `pico_stubs.h` mock wrappers.

**Lesson**: Structural refactors of PICO_BUILD-guarded code can only be verified through behavioral test execution, not coverage data. Accept this limitation for host testing. The stubs approach (`pico_stubs.h`) is the correct pattern — it allows the real dispatch logic to run on host while the hardware-dependent paths are mocked.

## Approval Testing is the Right Pattern for Pure Refactors

**Problem**: When behavior must be preserved 1:1 (no spec changes), TDD's RED-GREEN-REFACTOR cycle needs adaptation — you can't write a failing test for behavior that already works.

**Solution**: Use the approval testing pattern — write comprehensive regression tests that capture current behavior FIRST, then apply the structural refactor, then verify all tests still pass. This satisfies the intent of RED (tests exist before code change) without requiring a failing test.

## Chained PRs Protect Review Quality for Large Refactors

**Lesson**: Stacking CRC-32 extraction (safe, small, easy) + EKF extraction (small, self-contained) + Telemetry split (structural, no behavior change) into one PR and deferring the command table (615-line if-else → table dispatch, regression-test-heavy) to a separate PR kept each diff under 400 lines and made reviews tractable. Splitting by risk profile (safe refactors first, high-risk last) worked better than splitting by module.

## Pre-Refactor Regression Tests Catch Hidden Bugs

**Discovery**: Writing comprehensive regression tests for the command handler before touching dispatch uncovered two pre-existing bugs — `RESETGPS COLD` (off-by-space in `strncmp(cmd+8, "COLD", 4)`) and `BH1750_TEST5C` (off-by-one in command index). These bugs were documented but intentionally not fixed during the refactor to preserve scope discipline. The tests now serve as regression protection for future fixes.

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

---

# Golden Image MPU — Lessons Learned (2026-06-24)

## RP2350 Cortex-M33: SRAM Must Be Executable (XN=0)

**Problem**: Setting XN=1 (Execute Never) on the SRAM MPU region caused an immediate MemManage fault when the MPU was enabled. This happened at the `msr mpu_ctrl, r0` instruction in `mpu_init()`, before any application code ran.

**Investigation**: Initial theory was a register offset mismatch. The M33 MPU register layout (base + 0xED90) differs from M4 (base + 0xD90), and the SDK provides `mpu_hw_t` with correct ARMv8-M offsets. The offset theory was wrong.

**Root cause**: The Cortex-M33's prefetcher speculatively fetches instructions from the SRAM region, or FreeRTOS SMP internal code (spinlock trampolines, PendSV handler dispatch) resides in SRAM and the processor attempts to execute from it. With XN=1, any instruction fetch from SRAM triggers MemManage.

**Solution**: Set XN=0 on the SRAM region. Accept the security trade-off (SRAM executable) because PRIVDEFENA restricts execution to privileged mode only (the application runs in privileged mode).

**Lesson learned**: RP2350 Cortex-M33 requires SRAM to be executable when MPU is enabled. This is a documented ARMv8-M behavior but is easily missed because the XN bit is the default recommendation for SRAM on other architectures (M4, M7). Always verify MPU regions on real hardware — emulation/stubs do not catch this.

## ADC0 Divider Floats on Dev Board Without Battery

**Problem**: The EPS HealthMonitor entered SAFE mode sporadically on the dev board. The log showed `FAULT_EPS_VBATT_CRITICAL` even though USB power was stable at 4.4V.

**Investigation**: `eps_hal_read()` read GPIO26 (ADC0) through a resistor divider (R1+R2). On the dev board, no battery is connected, so GPIO26 floated. The ADC read a garbage voltage (~3.2V) that fell within the plausibility range (2.5-6.0V) but registered as CRITICAL, triggering `fmm_force_safe()`.

**Solution**: Replaced ADC0 with the INA219 (0x40) bus voltage monitor. The INA219 is:
- Already initialized during POST and read at 100 Hz by the sensor task
- Connected to the system bus (4.4V USB on dev board, battery voltage on flight)
- Accessed via `ina219_get_voltage_mv()` which returns the cached value (no I2C transaction needed)

**Lesson learned**: Do not rely on ADC pins for battery measurement on development boards. If a battery is optional (dev/testing vs flight), use a dedicated power monitor IC (INA219) that works regardless of battery presence. The ADC resistor divider approach is acceptable only when a battery is always connected.

## Bootloader ↔ FSW Communication Protocol Gap

**Problem**: The bootloader writes boot metadata to `BOOT_META_BASE` (0x10221000, flash) and POST codes to `POST_CODE_ADDR` (0x20040000, SRAM). The FSW reads `boot_info_t` from `BOOT_INFO_ADDR` (0x2007FF00, SRAM) via `boot_info_read()`. These are **completely different addresses with different structs** — neither side can read what the other wrote.

**Details**:

| What | Address | Written by | Read by |
|------|---------|-----------|---------|
| `boot_meta_t` | `0x10221000` (flash) | ✅ Bootloader | ❌ Never read by FSW |
| POST code | `0x20040000` (SRAM) | ✅ Bootloader | ❌ Never read by FSW |
| `boot_info_t` | `0x2007FF00` (SRAM) | ❌ Never written | ✅ FSW via `boot_info_read()` |

**Impact**: `CMD_FW_BOOT_INFO` (CSP command 25) and `boot_info_read()` always return invalid data (magic mismatch), because the bootloader never writes to `BOOT_INFO_ADDR`.

**Root cause**: The bootloader and FSW were developed with separate data structures and addresses. The bootloader evolved its own `boot_meta_t` and `post_code()` functions, while the FSW had an independent `boot_info_t` design. No integration review caught the mismatch.

**Fix required**: Define a single `BootStatus_t` struct in SRAM (e.g., at 0x2007FF00) that the bootloader writes and the FSW reads. Include magic, CRC self-validation, active slot, reset cause, and boot count.

## Trust-on-First-Boot Masks Bootloader Execution

**Problem**: When flashing the combined UF2 (bootloader + firmware), the bootloader showed no visible output. The FSW booted as if no bootloader existed.

**Investigation**: The bootloader does not initialize UART or print any messages. It communicates only via SRAM POST codes and flash boot metadata. When no slot metadata exists (first flash), the trust-on-first-boot path validates the vector table (SP in SRAM, PC in flash) and jumps directly — silently.

**Evidence the bootloader ran**:
1. RP2350 BootROM always jumps to the reset vector at flash base (0x10000000)
2. The combined UF2 places the bootloader at 0x10000000 and firmware at 0x10010000
3. Boot metadata was written to flash (trust-on-first-boot path)
4. POST codes were written to SRAM
5. `cleanup_before_jump()` ran (disables SysTick, clears NVIC, forces off core 1)

**Lesson**: Silent bootloaders make debugging difficult. Add `stdio_init_all()` and a brief banner to the bootloader (even if only compiled in DEBUG mode). This costs nothing on flight hardware (UART pins simply not connected).

## Pico 2W W25Q32 is 4 MB, Not 2 MB

**Problem**: `flash_backend.c` hardcoded `FLASH_TOTAL_BYTES = 2 * 1024 * 1024` (2 MB), but the Pico 2W has a W25Q32 (4 MB) flash. The bootloader layout required 4 MB.

**Fix**: Use `PICO_FLASH_SIZE_BYTES` from the board header, with a fallback to 4 MB for host builds. The SDK defines this correctly per board.

## pico_ci.sh Build Stages Need Isolation

**Problem**: Building the combined UF2 requires both the bootloader and firmware to compile with the same cmake build directory. Running `pico-build` then `bootloader-build` sequentially from the same `build_pico_ci` directory works, but running them in parallel or from separate build dirs creates stale or conflicting artifacts.

**Fix**: Split `pico_ci.sh` into explicit stages (`pico-build`, `bootloader-build`) that share a single cmake build dir. The `all` case runs them sequentially. Removed stale test UF2s that were no longer built.
