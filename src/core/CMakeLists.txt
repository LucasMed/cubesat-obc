add_library(core_lib
    system_state.c
)

target_include_directories(core_lib PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../include)
