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
    
    # =====================================================
    # FreeRTOS (RP2040 / Pico2W)
    # =====================================================

    set(FREERTOS_KERNEL_PATH
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/FreeRTOS-Kernel
    )

    message(STATUS "Using standalone FreeRTOS kernel")

    # Kernel library
    add_library(FreeRTOS-Kernel STATIC
        ${FREERTOS_KERNEL_PATH}/tasks.c
        ${FREERTOS_KERNEL_PATH}/queue.c
        ${FREERTOS_KERNEL_PATH}/list.c
        ${FREERTOS_KERNEL_PATH}/timers.c
        ${FREERTOS_KERNEL_PATH}/event_groups.c
    )

    target_include_directories(FreeRTOS-Kernel PUBLIC
        ${FREERTOS_KERNEL_PATH}/include
        ${FREERTOS_KERNEL_PATH}/portable/ThirdParty/GCC/RP2040
        ${CMAKE_CURRENT_SOURCE_DIR}/config
    )

    # RP2040 port
    target_sources(FreeRTOS-Kernel PRIVATE
        ${FREERTOS_KERNEL_PATH}/portable/ThirdParty/GCC/RP2040/port.c
        ${FREERTOS_KERNEL_PATH}/portable/MemMang/heap_4.c
    )

    target_compile_definitions(FreeRTOS-Kernel PUBLIC
        PICO_FLASH_SAFE_EXECUTE_SUPPORT_FREERTOS_SMP=1
        FREE_RTOS_KERNEL_SMP=1
    )

    target_link_libraries(FreeRTOS-Kernel PUBLIC pico_stdlib)

    add_library(freertos_kernel INTERFACE)
    target_link_libraries(freertos_kernel INTERFACE FreeRTOS-Kernel)

    message(STATUS "FreeRTOS standalone kernel ready")
endif()

# Enable testing support (only for host builds)
if(NOT PICO_BUILD)
    enable_testing()
    add_compile_options(--coverage -O0 -g)
    add_link_options(--coverage)
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
set(CSP_USE_RTABLE ON CACHE BOOL "Use routing table" FORCE)

if(PICO_BUILD)
    # Force libcsp to use FreeRTOS port
    set(CSP_FREERTOS ON CACHE BOOL "Use FreeRTOS arch" FORCE)
    set(CSP_POSIX OFF CACHE BOOL "Use POSIX arch" FORCE)
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
    # Host builds should have standard I/O available so that libcsp
    # provides non-inline rtable helpers and prints.  Previously the value
    # was left unset which caused CSP_HAVE_STDIO to default to 0 in the
    # cache (see generated autoconfig.h), leading to inline stubs that
    # conflicted with our mocks.
    set(CSP_HAVE_STDIO ON CACHE BOOL "OS provides C Standard I/O functions" FORCE)
    # Ensure csp_hex_dump is compiled as a real symbol into libcsp.so;
    # without this the inline stub in csp.h may not be materialised and
    # the linker reports undefined references from within the shared lib.
    set(CSP_ENABLE_CSP_PRINT ON CACHE BOOL "Enable CSP print / hex dump" FORCE)
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
