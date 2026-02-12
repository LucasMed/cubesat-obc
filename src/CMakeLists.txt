cmake_minimum_required(VERSION 3.13)

add_subdirectory(core)
add_subdirectory(drivers)
add_subdirectory(actuators)
add_subdirectory(control)
add_subdirectory(dynamics)
add_subdirectory(tasks)
add_subdirectory(../tests unit_tests)

add_executable(cubesat_obc_firmware
    obc_main.c
)

target_include_directories(cubesat_obc_firmware PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../include)

target_link_libraries(cubesat_obc_firmware
    core_lib
    drivers_lib
    actuators_lib
    control_lib
    dynamics_lib
    tasks_lib
)

install(TARGETS cubesat_obc_firmware DESTINATION .)
