add_library(actuators_lib
    reaction_wheel.c
    magnetorquer.c
    pwm_hal.c
)

target_include_directories(actuators_lib PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../include)

# hardware libraries are only available on Pico
if(PICO_BUILD)
    target_link_libraries(actuators_lib PUBLIC hardware_gpio hardware_pwm)
endif()
