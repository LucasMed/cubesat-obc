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
