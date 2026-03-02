add_library(fault_lib
    fault_manager.c
)

target_include_directories(fault_lib
    PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/../../../include
)

if(PICO_BUILD)
    target_link_libraries(fault_lib PUBLIC FreeRTOS-Kernel fmm_lib)
else()
    target_link_libraries(fault_lib PUBLIC fmm_lib)
endif()
