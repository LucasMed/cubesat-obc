add_library(core_lib
    system_state.c
    data_layer.c
    comm_init.c
    event_logger.c
    flash_backend_stub.c
    flash_backend.c
    mpu_init.c
    boot_info.c
    boot_meta.c
    boot_log.c
    fw_upload.c
)

target_include_directories(core_lib PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../config)

if(PICO_BUILD)
    target_link_libraries(core_lib PUBLIC FreeRTOS-Kernel csp pico_stdlib hardware_flash lib_lib)
else()
    target_sources(core_lib PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../control/quaternion.c)
    target_link_libraries(core_lib PUBLIC csp m lib_lib)
endif()
