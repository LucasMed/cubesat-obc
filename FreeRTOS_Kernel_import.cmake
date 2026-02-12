# FreeRTOS Kernel import (simple version for host builds and Pico SDK)
# Usage: include(FreeRTOS_Kernel_import) before project()
# Expects FREERTOS_KERNEL_PATH env var or CMake variable

if(NOT DEFINED FREERTOS_KERNEL_PATH)
    if(DEFINED ENV{FREERTOS_KERNEL_PATH})
        set(FREERTOS_KERNEL_PATH $ENV{FREERTOS_KERNEL_PATH})
    else()
        message(STATUS "FreeRTOS: FREERTOS_KERNEL_PATH not set, assuming bundled or Pico SDK provided")
        set(FREERTOS_KERNEL_PATH "")
    endif()
endif()

if(FREERTOS_KERNEL_PATH)
    add_library(freertos_kernel STATIC
        ${FREERTOS_KERNEL_PATH}/portable/GCC/ARM_CM4F/port.c
        ${FREERTOS_KERNEL_PATH}/portable/MemMang/heap_4.c
        ${FREERTOS_KERNEL_PATH}/croutine.c
        ${FREERTOS_KERNEL_PATH}/event_groups.c
        ${FREERTOS_KERNEL_PATH}/list.c
        ${FREERTOS_KERNEL_PATH}/queue.c
        ${FREERTOS_KERNEL_PATH}/stream_buffer.c
        ${FREERTOS_KERNEL_PATH}/tasks.c
        ${FREERTOS_KERNEL_PATH}/timers.c
    )
    target_include_directories(freertos_kernel PUBLIC
        ${FREERTOS_KERNEL_PATH}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/config
    )
    target_compile_definitions(freertos_kernel PRIVATE
        $<$<COMPILE_LANGUAGE:C>:projCOVERAGE_TEST=0>
    )
else()
    message(STATUS "FreeRTOS: Will use Pico SDK bundled version")
endif()
