cmake_minimum_required(VERSION 3.13)

# Only build examples when Pico SDK is available (detected by parent CMakeLists.txt)
# Variable PICO_BUILD is set by parent CMakeLists.txt
if(PICO_BUILD)
    # Blink Test - validates Pico SDK integration
    add_executable(blink_test
        blink_test.c
        ${CMAKE_SOURCE_DIR}/src/freertos_hooks.c
    )
    
    # Link Pico standard library and CYW43 architecture
    # pico_cyw43_arch_none: Controls LED via CYW43 (WiFi chip), no WiFi stack needed
    target_link_libraries(blink_test
        pico_stdlib
        pico_cyw43_arch_none
        FreeRTOS-Kernel
        FreeRTOS-Kernel-Heap4
    )
    
    # Enable both USB and UART output for diagnostics
    pico_enable_stdio_usb(blink_test 1)
    pico_enable_stdio_uart(blink_test 1)
    
    # Create map/bin/hex/uf2 files
    pico_add_extra_outputs(blink_test)
    
    message(STATUS "Blink test will be built: build/examples/blink_test.uf2")
    
    # ========================================================================
    # OBC Firmware with Real FreeRTOS (Phase 2.2)
    # ========================================================================
    add_executable(obc_firmware_pico
        ${CMAKE_SOURCE_DIR}/src/obc_main.c
        ${CMAKE_SOURCE_DIR}/src/freertos_hooks.c
    )

    target_link_libraries(obc_firmware_pico
        pico_stdlib
        pico_cyw43_arch_none
        FreeRTOS-Kernel
        FreeRTOS-Kernel-Heap4
        tasks_lib
        core_lib
        drivers_lib
        actuators_lib
        control_lib
        dynamics_lib
    )

    target_compile_definitions(obc_firmware_pico PRIVATE
        PICO_FLASH_SAFE_EXECUTE_SUPPORT_FREERTOS_SMP=1
        LIB_FREERTOS_KERNEL=1
        PICO_STDIO_USB_SUPPORT_FREERTOS=1
    )

    target_include_directories(obc_firmware_pico PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/config
        $ENV{PICO_SDK_PATH}/lib/FreeRTOS-Kernel/include
        $ENV{PICO_SDK_PATH}/lib/FreeRTOS-Kernel/portable/ThirdParty/GCC/RP2040/include
    )

    pico_enable_stdio_usb(obc_firmware_pico 1)
    pico_enable_stdio_uart(obc_firmware_pico 1)
    pico_add_extra_outputs(obc_firmware_pico)

    message(STATUS "OBC firmware will be built: build/examples/obc_firmware_pico.uf2")
else()
    message(STATUS "Skipping examples (Pico SDK not available)")
endif()
