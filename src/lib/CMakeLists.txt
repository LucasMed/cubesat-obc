add_library(lib_lib
    crc32.c
)

target_include_directories(lib_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
)
