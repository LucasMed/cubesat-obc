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
        ../../src/core/data_layer.c
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
        fault_lib
        eps_lib
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

    # PR-2 gate: Data Layer Abstraction — read/write round-trip + unit migration.
    add_executable(test_data_layer
        test_data_layer.c
        ../../src/core/data_layer.c
        ../../src/core/system_state.c
    )
    target_include_directories(test_data_layer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../include)
    add_test(NAME data_layer_test COMMAND test_data_layer)

    # PR-3 gate: Flight Mode Manager — state machine + fault-block behaviour.
    # Includes data_layer.c and system_state.c; fault_get_highest_level() is
    # overridden by a strong symbol in test_fmm.c (weak stub in FMM source).
    add_executable(test_fmm
        test_fmm.c
        ../../src/services/fmm/flight_mode_manager.c
        ../../src/core/data_layer.c
        ../../src/core/system_state.c
    )
    target_include_directories(test_fmm PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../include)
    add_test(NAME fmm_test COMMAND test_fmm)

    # PR-4 gate: Fault Manager — table, severity, auto-clear, CRITICAL side-effects.
    # Links real fault_manager + fmm + data_layer so fmm_force_safe() is live.
    add_executable(test_fault_manager
        test_fault_manager.c
        ../../src/services/fault/fault_manager.c
        ../../src/services/fmm/flight_mode_manager.c
        ../../src/core/data_layer.c
        ../../src/core/system_state.c
    )
    target_include_directories(test_fault_manager PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../include)
    add_test(NAME fault_manager_test COMMAND test_fault_manager)

    # PR-5 gate: EPS Monitor — voltage classification, hysteresis, fault reports,
    # rail control.  The test overrides eps_hal_read() with a strong symbol to
    # inject arbitrary voltage values without hardware.
    add_executable(test_eps_monitor
        test_eps_monitor.c
        ../../src/services/eps/eps_monitor.c
        ../../src/services/fault/fault_manager.c
        ../../src/services/fmm/flight_mode_manager.c
        ../../src/core/data_layer.c
        ../../src/core/system_state.c
    )
    target_include_directories(test_eps_monitor PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../include)
    add_test(NAME eps_monitor_test COMMAND test_eps_monitor)

    # PR-6 gate: Persistent Logger — ring buffer, class protection, payload,
    # clear_info, newest-first ordering.  No hardware dependencies.
    add_executable(test_logger
        test_logger.c
        ../../src/services/log/logger.c
    )
    target_include_directories(test_logger PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../include)
    add_test(NAME logger_test COMMAND test_logger)

    # PR-7 gate: Sensor Read Task DLA migration — verify IMU/temp are written to
    # DLA, deg/s→rad/s conversion applied, and driver failures handled.
    # Driver stubs (mpu6050_read_raw, temperature_read) are defined in the test
    # file; drivers_lib is NOT linked to prevent symbol conflicts.
    add_executable(test_sensor_read_task
        test_sensor_read_task.c
        ../../src/tasks/sensor_read_task.c
        ../../src/core/data_layer.c
        ../../src/core/system_state.c
        ../../src/control/ekf.c
    )
    target_include_directories(test_sensor_read_task PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    )
    target_link_libraries(test_sensor_read_task m)
    add_test(NAME sensor_read_task_test COMMAND test_sensor_read_task)

    # PR-8 gate: Attitude Control Task DLA migration — FM guard, imu_valid guard,
    # data passed from DLA to ctrl_update, torque passed to dyn_step.
    # attitude_ctrl_update / attitude_dynamics_step replaced by instrumented stubs.
    add_executable(test_attitude_control_task
        test_attitude_control_task.c
        ../../src/tasks/attitude_control_task.c
        ../../src/core/data_layer.c
        ../../src/core/system_state.c
    )
    target_include_directories(test_attitude_control_task PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    )
    target_link_libraries(test_attitude_control_task m)
    add_test(NAME attitude_control_task_test COMMAND test_attitude_control_task)

    # PR-10 gate: Health Monitor Task — verify fault_manager_tick() and
    # eps_monitor_tick() are both invoked on every Step call.
    # Both service tick functions are replaced by strong-symbol stubs defined
    # in the test file; no service source files are compiled here.
    add_executable(test_health_monitor_task
        test_health_monitor_task.c
        ../../src/tasks/health_monitor_task.c
    )
    target_include_directories(test_health_monitor_task PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    )
    add_test(NAME health_monitor_task_test COMMAND test_health_monitor_task)

    add_executable(test_ekf
        test_ekf.c
        ../../src/control/ekf.c
    )
    target_include_directories(test_ekf PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../include)
    target_link_libraries(test_ekf m)
    add_test(NAME ekf_test COMMAND test_ekf)

    add_executable(test_lqr
        test_lqr.c
        ../../src/control/lqr.c
        ../../src/dynamics/attitude_dynamics.c
    )
    target_include_directories(test_lqr PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../include)
    add_test(NAME lqr_test COMMAND test_lqr)
endif()
