cmake_minimum_required(VERSION 3.13)

# Only build examples when Pico SDK is available (detected by parent CMakeLists.txt)
# Variable PICO_BUILD is set by parent CMakeLists.txt
if(PICO_BUILD)
    # Blink Test - validates Pico SDK integration
    add_executable(blink_test
        blink_test.c
    )
    
    # Link Pico standard library
    target_link_libraries(blink_test
        pico_stdlib
    )
    
    # Enable USB output, disable UART output (standard Pico practice)
    pico_enable_stdio_usb(blink_test 1)
    pico_enable_stdio_uart(blink_test 0)
    
    # Create map/bin/hex files
    pico_add_extra_outputs(blink_test)
    
    message(STATUS "Blink test will be built: build/examples/blink_test.elf")
else()
    message(STATUS "Skipping examples (Pico SDK not available)")
endif()
