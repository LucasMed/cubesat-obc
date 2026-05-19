# eps_hal.c lives alongside eps_monitor.c here (not in drivers_lib) so the
# strong symbol from eps_hal overrides the weak default within the SAME
# library — no linker-ordering trouble between separate static libs.
add_library(eps_lib eps_monitor.c ../../drivers/eps_hal.c)

target_include_directories(eps_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../include
)

# eps_hal.c includes hardware/adc.h when PICO_BUILD is defined
if(PICO_BUILD)
  target_include_directories(eps_lib PRIVATE
      ${PICO_SDK_PATH}/src/rp2_common/hardware_adc/include
  )
endif()

# EPS monitor drives fault reports and FMM transitions
target_link_libraries(eps_lib PUBLIC fault_lib fmm_lib)

if(PICO_BUILD)
  target_link_libraries(eps_lib PUBLIC FreeRTOS-Kernel)
endif()
