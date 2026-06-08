add_library(tasks_lib
    sensor_read_task.c
    attitude_control_task.c
    telemetry_task.c
    health_monitor_task.c
    payload_task.c
    command_task.c
    gps_task.c
)

target_include_directories(tasks_lib PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../config)
target_link_libraries(tasks_lib PRIVATE control_lib dynamics_lib drivers_lib actuators_lib core_lib adcs_lib wcet_lib)

if(PICO_BUILD)
    target_link_libraries(tasks_lib PUBLIC FreeRTOS-Kernel csp hardware_watchdog)
else()
    target_link_libraries(tasks_lib PUBLIC csp watchdog_hal_lib)
endif()
