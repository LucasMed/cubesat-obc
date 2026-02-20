# Integration Tests for OBC

if(NOT PICO_BUILD)
    # Host simulation integration tests
    add_executable(test_i2c_mock
        test_i2c_mpu6050.c
    )
    target_include_directories(test_i2c_mock PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../include)
    target_link_libraries(test_i2c_mock drivers_lib core_lib)
    add_test(NAME i2c_mock_test COMMAND test_i2c_mock)
else()
    # Pico hardware integration tests
    add_executable(test_i2c_hardware
        test_i2c_mpu6050.c
        ${CMAKE_SOURCE_DIR}/src/freertos_hooks.c
    )
    target_link_libraries(test_i2c_hardware
        pico_stdlib
        drivers_lib
        core_lib
        FreeRTOS-Kernel-Heap4
    )
    
    # Enable both USB and UART output for test logging
    pico_enable_stdio_usb(test_i2c_hardware 1)
    pico_enable_stdio_uart(test_i2c_hardware 1)
    pico_add_extra_outputs(test_i2c_hardware)
    
    message(STATUS "Hardware I2C test will be built: build/tests/integration/test_i2c_hardware.uf2")
endif()
