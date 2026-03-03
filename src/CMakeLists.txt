cmake_minimum_required(VERSION 3.13)

add_subdirectory(core)
add_subdirectory(drivers)
add_subdirectory(actuators)
add_subdirectory(control)
add_subdirectory(dynamics)
add_subdirectory(tasks)
add_subdirectory(services)

# Main firmware executable
if(NOT PICO_BUILD)
    add_executable(cubesat_obc_firmware
        obc_main.c
    )

    target_include_directories(cubesat_obc_firmware PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../include)

    target_link_libraries(cubesat_obc_firmware
        core_lib
        drivers_lib
        actuators_lib
        control_lib
        dynamics_lib
        tasks_lib
        fault_lib
        eps_lib
    )

    install(TARGETS cubesat_obc_firmware DESTINATION .)
else()
    # ── Pico 2W hardware firmware ────────────────────────────────────────────
    add_executable(cubesat_obc_pico
        obc_main.c
        freertos_hooks.c
    )

    target_compile_definitions(cubesat_obc_pico PRIVATE PICO_BUILD=1)

    target_include_directories(cubesat_obc_pico PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
        ${CMAKE_CURRENT_SOURCE_DIR}/../config
    )

    # csp and drivers_lib have a circular dependency (csp references csp_usart_*
    # defined in pico_usart.c inside drivers_lib).  Wrap them in a linker group
    # so the GNU ld makes multiple passes until all symbols are resolved.
    target_link_libraries(cubesat_obc_pico PRIVATE
        core_lib
        actuators_lib
        control_lib
        dynamics_lib
        tasks_lib
        fault_lib
        eps_lib
        adcs_lib
        watchdog_hal_lib
        FreeRTOS-Kernel
        FreeRTOS-Kernel-Heap4
        pico_stdlib
        pico_cyw43_arch_none
        hardware_i2c
        hardware_adc
        hardware_uart
        hardware_watchdog
        -Wl,--start-group
        csp
        drivers_lib
        -Wl,--end-group
    )

    # Route all printf/puts to USB CDC (primary) + UART0 (secondary)
    pico_enable_stdio_usb(cubesat_obc_pico 1)
    pico_enable_stdio_uart(cubesat_obc_pico 1)

    # Generate .uf2, .bin, .hex, .map for flashing
    pico_add_extra_outputs(cubesat_obc_pico)

    message(STATUS "Pico firmware target: cubesat_obc_pico")
endif()
