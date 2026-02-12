add_library(drivers_lib
    imu/mpu6050.c
)

target_include_directories(drivers_lib PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../include)
