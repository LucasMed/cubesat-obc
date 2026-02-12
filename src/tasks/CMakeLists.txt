add_library(tasks_lib
    sensor_read_task.c
    attitude_control_task.c
    telemetry_task.c
    health_monitor_task.c
)

target_include_directories(tasks_lib PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../include)
target_link_libraries(tasks_lib control_lib dynamics_lib drivers_lib actuators_lib)
