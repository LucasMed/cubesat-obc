add_library(log_lib logger.c)

target_include_directories(log_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../include
)

if(PICO_BUILD)
  target_link_libraries(log_lib PUBLIC FreeRTOS-Kernel)
endif()
