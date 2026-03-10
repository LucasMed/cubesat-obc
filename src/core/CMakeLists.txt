add_library(core_lib
    system_state.c
    data_layer.c
    comm_init.c
    event_logger.c
    flash_backend_stub.c
    flash_backend.c
)

target_include_directories(core_lib PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../include)

if(PICO_BUILD)
    target_link_libraries(core_lib PUBLIC FreeRTOS-Kernel csp pico_stdlib hardware_flash)
else()
    target_link_libraries(core_lib PUBLIC csp)
endif()
