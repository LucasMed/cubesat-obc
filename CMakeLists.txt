cmake_minimum_required(VERSION 3.13)
project(cubesat_obc C CXX ASM)

# Enable testing support
enable_testing()

# Expect user to set PICO_SDK_PATH externally when building for Pico
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -pedantic")

add_subdirectory(src)
