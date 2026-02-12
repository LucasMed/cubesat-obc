add_library(dynamics_lib
    attitude_dynamics.c
)

target_include_directories(dynamics_lib PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../include)
