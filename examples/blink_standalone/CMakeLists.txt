cmake_minimum_required(VERSION 3.13)

# ── SDK path resolution (same logic as workspace CMakeLists.txt) ─────────────
if(NOT DEFINED PICO_SDK_PATH)
    if(DEFINED ENV{PICO_SDK_PATH})
        set(PICO_SDK_PATH $ENV{PICO_SDK_PATH})
    else()
        get_filename_component(PICO_SDK_PATH
            "${CMAKE_SOURCE_DIR}/../../third_party/pico-sdk" ABSOLUTE)
    endif()
endif()
message(STATUS "Pico SDK: ${PICO_SDK_PATH}")
set(PICO_BOARD pico2_w)
include(${PICO_SDK_PATH}/external/pico_sdk_import.cmake)

# ── FreeRTOS (same approach as workspace CMakeLists.txt) ─────────────────────
set(FREERTOS_KERNEL_PATH
    "${CMAKE_SOURCE_DIR}/../../third_party/FreeRTOS-Kernel"
    CACHE PATH "FreeRTOS kernel root")
set(FREERTOS_CONFIG_FILE_DIRECTORY
    "${CMAKE_SOURCE_DIR}/../../config"
    CACHE PATH "Directory with FreeRTOSConfig.h")
include(${FREERTOS_KERNEL_PATH}/portable/ThirdParty/GCC/RP2040/library.cmake)

project(blink_standalone C CXX ASM)
set(CMAKE_C_STANDARD   11)
set(CMAKE_CXX_STANDARD 17)
set(PICO_BUILD TRUE)

pico_sdk_init()

# freertos_kernel alias required by libcsp
if(NOT TARGET freertos_kernel)
    add_library(freertos_kernel INTERFACE)
    target_link_libraries(freertos_kernel INTERFACE FreeRTOS-Kernel)
endif()
if(NOT TARGET FreeRTOS-Kernel-Config)
    add_library(FreeRTOS-Kernel-Config INTERFACE)
endif()

# ── libcsp (identical setup to workspace CMakeLists.txt) ─────────────────────
set(CSP_USE_RTABLE   ON  CACHE BOOL "" FORCE)
set(CSP_FREERTOS     ON  CACHE BOOL "" FORCE)
set(CSP_POSIX        OFF CACHE BOOL "" FORCE)
set(CSP_HAVE_STDIO   ON  CACHE BOOL "" FORCE)
set(CSP_BUILD_SAMPLES OFF CACHE BOOL "")

if(NOT TARGET Threads::Threads)
    add_library(Threads::Threads INTERFACE IMPORTED)
endif()

add_subdirectory(${CMAKE_SOURCE_DIR}/../../third_party/libcsp
                 ${CMAKE_BINARY_DIR}/libcsp EXCLUDE_FROM_ALL)

target_link_libraries(csp PRIVATE
    pico_stdlib FreeRTOS-Kernel-Config FreeRTOS-Kernel)
target_include_directories(csp PRIVATE
    ${CMAKE_SOURCE_DIR}/../../include/pico_compat)

# ── OBC source libraries (reuse workspace src subdirectories) ─────────────
set(OBC_SRC "${CMAKE_SOURCE_DIR}/../../src")

add_subdirectory(${OBC_SRC}/core          ${CMAKE_BINARY_DIR}/src/core)
add_subdirectory(${OBC_SRC}/drivers       ${CMAKE_BINARY_DIR}/src/drivers)
add_subdirectory(${OBC_SRC}/actuators     ${CMAKE_BINARY_DIR}/src/actuators)
add_subdirectory(${OBC_SRC}/control       ${CMAKE_BINARY_DIR}/src/control)
add_subdirectory(${OBC_SRC}/dynamics      ${CMAKE_BINARY_DIR}/src/dynamics)
add_subdirectory(${OBC_SRC}/services      ${CMAKE_BINARY_DIR}/src/services)
add_subdirectory(${OBC_SRC}/tasks         ${CMAKE_BINARY_DIR}/src/tasks)

# ── Executable ───────────────────────────────────────────────────────────────
add_executable(blink_pico2w
    blink.c
    ${OBC_SRC}/freertos_hooks.c
)

target_compile_definitions(blink_pico2w PRIVATE
    PICO_BUILD=1
    LIB_FREERTOS_KERNEL=1
    PICO_FLASH_SAFE_EXECUTE_SUPPORT_FREERTOS_SMP=1
)

target_include_directories(blink_pico2w PRIVATE
    ${CMAKE_SOURCE_DIR}/../../include
    ${CMAKE_SOURCE_DIR}/../../config
)

# Identical link order to cubesat_obc_pico in src/CMakeLists.txt
target_link_libraries(blink_pico2w PRIVATE
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

pico_enable_stdio_usb(blink_pico2w  1)
pico_enable_stdio_uart(blink_pico2w 1)
pico_add_extra_outputs(blink_pico2w)
