cmake_minimum_required(VERSION 3.13)

# ── Bootloader ────────────────────────────────────────────────────────
# Standalone binary at XIP flash base (0x10000000). Validates CRC32 of
# slot A/B images, selects slot, performs golden restore from W25Q64.
#
# Must fit in 64 KB (BOOTLOADER_SIZE from internal_flash_layout.h).
# Uses Pico SDK hardware_flash and hardware_spi.

add_executable(cubesat_obc_bootloader
    bootloader_asm.S
    bootloader.c
    crc32.c
    spi_flash.c
    ../src/core/boot_log.c
)

target_compile_definitions(cubesat_obc_bootloader PRIVATE
    PICO_BUILD=1
    LIB_FREERTOS_KERNEL=1
    PICO_CRT0_NO_RESET_SECTION=1
)

target_include_directories(cubesat_obc_bootloader PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}  # For crc32.h, spi_flash.h
    ${CMAKE_SOURCE_DIR}/include  # For post.h, flash_layout.h if needed
)

# NOTE: PICO_CRT0_NO_RESET_SECTION=1 prevents pico_crt0's .reset section
# from being linked (we provide our own in bootloader_asm.S).
target_link_libraries(cubesat_obc_bootloader PRIVATE
    hardware_flash
    hardware_spi
    hardware_gpio
    hardware_sync
    pico_stdlib
)

# Set custom linker script for bootloader at 0x10000000
pico_set_linker_script(cubesat_obc_bootloader
    ${CMAKE_SOURCE_DIR}/linker/memmap_bootloader.ld
)

# Generate .uf2 output for flashing
pico_add_extra_outputs(cubesat_obc_bootloader)

message(STATUS "Bootloader target: cubesat_obc_bootloader")

# ── Blink Test ────────────────────────────────────────────────────────
# Minimal smoke test for the bootloader binary layout. If this blinks
# GP0 (pin 1), the custom linker script, boot2, and entry point are
# correct. If the Pico enters mass storage, the layout is broken.
#
# Uses the SAME linker script as the bootloader to test the flash layout.
# Links pico_stdlib minimally for gpio_* and sleep_ms.

# ── Blink Test (custom linker script) ─────────────────────────────────
# Pure-assembly blink test using the SAME layout as the bootloader.
# Validates that the custom linker script produces a bootable RP2350
# binary (boot2 + vector table + entry at 0x10000000).
#
# Uses PICO_CRT0_NO_RESET_SECTION=1 so blink_test.S controls reset.
# Links pico_stdlib for proper --gc-sections, --specs=nosys.specs,
# and boot2 object — but the SDK's crt0 .reset section is excluded.
#
# Output: GP0 (header pin 1) square wave at ~1 Hz.

add_executable(cubesat_obc_blink_test
    blink_test.S
)

target_compile_definitions(cubesat_obc_blink_test PRIVATE
    PICO_BUILD=1
    PICO_CRT0_NO_RESET_SECTION=1
)

target_link_libraries(cubesat_obc_blink_test PRIVATE
    pico_stdlib
)

pico_set_linker_script(cubesat_obc_blink_test
    ${CMAKE_SOURCE_DIR}/linker/memmap_bootloader.ld
)

pico_add_extra_outputs(cubesat_obc_blink_test)

message(STATUS "Blink test target: cubesat_obc_blink_test")

# ── Blink Test (C) ──────────────────────────────────────────────────
# Segunda fuente de verdad: mismo layout, misma config, pero en C.
# Usa el crt0 estandar del SDK (sin PICO_CRT0_NO_RESET_SECTION).
# Si este funciona y la version ASM no, el bug esta en el assembly.
add_executable(cubesat_obc_blink_test_c
    blink_test.c
)

target_compile_definitions(cubesat_obc_blink_test_c PRIVATE
    PICO_BUILD=1
)

target_link_libraries(cubesat_obc_blink_test_c PRIVATE
    pico_stdlib
)

pico_set_linker_script(cubesat_obc_blink_test_c
    ${CMAKE_SOURCE_DIR}/linker/memmap_bootloader.ld
)

pico_add_extra_outputs(cubesat_obc_blink_test_c)

message(STATUS "Blink test C target: cubesat_obc_blink_test_c")

# ── IO Access Test ────────────────────────────────────────────────────
# Determines if IO_BANK0 registers are accessible on RP2350.
# Signals via GP0 voltage:
#   0.42V = BusFault on IO access
#   3.3V  = No BusFault, bus returns 0 (silent fail)
#   0V    = IO_BANK0 accessible (read returned 0x1f)
add_executable(cubesat_obc_io_access_test
    io_access_test.S
)

target_compile_definitions(cubesat_obc_io_access_test PRIVATE
    PICO_BUILD=1
    PICO_CRT0_NO_RESET_SECTION=1
)

target_link_libraries(cubesat_obc_io_access_test PRIVATE
    pico_stdlib
)

pico_set_linker_script(cubesat_obc_io_access_test
    ${CMAKE_SOURCE_DIR}/linker/memmap_bootloader.ld
)

pico_add_extra_outputs(cubesat_obc_io_access_test)

message(STATUS "IO access test target: cubesat_obc_io_access_test")
