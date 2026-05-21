add_library(fmm_lib
    flight_mode_manager.c
    deploy_monitor.c
)

target_include_directories(fmm_lib
    PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/../../../include
)

target_link_libraries(fmm_lib PUBLIC
    post_lib
    core_lib
    m
)

if(PICO_BUILD)
    target_link_libraries(fmm_lib PUBLIC FreeRTOS-Kernel)
endif()
