cmake_minimum_required(VERSION 3.13)

# Option to enable/disable Pico SDK
option(PICO_ENABLED "Enable Pico SDK compilation" ON)

# Check if Pico SDK is available (only if enabled).
# Accept PICO_SDK_PATH as a CMake cache variable (-DPICO_SDK_PATH=...)
# or as an environment variable (export PICO_SDK_PATH=...).
if(PICO_ENABLED AND NOT DEFINED PICO_SDK_PATH)
    if(DEFINED ENV{PICO_SDK_PATH})
        set(PICO_SDK_PATH $ENV{PICO_SDK_PATH} CACHE PATH "Path to the Pico SDK")
    endif()
endif()

if(PICO_ENABLED AND DEFINED PICO_SDK_PATH AND EXISTS "${PICO_SDK_PATH}/external/pico_sdk_import.cmake")
    message(STATUS "Pico SDK enabled at: ${PICO_SDK_PATH}")
    
    # Set board BEFORE including SDK
    set(PICO_BOARD pico2_w)
    
    # Pull in Pico SDK
    include(${PICO_SDK_PATH}/external/pico_sdk_import.cmake)
    set(PICO_BUILD TRUE)
    # Propagate PICO_BUILD to all C/C++ translation units (libraries + executables).
    # Without this, static libraries compiled before the final link step would
    # not see the #ifdef PICO_BUILD guards in source files.
    add_compile_definitions(PICO_BUILD=1)
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
    message(STATUS "Pico SDK initialized for RP2040/RP2350")

    # =====================================================
    # FreeRTOS (RP2040/RP2350 via official port library.cmake)
    # =====================================================
    set(FREERTOS_KERNEL_PATH
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/FreeRTOS-Kernel
    )
    # Tell the official library.cmake where our FreeRTOSConfig.h lives
    set(FREERTOS_CONFIG_FILE_DIRECTORY
        ${CMAKE_CURRENT_SOURCE_DIR}/config
    )
    message(STATUS "Using FreeRTOS kernel from ${FREERTOS_KERNEL_PATH}")
    # Use the officially sanctioned RP2040 port CMake integration — properly
    # links hardware_exception, hardware_clocks, pico_multicore, etc.
    include(${CMAKE_CURRENT_SOURCE_DIR}/config/library.cmake)

    # freertos_kernel alias expected by libcsp FreeRTOS port
    if(NOT TARGET freertos_kernel)
        add_library(freertos_kernel INTERFACE)
        target_link_libraries(freertos_kernel INTERFACE
            FreeRTOS-Kernel
            FreeRTOS-Kernel-Heap4
        )
    endif()
    message(STATUS "FreeRTOS kernel (RP2040 INTERFACE port) ready")
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
endif()

# Only add libcsp if it exists
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/libcsp/CMakeLists.txt")
    # Ensure Threads::Threads target is available on all platforms (macOS, Linux).
    # libcsp's examples reference Threads::Threads but libcsp only calls
    # find_package(Threads) when building on Linux, causing CMake generation
    # to fail on macOS. We call it here so the target always exists.
    # For Pico (cross-compilation) builds, find_package(Threads) fails because
    # arm-none-eabi does not provide pthreads — create a stub target instead.
    if(PICO_BUILD)
        if(NOT TARGET Threads::Threads)
            add_library(Threads::Threads INTERFACE IMPORTED)
        endif()
    else()
        find_package(Threads REQUIRED)
    endif()
    # Disable libcsp's internal unittests and samples to speed up build
    set(CSP_BUILD_SAMPLES OFF CACHE BOOL "Build CSP samples")
    
    add_subdirectory(third_party/libcsp EXCLUDE_FROM_ALL)
    message(STATUS "libcsp integrated from third_party/libcsp")

    # Fix Apple Clang incompatibilities in libcsp (third-party code).
    # libcsp's 'csp_common' INTERFACE target injects -Werror / -Wpedantic
    # which cause build failures on macOS for GNU extensions used in libcsp.
    # csp_macro.h has been patched for __noinit and __weak, but we still need
    # to suppress ##__VA_ARGS__ warnings that -Wpedantic + -Werror rejects.
    # Strategy: clear csp_common's INTERFACE_COMPILE_OPTIONS (the source of
    # -Werror/-Wpedantic) and add back only the warnings we want.
    if(APPLE)
        # macOS has stricter warnings when compiling libcsp.  We keep
        # the Ethernet driver disabled on the host since it depends on
        # linux-specific APIs, but leave the USART code enabled so that
        # the host firmware executable and unit tests can link.  The
        # drivers are mocked in tests when necessary.
        target_compile_definitions(csp PRIVATE
            CSP_DISABLE_ETHERNET=1
            # keep USART enabled for host builds
        )
        set_property(TARGET csp_common PROPERTY INTERFACE_COMPILE_OPTIONS
            -Wall -Wextra -Wcast-align -Wmissing-prototypes
            -Wpointer-arith -Wshadow -Wwrite-strings
            -Wno-gnu-zero-variadic-macro-arguments
            -include ${CMAKE_CURRENT_SOURCE_DIR}/include/pico_compat/csp_macos_compat.h
        )
        target_include_directories(csp PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include/pico_compat
        )
        message(STATUS "Applied macOS compatibility patches to libcsp")
    endif()

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
