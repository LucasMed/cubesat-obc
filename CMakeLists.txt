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

# Add main sources
add_subdirectory(src)

# Add examples (only for Pico SDK builds)
if(PICO_BUILD)
    add_subdirectory(examples)
    message(STATUS "Examples will be built")
endif()

# Add tests (only for host mode builds)
if(NOT PICO_BUILD)
    add_subdirectory(tests)
    message(STATUS "Unit tests will be built")
endif()
