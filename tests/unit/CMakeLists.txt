if(NOT PICO_BUILD)
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

    add_executable(test_telemetry
        test_telemetry.c
        ../../src/tasks/telemetry_task.c
        ../../src/core/system_state.c
    )
    target_include_directories(test_telemetry PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../include)
    target_link_libraries(test_telemetry csp)
    add_test(NAME telemetry_test COMMAND test_telemetry)

    add_executable(test_command
        test_command.c
        command_task_test.c
    )
    target_include_directories(test_command PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../../include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/libcsp/include
        ${CMAKE_BINARY_DIR}/third_party/libcsp/include
    )
    # The command test exercises vCommandTask which uses various CSP
    # socket functions.  Link the CSP library so those symbols are
    # satisfied.  Mocks are declared static so they won't collide.
    target_link_libraries(test_command csp)
    add_test(NAME command_test COMMAND test_command)

    add_executable(test_comm_init
        test_comm_init.c
        ../../src/core/comm_init.c
    )
    target_include_directories(test_comm_init PRIVATE 
        ${CMAKE_CURRENT_SOURCE_DIR}/../../include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/libcsp/include
        ${CMAKE_BINARY_DIR}/third_party/libcsp/include
    )
    add_test(NAME comm_init_test COMMAND test_comm_init)

    add_executable(test_tasks
        test_tasks.c
    )
    target_include_directories(test_tasks PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../include)
    target_link_libraries(test_tasks 
        tasks_lib
        control_lib
        dynamics_lib
        drivers_lib
        actuators_lib
        core_lib
    )
    add_test(NAME tasks_test COMMAND test_tasks)

    # PR-1 gate: compile + invariant check for all Foundation Types headers.
    # This test has no runtime dependencies — it exercises only the public
    # header files and verifies enum values, struct sizes, and macro bounds.
    add_executable(test_types
        test_types.c
    )
    target_include_directories(test_types PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../include)
    add_test(NAME types_test COMMAND test_types)
endif()
