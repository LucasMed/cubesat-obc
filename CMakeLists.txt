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
        PICO_BUILD=1
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

# Add libcsp (Task 3.1)
if(PICO_BUILD)
    # Force libcsp to use FreeRTOS port
    set(CSP_FREERTOS ON CACHE BOOL "Use FreeRTOS arch" FORCE)
    set(CSP_POSIX OFF CACHE BOOL "Use POSIX arch" FORCE)
    set(CSP_USE_RTABLE ON CACHE BOOL "Use routing table")
    set(CSP_HAVE_STDIO ON CACHE BOOL "OS provides C Standard I/O functions")
    
    # libcsp expects 'freertos_kernel' target name
    if(NOT TARGET freertos_kernel)
        add_library(freertos_kernel INTERFACE)
        target_link_libraries(freertos_kernel INTERFACE FreeRTOS-Kernel)
    endif()
    if(NOT TARGET FreeRTOS-Kernel-Config)
        # Ensure it exists as a target if not already
        add_library(FreeRTOS-Kernel-Config INTERFACE)
    endif()
else()
    set(CSP_POSIX ON CACHE BOOL "Use POSIX arch" FORCE)
    set(CSP_FREERTOS OFF CACHE BOOL "Use FreeRTOS arch" FORCE)
endif()

# Only add libcsp if it exists
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/libcsp/CMakeLists.txt")
    # Disable libcsp's internal unittests and samples to speed up build
    set(CSP_BUILD_SAMPLES OFF CACHE BOOL "Build CSP samples")
    
    add_subdirectory(third_party/libcsp EXCLUDE_FROM_ALL)
    message(STATUS "libcsp integrated from third_party/libcsp")
    
    # After adding the subdirectory, we can customize the 'csp' target
    if(PICO_BUILD)
        # Link libcsp with Pico SDK and FreeRTOS config so it can find pico.h and FreeRTOSConfig.h
        target_link_libraries(csp PRIVATE pico_stdlib FreeRTOS-Kernel-Config)
        # Also ensure it has FreeRTOS kernel includes
        target_link_libraries(csp PRIVATE FreeRTOS-Kernel)
        # Provide compatible endian.h (Task 3.1 fix)
        target_include_directories(csp PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include/pico_compat)
    endif()
else()
    message(WARNING "libcsp not found in third_party/libcsp. Run 'git submodule update --init' if needed.")
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
