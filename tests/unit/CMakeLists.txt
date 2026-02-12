add_executable(test_pid
    test_pid.c
    ../../src/control/pid_controller.c
)
target_include_directories(test_pid PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../include)
add_test(NAME pid_test COMMAND test_pid)

add_executable(test_dynamics
    test_dynamics.c
    ../../src/dynamics/attitude_dynamics.c
)
target_include_directories(test_dynamics PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../include)
add_test(NAME dynamics_test COMMAND test_dynamics)

add_executable(test_actuators
    test_actuators.c
    ../../src/actuators/reaction_wheel.c
    ../../src/actuators/magnetorquer.c
)
target_include_directories(test_actuators PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../include)
add_test(NAME actuators_test COMMAND test_actuators)
