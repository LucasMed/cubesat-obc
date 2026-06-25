if(NOT PICO_BUILD)
        add_executable(test_rm3100
            test_rm3100.c
            ../../src/drivers/payload/rm3100.c
            ../../src/drivers/payload/spi_payload.c
        )
        target_include_directories(test_rm3100 PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/../../include
            ${CMAKE_CURRENT_SOURCE_DIR}/../../config
            ${CMAKE_SOURCE_DIR}
        )
        add_test(NAME rm3100_test COMMAND test_rm3100)

    add_executable(test_radiation_driver
        test_radiation_driver.c
        ../../src/drivers/payload/radiation_driver.c
    )
    target_include_directories(test_radiation_driver PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../../include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../config
        ${CMAKE_SOURCE_DIR}
    )
    add_test(NAME radiation_driver_test COMMAND test_radiation_driver)

    add_executable(test_camera_driver
        test_camera_driver.c
        ../../src/drivers/payload/camera_driver.c
        ../../src/drivers/payload/spi_payload.c
    )
    target_include_directories(test_camera_driver PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../../include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../config
        ${CMAKE_SOURCE_DIR}
    )
    add_test(NAME camera_driver_test COMMAND test_camera_driver)

    add_executable(test_storage_manager
        test_storage_manager.c
        ../../src/drivers/payload/storage_manager.c
    )
    target_include_directories(test_storage_manager PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../../include
        ${CMAKE_SOURCE_DIR}
    )
    add_test(NAME storage_manager_test COMMAND test_storage_manager)

    add_executable(test_w25q64
        test_w25q64.c
        ../../src/drivers/payload/w25q64.c
        ../../src/drivers/payload/spi_payload.c
    )
    target_include_directories(test_w25q64 PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../../include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../config
        ${CMAKE_SOURCE_DIR}
    )
    add_test(NAME w25q64_test COMMAND test_w25q64)

    # PR-gate: Flash layout — compile-time region assertions.
    # If _Static_assert in flash_layout.h fires, this test does not compile.
    add_executable(test_flash_layout
        test_flash_layout.c
    )
    target_include_directories(test_flash_layout PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    )
    add_test(NAME flash_layout_test COMMAND test_flash_layout)

    # SC-4 gate: imu_calib save/load end-to-end through w25q64 host stubs.
    # Provides its own mpu6050_write_gyro_offset stub; not linked to
    # drivers_lib to avoid symbol conflicts.
    add_executable(test_imu_calib_flash
        test_imu_calib_flash.c
        ../../src/drivers/imu/imu_calib.c
        ../../src/drivers/payload/w25q64.c
        ../../src/drivers/payload/spi_payload.c
    )
    target_include_directories(test_imu_calib_flash PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../../include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../config
        ${CMAKE_SOURCE_DIR}
    )
    add_test(NAME imu_calib_flash_test COMMAND test_imu_calib_flash)

    add_executable(test_payload_manager
        test_payload_manager.c
        ../../src/drivers/payload/payload_manager.c
    )
    target_include_directories(test_payload_manager PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../../include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../config
        ${CMAKE_SOURCE_DIR}/config
    )
    add_test(NAME payload_manager_test COMMAND test_payload_manager)

    add_executable(test_pid
        test_pid.c
        ../../src/control/pid_controller.c
    )
    target_include_directories(test_pid PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../include)
    add_test(NAME pid_test COMMAND test_pid)

    add_executable(test_attitude_control
        test_attitude_control.c
        ../../src/control/attitude_control.c
        ../../src/control/pid_controller.c
    )
    target_include_directories(test_attitude_control PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../include)
    target_link_libraries(test_attitude_control m)
    add_test(NAME attitude_control_test COMMAND test_attitude_control)

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
        ../../src/control/quaternion.c
        ../../src/drivers/payload/telemetry_storage_stub.c
    )
    target_include_directories(test_telemetry PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../include)
    target_link_libraries(test_telemetry wcet_lib eps_lib csp m)
    add_test(NAME telemetry_test COMMAND test_telemetry)

    add_executable(test_command
        test_command.c
        command_task_test.c
        ../../src/drivers/payload/telemetry_storage_stub.c
    )
    target_include_directories(test_command PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../../include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/libcsp/include
        ${CMAKE_BINARY_DIR}/third_party/libcsp/include
    )
    # The command test exercises vCommandTask which uses various CSP
    # socket functions.  Link the CSP library so those symbols are
    # satisfied.  Mocks are declared static so they won't collide.
    target_link_libraries(test_command wcet_lib csp core_lib)
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
        wcet_lib
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
        ../../src/control/quaternion.c
    )
    target_include_directories(test_data_layer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../include)
    target_link_libraries(test_data_layer m)
    add_test(NAME data_layer_test COMMAND test_data_layer)

    add_executable(test_system_state
        test_system_state.c
        ../../src/core/system_state.c
    )
    target_include_directories(test_system_state PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../include)
    target_link_libraries(test_system_state m)
    add_test(NAME system_state_test COMMAND test_system_state)

    # PR-3 gate: Flight Mode Manager — state machine + fault-block behaviour.
    # Includes data_layer.c and system_state.c; fault_get_highest_level() is
    # overridden by a strong symbol in test_fmm.c (weak stub in FMM source).
    add_executable(test_fmm
        test_fmm.c
        ../../src/services/fmm/flight_mode_manager.c
        ../../src/core/data_layer.c
        ../../src/core/system_state.c
        ../../src/control/quaternion.c
    )
    target_include_directories(test_fmm PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../include)
    target_link_libraries(test_fmm m)
    add_test(NAME fmm_test COMMAND test_fmm)

    # POST suite — boot reason names, critical-fail detection, host-run validation
    add_executable(test_post
        test_post.c
        ../../src/services/post/post.c
        ../../src/core/data_layer.c
        ../../src/core/system_state.c
        ../../src/control/quaternion.c
    )
    target_include_directories(test_post PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../include)
    target_link_libraries(test_post m)
    add_test(NAME post_test COMMAND test_post)

    # Deploy Monitor — test includes deploy_monitor.c directly so FreeRTOS
    # mocks (xTaskGetTickCount) apply to the tested logic.
    add_executable(test_deploy_monitor
        test_deploy_monitor.c
        ../../src/services/fmm/flight_mode_manager.c
        ../../src/services/post/post.c
        ../../src/core/data_layer.c
        ../../src/core/system_state.c
        ../../src/control/quaternion.c
    )
    target_include_directories(test_deploy_monitor PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../include)
    target_link_libraries(test_deploy_monitor m)
    add_test(NAME deploy_monitor_test COMMAND test_deploy_monitor)

    # PR-4 gate: Fault Manager — table, severity, auto-clear, CRITICAL side-effects.
    # Links real fault_manager + fmm + data_layer so fmm_force_safe() is live.
    add_executable(test_fault_manager
        test_fault_manager.c
        ../../src/services/fault/fault_manager.c
        ../../src/services/fmm/flight_mode_manager.c
        ../../src/core/data_layer.c
        ../../src/core/system_state.c
        ../../src/control/quaternion.c
    )
    target_include_directories(test_fault_manager PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../include)
    target_link_libraries(test_fault_manager m)
    add_test(NAME fault_manager_test COMMAND test_fault_manager)

    # CDR-SAF-04 / OI-SW-5: Fault injection matrix — all 25 fault IDs, all subsystems,
    # all levels (WARNING/ERROR/CRITICAL), cross-subsystem multi-fault, level precedence.
    add_executable(test_fault_injection
        test_fault_injection.c
        ../../src/services/fault/fault_manager.c
        ../../src/services/fmm/flight_mode_manager.c
        ../../src/core/data_layer.c
        ../../src/core/system_state.c
        ../../src/control/quaternion.c
    )
    target_include_directories(test_fault_injection PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../include)
    target_link_libraries(test_fault_injection m)
    add_test(NAME fault_injection_test COMMAND test_fault_injection)

    add_executable(test_fdir_isr
        test_fdir_isr.c
        ../../src/services/fault/fault_manager.c
        ../../src/core/data_layer.c
        ../../src/control/quaternion.c
    )
    target_include_directories(test_fdir_isr PRIVATE ../../include ../../include/host)
    target_link_libraries(test_fdir_isr m)
    add_test(NAME fdir_isr_test COMMAND test_fdir_isr)

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
        ../../src/control/quaternion.c
    )
    target_include_directories(test_eps_monitor PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../include)
    target_link_libraries(test_eps_monitor m)
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
        ../../src/control/quaternion.c
        ../../src/drivers/mag/mag_calib.c
        ../../src/drivers/env/sht31_stub.c
        ../../src/drivers/rtc/ds3231_stub.c
        ../../src/drivers/sun_sensor_stub.c
    )
    target_include_directories(test_sensor_read_task PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    )
    target_link_libraries(test_sensor_read_task wcet_lib m)
    add_test(NAME sensor_read_task_test COMMAND test_sensor_read_task)

    # PR-8 gate: Attitude Control Task DLA migration — FM guard, imu_valid guard,
    # data passed from DLA to ctrl_update, torque passed to dyn_step.
    # attitude_ctrl_update / attitude_dynamics_step replaced by instrumented stubs.
    add_executable(test_attitude_control_task
        test_attitude_control_task.c
        ../../src/tasks/attitude_control_task.c
        ../../src/core/data_layer.c
        ../../src/core/system_state.c
        ../../src/control/lqr_schedule.c
        ../../src/control/quaternion.c
    )
    target_include_directories(test_attitude_control_task PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    )
    target_link_libraries(test_attitude_control_task wcet_lib m)
    add_test(NAME attitude_control_task_test COMMAND test_attitude_control_task)

    # PR-10 gate: Health Monitor Task — verify fault_manager_tick() and
    # eps_monitor_tick() are both invoked on every Step call.
    # Both service tick functions are replaced by strong-symbol stubs defined
    # in the test file; no service source files are compiled here.
    add_executable(test_health_monitor_task
        test_health_monitor_task.c
        ../../src/tasks/health_monitor_task.c
        ../../src/core/data_layer.c
        ../../src/core/system_state.c
        ../../src/control/quaternion.c
        ../../src/drivers/host_spi_gpio_stubs.c
    )
    target_include_directories(test_health_monitor_task PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    )
    target_link_libraries(test_health_monitor_task wcet_lib m)
    add_test(NAME health_monitor_task_test COMMAND test_health_monitor_task)

    add_executable(test_ekf
        test_ekf.c
        ../../src/control/ekf.c
        ../../src/control/quaternion.c
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

    # PR-16 gate: Watchdog HAL — no-crash, host triggered=false, feed-per-step.
    # test_watchdog.c provides its own watchdog_hal_{init,feed,triggered} stubs
    # (instrumented for call counting) so watchdog_hal_host.c is NOT compiled here;
    # strong symbols in the test file take precedence at link time.
    add_executable(test_watchdog
        test_watchdog.c
        ../../src/tasks/health_monitor_task.c
        ../../src/services/fmm/flight_mode_manager.c
        ../../src/core/data_layer.c
        ../../src/core/system_state.c
        ../../src/control/quaternion.c
        ../../src/drivers/host_spi_gpio_stubs.c
    )
    target_include_directories(test_watchdog PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../include)
    target_link_libraries(test_watchdog wcet_lib m)
    add_test(NAME watchdog_test COMMAND test_watchdog)

    # PR-17 gate: Momentum Dump — init, degenerate B guard, cross-product accuracy,
    # threshold logic, gain scaling.  Pure algorithm; no hardware dependency.
    add_executable(test_momentum_dump
        test_momentum_dump.c
        ../../src/services/adcs/momentum_dump.c
    )
    target_include_directories(test_momentum_dump PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../include)
    target_link_libraries(test_momentum_dump m)
    add_test(NAME momentum_dump_test COMMAND test_momentum_dump)

    # PR-18 gate: HMC5883L magnetometer stub + DLA integration.
    # Compiles the real host stub so T-MAG-01/02 exercise actual code.
    # sensor_read_task.c + data_layer.c + ekf.c pulled in for T-MAG-04;
    # mpu6050 / temperature stubs are defined inside test_hmc5883l.c.
    add_executable(test_hmc5883l
        test_hmc5883l.c
        ../../src/drivers/mag/hmc5883l.c
        ../../src/tasks/sensor_read_task.c
        ../../src/core/data_layer.c
        ../../src/core/system_state.c
        ../../src/control/ekf.c
        ../../src/control/quaternion.c
        ../../src/drivers/env/sht31_stub.c
        ../../src/drivers/rtc/ds3231_stub.c
        ../../src/drivers/sun_sensor_stub.c
        ../../src/drivers/mag/mag_calib.c
    )
    target_include_directories(test_hmc5883l PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../include)
    target_link_libraries(test_hmc5883l wcet_lib m)
    add_test(NAME hmc5883l_test COMMAND test_hmc5883l)

    # PR-19 gate: EKF yaw update via magnetometer.
    # T-EKFM-01..06: no-crash, degenerate field guard, yaw driven to measured
    # value, innovation wrapping at ±π, covariance reduction, repeated
    # predict+update_mag convergence test.
    add_executable(test_ekf_mag
        test_ekf_mag.c
        ../../src/control/ekf.c
        ../../src/control/quaternion.c
    )
    target_include_directories(test_ekf_mag PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../include)
    target_link_libraries(test_ekf_mag m)
    add_test(NAME ekf_mag_test COMMAND test_ekf_mag)

    # BH1750 light sensor driver stub + data layer integration tests.
    # Compiles the real host stub so tests exercise actual code.
    # sensor_read_task.c + data_layer.c pulled in for integration tests.
    add_executable(test_bh1750
        test_bh1750.c
        ../../src/drivers/light/bh1750_stub.c
        ../../src/tasks/sensor_read_task.c
        ../../src/core/data_layer.c
        ../../src/core/system_state.c
        ../../src/control/ekf.c
        ../../src/control/quaternion.c
        ../../src/drivers/rtc/ds3231_stub.c
        ../../src/drivers/sun_sensor_stub.c
        ../../src/drivers/mag/mag_calib.c
    )
    target_include_directories(test_bh1750 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../include)
    target_link_libraries(test_bh1750 wcet_lib m)
    add_test(NAME bh1750_test COMMAND test_bh1750)

    # INA219 Power Monitor Driver Tests
    # T-INA-01..10: Init, is_present, power calculation, data layer integration
    add_executable(test_ina219
        test_ina219.c
        ../../src/drivers/power/ina219_stub.c
        ../../src/tasks/sensor_read_task.c
        ../../src/core/data_layer.c
        ../../src/core/system_state.c
        ../../src/control/ekf.c
        ../../src/control/quaternion.c
        ../../src/drivers/rtc/ds3231_stub.c
        ../../src/drivers/sun_sensor_stub.c
        ../../src/drivers/mag/mag_calib.c
    )
    target_include_directories(test_ina219 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../include)
    target_link_libraries(test_ina219 wcet_lib m)
    add_test(NAME ina219_test COMMAND test_ina219)

    # DS3231 RTC Driver Tests
    # T-DS3231-01..09: Init, is_present, read_time, set_time, epoch conversion, data layer
    add_executable(test_ds3231
        test_ds3231.c
        ../../src/drivers/rtc/ds3231_stub.c
        ../../src/drivers/env/sht31_stub.c
        ../../src/tasks/sensor_read_task.c
        ../../src/core/data_layer.c
        ../../src/core/system_state.c
        ../../src/control/ekf.c
        ../../src/control/quaternion.c
        ../../src/drivers/sun_sensor_stub.c
        ../../src/drivers/mag/mag_calib.c
    )
    target_include_directories(test_ds3231 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../include)
    target_link_libraries(test_ds3231 wcet_lib m)
    add_test(NAME ds3231_test COMMAND test_ds3231)

    # SHT31 Temperature/Humidity Driver Tests
    # T-SHT31-01..11: Init, is_present, constants, heater, data layer integration
    add_executable(test_sht31
        test_sht31.c
        ../../src/drivers/env/sht31_stub.c
        ../../src/tasks/sensor_read_task.c
        ../../src/core/data_layer.c
        ../../src/core/system_state.c
        ../../src/control/ekf.c
        ../../src/control/quaternion.c
        ../../src/drivers/rtc/ds3231_stub.c
        ../../src/drivers/sun_sensor_stub.c
        ../../src/drivers/mag/mag_calib.c
    )
    target_include_directories(test_sht31 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../include)
    target_link_libraries(test_sht31 wcet_lib m)
    add_test(NAME sht31_test COMMAND test_sht31)

    # PR-21 gate: Unit-quaternion utility library.
    # T-QAT-01..05: euler round-trip, q⊗q*=identity, normalize unit norm,
    # rotate_vec 90° yaw/roll, singularity-free at pitch=±90°.
    add_executable(test_quaternion
        test_quaternion.c
        ../../src/control/quaternion.c
    )
    target_include_directories(test_quaternion PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../include)
    target_link_libraries(test_quaternion m)
    add_test(NAME quaternion_test COMMAND test_quaternion)

    # PR-23 gate: LQR gain scheduling — mode-specific gain table.
    # T-LQRS-01: FM_NOMINAL  → default gains (ωn = 10 rad/s)
    # T-LQRS-02: FM_DETUMBLE → high-bandwidth gains (ωn = 30 rad/s)
    # T-LQRS-03: unhandled modes fall back to nominal gain set
    add_executable(test_lqr_schedule
        test_lqr_schedule.c
        ../../src/control/lqr.c
        ../../src/control/lqr_schedule.c
    )
    target_include_directories(test_lqr_schedule PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../include)
    target_link_libraries(test_lqr_schedule m)
    add_test(NAME lqr_schedule_test COMMAND test_lqr_schedule)

    # PR-24 gate: Closed-loop stability harness.
    # T-CLS-01..03: roll/pitch/yaw settle within tolerance in 10 s.
    # T-CLS-04: gyro-bias estimate converges within 5 mrad/s in 20 s.
    # T-CLS-05: torque saturation active and bounded by sqrt(3)·CLS_TAU_SAT.
    # T-CLS-06: RW momentum exceeds dump threshold during FM_DETUMBLE.
    add_executable(test_closed_loop
        test_closed_loop.c
        ../../src/control/ekf.c
        ../../src/control/lqr.c
        ../../src/control/lqr_schedule.c
        ../../src/control/closed_loop_sim.c
        ../../src/dynamics/attitude_dynamics.c
        ../../src/control/quaternion.c
    )
    target_include_directories(test_closed_loop PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../include)
    target_link_libraries(test_closed_loop m)
    add_test(NAME closed_loop_test COMMAND test_closed_loop)

    # PR-26: Event logger flush test (T-LOG-01a..d).
    # flash_backend_stub.c is intentionally excluded so the test can supply
    # its own strong-symbol override of flash_backend_flush().
    add_executable(test_event_logger
        test_event_logger.c
        ../../src/core/event_logger.c
    )
    target_include_directories(test_event_logger PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../include)
    add_test(NAME event_logger_test COMMAND test_event_logger)

    # GPS task unit tests — task-level logic for timeout detection, recovery.
    # Provides its own data_layer_set_gps_fix / fault_report / fault_clear stubs.
    add_executable(test_gps_task
        test_gps_task.c
        ../../src/mocks/gps_stub.c
    )
    target_include_directories(test_gps_task PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../../include
        ${CMAKE_SOURCE_DIR}
    )
    target_link_libraries(test_gps_task wcet_lib m)
    add_test(NAME gps_task_test COMMAND test_gps_task)

    add_executable(test_gps
        test_gps.c
        ../../src/mocks/gps_stub.c
        ../../src/core/data_layer.c
        ../../src/core/system_state.c
        ../../src/control/quaternion.c
    )
    target_link_libraries(test_gps m)
    target_include_directories(test_gps PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../../include
        ${CMAKE_SOURCE_DIR}
    )
    add_test(NAME gps_stub_test COMMAND test_gps)
    add_executable(test_gps_neo7m
        test_gps_neo7m.c
        ../../src/drivers/gps/neo7m.c
    )
    target_include_directories(test_gps_neo7m PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../../include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../config
        ${CMAKE_SOURCE_DIR}
    )
    target_compile_definitions(test_gps_neo7m PRIVATE GPS_TEST)
    add_test(NAME gps_neo7m_test COMMAND test_gps_neo7m)

    add_executable(test_gps_integration
        test_gps_integration.c
        ../../src/drivers/gps/neo7m.c
        ../../src/core/data_layer.c
        ../../src/core/system_state.c
        ../../src/control/quaternion.c
    )
    target_link_libraries(test_gps_integration csp m)
    target_include_directories(test_gps_integration PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../../include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/core
        ${CMAKE_CURRENT_SOURCE_DIR}/../../config
        ${CMAKE_SOURCE_DIR}
    )
    target_compile_definitions(test_gps_integration PRIVATE GPS_TEST)
    add_test(NAME gps_integration_test COMMAND test_gps_integration)

    add_executable(test_imu_integration
        test_imu_integration.c
        ../../src/core/data_layer.c
        ../../src/core/system_state.c
        ../../src/control/quaternion.c
        ../../src/drivers/imu/mpu6050.c
    )
    target_link_libraries(test_imu_integration m)
    target_include_directories(test_imu_integration PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../../include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/core
        ${CMAKE_SOURCE_DIR}
    )
    add_test(NAME imu_integration_test COMMAND test_imu_integration)

    # PR-18 gate: HMC5883L magnetometer stub + DLA integration.
    add_executable(test_mag_integration
        test_mag_integration.c
        ../../src/drivers/mag/hmc5883l.c
        ../../src/core/data_layer.c
        ../../src/core/system_state.c
        ../../src/control/ekf.c
        ../../src/control/quaternion.c
    )
    target_link_libraries(test_mag_integration m)
    target_include_directories(test_mag_integration PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../../include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../include/drivers/mag
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/core
        ${CMAKE_CURRENT_SOURCE_DIR}/../../src/control
        ${CMAKE_SOURCE_DIR}
    )
    add_test(NAME mag_integration_test COMMAND test_mag_integration)

    add_executable(test_radiation_integration
        test_radiation_integration.c
        ../../src/drivers/payload/radiation_driver.c
        ../../src/drivers/payload/payload_manager.c
        ../../src/drivers/payload/storage_manager.c
        ../../src/drivers/payload/rm3100.c
        ../../src/drivers/payload/camera_driver.c
        ../../src/drivers/payload/spi_payload.c
        ../../src/drivers/host_spi_gpio_stubs.c
        ../../src/core/data_layer.c
        ../../src/core/system_state.c
        ../../src/control/quaternion.c
    )
    target_link_libraries(test_radiation_integration m)
    target_include_directories(test_radiation_integration PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../../include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../config
        ${CMAKE_SOURCE_DIR}
    )
    add_test(NAME radiation_integration_test COMMAND test_radiation_integration)

    add_executable(test_pwm_hal
        test_pwm_hal.c
    )
    target_include_directories(test_pwm_hal PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    )
    target_link_libraries(test_pwm_hal m)
    add_test(NAME pwm_hal_test COMMAND test_pwm_hal)

    # CRC8 Checksum Tests
    # T-CRC-01..05: Basic CRC, zeros, ones, string, hex conversion
    add_executable(test_crc8
        test_crc8.c
    )
    target_link_libraries(test_crc8 m)
    add_test(NAME crc8_test COMMAND test_crc8)

    # CRC32 Checksum Tests
    # T-CRC32-01..05: Known vector, empty, single-byte, incremental, consistency
    add_executable(test_crc32
        test_crc32.c
        ../../bootloader/crc32.c
    )
    target_include_directories(test_crc32 PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../../bootloader
    )
    add_test(NAME crc32_test COMMAND test_crc32)

    # Firmware Upload State Machine Tests
    # T-FWU-01..17: State transitions, sequencing, error handling, full cycle
    add_executable(test_fw_upload
        test_fw_upload.c
        ../../src/core/fw_upload.c
    )
    target_include_directories(test_fw_upload PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../../include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/libcsp/include
        ${CMAKE_BINARY_DIR}/third_party/libcsp/include
    )
    target_link_libraries(test_fw_upload csp m)
    add_test(NAME fw_upload_test COMMAND test_fw_upload)

    add_executable(test_sun_sensor
        test_sun_sensor.c
        ../../src/drivers/sun_sensor_stub.c
        ../../src/core/data_layer.c
        ../../src/core/system_state.c
        ../../src/control/quaternion.c
    )
    target_include_directories(test_sun_sensor PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    )
    target_link_libraries(test_sun_sensor wcet_lib m)
    add_test(NAME sun_sensor_test COMMAND test_sun_sensor)

    # IMU calibration core-math unit tests (start -> collect -> finish -> apply).
    # Provides its own mpu6050_write_gyro_offset + w25q64 stubs; no hw deps.
    add_executable(test_imu_calib
        test_imu_calib.c
        ../../src/drivers/imu/imu_calib.c
    )
    target_include_directories(test_imu_calib PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../include)
    target_link_libraries(test_imu_calib m)
    add_test(NAME imu_calib_test COMMAND test_imu_calib)

    # Mag calibration core-math unit tests (start -> collect -> finish -> apply).
    # Pure algorithm, no hardware dependencies.
    add_executable(test_mag_calib
        test_mag_calib.c
        ../../src/drivers/mag/mag_calib.c
    )
    target_include_directories(test_mag_calib PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../include)
    target_link_libraries(test_mag_calib m)
    add_test(NAME mag_calib_test COMMAND test_mag_calib)

    # PR-coverage gate: FatFs disk I/O bridge — status, init, read, write,
    # ioctl, get_fattime.  Test provides its own sd_spi_* mock symbols so
    # drivers_lib (which includes host_sd_spi_stubs.c) is NOT linked.
    add_executable(test_diskio
        test_diskio.c
        ../../src/drivers/payload/diskio.c
    )
    target_include_directories(test_diskio PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../../include
        ${CMAKE_SOURCE_DIR}/third_party/pico-sdk/lib/tinyusb/lib/fatfs/source
    )
    add_test(NAME diskio_test COMMAND test_diskio)

    # PR-coverage gate: EPS HAL host fallback — eps_hal_read() returns
    # fixed values (7.6 V, 0.5 A, 25 °C) on host.  The test compiles
    # eps_hal.c directly so the real function is tested; it does NOT
    # link eps_lib (which contains a weak eps_hal_read in eps_monitor.c).
    add_executable(test_eps_hal
        test_eps_hal.c
        ../../src/drivers/eps_hal.c
    )
    target_include_directories(test_eps_hal PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../include)
    add_test(NAME eps_hal_test COMMAND test_eps_hal)

    # PR-coverage gate: Watchdog HAL host stubs — verify all four
    # no-op functions exist and can be called without crash.
    add_executable(test_watchdog_hal_host
        test_watchdog_hal_host.c
        ../../src/services/watchdog/watchdog_hal_host.c
    )
    target_include_directories(test_watchdog_hal_host PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../include)
    add_test(NAME watchdog_hal_host_test COMMAND test_watchdog_hal_host)

    # PR-coverage gate: SPI payload bus — cs_select and cs_deselect are
    # compiled on host but never exercised by driver tests that provide
    # their own SPI mocks.
    add_executable(test_spi_payload
        test_spi_payload.c
        ../../src/drivers/payload/spi_payload.c
    )
    target_include_directories(test_spi_payload PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../../include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../config
    )
    add_test(NAME spi_payload_test COMMAND test_spi_payload)
endif()
