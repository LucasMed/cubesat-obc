add_library(control_lib
    pid_controller.c
    attitude_control.c
    ekf.c
    lqr.c
)

target_include_directories(control_lib PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../include)
target_link_libraries(control_lib actuators_lib m)
