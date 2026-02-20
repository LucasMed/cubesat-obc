add_library(core_lib
    system_state.c
    comm_init.c
)

target_include_directories(core_lib PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../include)

if(PICO_BUILD)
    target_link_libraries(core_lib PUBLIC FreeRTOS-Kernel csp pico_stdlib)
else()
    target_link_libraries(core_lib PUBLIC csp)
endif()
