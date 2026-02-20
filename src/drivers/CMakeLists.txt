set(DRIVER_SOURCES
    imu/mpu6050.c
)

if(PICO_BUILD)
    list(APPEND DRIVER_SOURCES 
        i2c/pico_i2c.c
        temperature/pico_temp.c
    )
else()
    list(APPEND DRIVER_SOURCES 
        i2c/host_i2c.c
        temperature/host_temp.c
    )
endif()

add_library(drivers_lib ${DRIVER_SOURCES})

target_include_directories(drivers_lib PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../include)

if(PICO_BUILD)
    target_link_libraries(drivers_lib PUBLIC 
        pico_stdlib 
        hardware_i2c 
        hardware_adc
        FreeRTOS-Kernel
    )
endif()
