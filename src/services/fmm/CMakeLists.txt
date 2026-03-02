add_library(fmm_lib
    flight_mode_manager.c
)

target_include_directories(fmm_lib
    PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/../../../include
)

if(PICO_BUILD)
    target_link_libraries(fmm_lib PUBLIC FreeRTOS-Kernel core_lib)
else()
    target_link_libraries(fmm_lib PUBLIC core_lib)
endif()
