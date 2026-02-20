cmake_minimum_required(VERSION 3.13)

# Option to enable/disable Pico SDK
option(PICO_ENABLED "Enable Pico SDK compilation" ON)

# Check if Pico SDK is available (only if enabled)
if(PICO_ENABLED AND DEFINED ENV{PICO_SDK_PATH})
    set(PICO_SDK_PATH $ENV{PICO_SDK_PATH})
    message(STATUS "Pico SDK enabled at: ${PICO_SDK_PATH}")
    
    # Set board BEFORE including SDK
    set(PICO_BOARD pico2_w)
    
    # Pull in Pico SDK
    include(${PICO_SDK_PATH}/external/pico_sdk_import.cmake)
    set(PICO_BUILD TRUE)
else()
    set(PICO_BUILD FALSE)
    if(NOT PICO_ENABLED)
        message(STATUS "Pico SDK disabled (PICO_ENABLED=OFF). Building for host.")
    else()
        message(STATUS "Pico SDK not found. Building for host.")
    endif()
endif()

project(cubesat_obc C CXX ASM)

# Initialize Pico SDK if available
if(PICO_BUILD)
    pico_sdk_init()
    message(STATUS "Pico SDK initialized for RP2040")
    
    # Set FreeRTOS kernel path
    set(FREERTOS_KERNEL_PATH ${PICO_SDK_PATH}/lib/FreeRTOS-Kernel)
    message(STATUS "FreeRTOS Kernel Path: ${FREERTOS_KERNEL_PATH}")
    
    # Create FreeRTOS config target (required before including FreeRTOS CMake)
    # This tells FreeRTOS where to find the FreeRTOSConfig.h file
    add_library(freertos_config INTERFACE)
    target_include_directories(freertos_config SYSTEM
        INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/config
    )
    target_compile_definitions(freertos_config
        INTERFACE
        projCOVERAGE_TEST=0
    )
    # Also add the config to all compiled target interface for SDK config headers
    target_include_directories(freertos_config SYSTEM INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/config
    )
    message(STATUS "FreeRTOS config target created (includes ./config/FreeRTOSConfig.h)")
    
    # Global definitions to fix SDK components (like pico_flash) when using FreeRTOS SMP
    add_compile_definitions(
        PICO_FLASH_SAFE_EXECUTE_SUPPORT_FREERTOS_SMP=1
        LIB_FREERTOS_KERNEL=1
        FREE_RTOS_KERNEL_SMP=1
        configUSE_EVENT_GROUPS=1
        configUSE_TIMERS=1
        INCLUDE_xEventGroupSetBitsFromISR=1
        INCLUDE_xTimerPendFunctionCall=1
    )

    # Force software floating point because the RP2040 FreeRTOS port
    # does not handle FPU context switching on RP2350.
    add_compile_options(-mfloat-abi=soft)

    # Include FreeRTOS kernel port for RP2040 (works with RP2350 also)
    set(FREERTOS_CONFIG_FILE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/config)
    set(FREERTOS_PORT_BINARY_PATH "${CMAKE_BINARY_DIR}/freertos_kernel_rp2040")
    add_subdirectory(
        ${FREERTOS_KERNEL_PATH}/portable/ThirdParty/GCC/RP2040
        ${FREERTOS_PORT_BINARY_PATH}
        EXCLUDE_FROM_ALL
    )
    # Ensure the kernel picks up our config
    target_link_libraries(FreeRTOS-Kernel INTERFACE freertos_config)
    
    message(STATUS "FreeRTOS Kernel integrated (RP2040 port for Pico 2W)")
endif()

# Enable testing support (only for host builds)
if(NOT PICO_BUILD)
    enable_testing()
endif()

# C standard and compiler flags
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -pedantic")

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# Add host-specific stubs only for non-Pico builds
if(NOT PICO_BUILD)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include/host)
endif()

# Add main sources
add_subdirectory(src)

# Add examples (only for Pico SDK builds)
if(PICO_BUILD)
    add_subdirectory(examples)
    message(STATUS "Examples will be built")
endif()

# Add tests (unit for host, integration for both)
add_subdirectory(tests)
message(STATUS "Tests will be built")
