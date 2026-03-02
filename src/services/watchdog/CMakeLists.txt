add_library(watchdog_hal_lib watchdog_hal_host.c)
target_include_directories(watchdog_hal_lib PUBLIC ${CMAKE_SOURCE_DIR}/include)
