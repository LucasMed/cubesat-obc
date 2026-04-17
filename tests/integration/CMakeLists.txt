# Integration Tests for OBC

if(NOT PICO_BUILD)
    # Host simulation integration tests
    add_executable(test_i2c_mock
        test_i2c_mpu6050.c
        host_spi_gpio_stubs.c
    )
    target_include_directories(test_i2c_mock PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../include)
    target_link_libraries(test_i2c_mock drivers_lib core_lib)
    add_test(NAME i2c_mock_test COMMAND test_i2c_mock)

    # PR-25 gate: T-FMS-01 — CRITICAL fault forces FM_SAFE end-to-end.
    # Uses real fault_manager + FMM + data_layer stack; no hardware dependency.
    # T-FMS-01a: CRITICAL → FM_SAFE immediate
    # T-FMS-01b: ERROR    → no mode change
    # T-FMS-01c: WARNING  → no mode change
    # T-FMS-01d: FM_SAFE sticky after fault_clear()
    add_executable(test_fault_safe
        test_fault_safe.c
        ../../src/services/fault/fault_manager.c
        ../../src/services/fmm/flight_mode_manager.c
        ../../src/core/data_layer.c
        ../../src/core/system_state.c
        ../../src/control/quaternion.c
        ../../src/drivers/host_spi_gpio_stubs.c
    )
    target_include_directories(test_fault_safe PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    )
    target_link_libraries(test_fault_safe m)
    add_test(NAME fault_safe_test COMMAND test_fault_safe)

    # PR-25 gate: T-SAFE-01 — watchdog reset detected by health monitor →
    # FAULT_WDT_KICK_MISSED CRITICAL → FM_SAFE autonomous transition.
    # Compiles health_monitor_task.c with a controllable watchdog HAL stub.
    # T-SAFE-01a: triggered=true  → FM_SAFE + fault active
    # T-SAFE-01b: triggered=false → no FM_SAFE (clean boot)
    # T-SAFE-01c: repeated triggers → FM_SAFE sticky, fault count increments
    add_executable(test_safe_trigger
        test_safe_trigger.c
        ../../src/tasks/health_monitor_task.c
        ../../src/services/fault/fault_manager.c
        ../../src/services/fmm/flight_mode_manager.c
        ../../src/core/data_layer.c
        ../../src/core/system_state.c
        ../../src/control/quaternion.c
        ../../src/drivers/host_spi_gpio_stubs.c
    )
    target_include_directories(test_safe_trigger PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    )
    target_link_libraries(test_safe_trigger wcet_lib m)
    add_test(NAME safe_trigger_test COMMAND test_safe_trigger)
    
    # WP-7.8: Payload Integration Test
    add_executable(test_payload_integration
        test_payload_integration.c
        ../../src/tasks/payload_task.c
        ../../src/drivers/payload/payload_manager.c
        ../../src/services/fault/fault_manager.c
        ../../src/services/fmm/flight_mode_manager.c
        ../../src/core/data_layer.c
        ../../src/core/system_state.c
        ../../src/control/quaternion.c
    )
    target_include_directories(test_payload_integration PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    )
    target_link_libraries(test_payload_integration wcet_lib m)
    add_test(NAME payload_integration_test COMMAND test_payload_integration)

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
