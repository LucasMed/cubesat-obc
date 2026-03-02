add_library(eps_lib eps_monitor.c)

target_include_directories(eps_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../include
)

# EPS monitor drives fault reports and FMM transitions
target_link_libraries(eps_lib PUBLIC fault_lib fmm_lib)

if(PICO_BUILD)
  target_link_libraries(eps_lib PUBLIC FreeRTOS-Kernel)
endif()
