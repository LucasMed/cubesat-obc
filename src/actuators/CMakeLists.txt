add_library(actuators_lib
    reaction_wheel.c
    magnetorquer.c
    pwm_hal.c
)

target_include_directories(actuators_lib PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../include)
