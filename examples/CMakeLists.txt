cmake_minimum_required(VERSION 3.13)

# Only build examples when Pico SDK is available (detected by parent CMakeLists.txt)
# Variable PICO_BUILD is set by parent CMakeLists.txt
if(PICO_BUILD)
    # Blink Test - validates Pico SDK integration
    add_executable(blink_test
        blink_test.c
    )
    
    # Link Pico standard library and CYW43 architecture
    # pico_cyw43_arch_none: Controls LED via CYW43 (WiFi chip), no WiFi stack needed
    target_link_libraries(blink_test
        pico_stdlib
        pico_cyw43_arch_none
    )
    
    # Enable USB output, disable UART output (standard Pico practice)
    pico_enable_stdio_usb(blink_test 1)
    pico_enable_stdio_uart(blink_test 0)
    
    # Create map/bin/hex/uf2 files
    pico_add_extra_outputs(blink_test)
    
    message(STATUS "✓ Blink test will be built: build/examples/blink_test.uf2")
    
    # ========================================================================
    # OBC Firmware Stub with Task Structure (Phase 2.2 prep)
    # ========================================================================
    # This demonstrates task structure; full FreeRTOS integration pending
    
    add_executable(obc_firmware_pico
        ../src/obc_main_pico.c
    )
    
    # Link basic Pico libraries (FreeRTOS integration TODO in Phase 2.2)
    target_link_libraries(obc_firmware_pico
        pico_stdlib
        pico_cyw43_arch_none
    )
    
    pico_enable_stdio_usb(obc_firmware_pico 1)
    pico_enable_stdio_uart(obc_firmware_pico 0)
    pico_add_extra_outputs(obc_firmware_pico)
    
    message(STATUS "ℹ OBC firmware created (FreeRTOS real kernel: Phase 2.2 TODO)")
else()
    message(STATUS "Skipping examples (Pico SDK not available)")
endif()
