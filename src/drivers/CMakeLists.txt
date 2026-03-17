set(DRIVER_SOURCES
    imu/mpu6050.c
    mag/hmc5883l.c
    payload/rm3100.c
    payload/spi_payload.c
    payload/camera_driver.c
    payload/radiation_driver.c
    payload/storage_manager.c
    payload/payload_manager.c
    gps/neo7m.c
)

# FatFs sources should be present in both embedded and host for static analysis and testability
if(EXISTS "${CMAKE_SOURCE_DIR}/third_party/pico-sdk/lib/tinyusb/lib/fatfs/source/ff.c" AND
   EXISTS "${CMAKE_SOURCE_DIR}/third_party/pico-sdk/lib/tinyusb/lib/fatfs/source/ffsystem.c" AND
   EXISTS "${CMAKE_SOURCE_DIR}/third_party/pico-sdk/lib/tinyusb/lib/fatfs/source/ffunicode.c")
    list(APPEND DRIVER_SOURCES 
        payload/diskio.c
        ${CMAKE_SOURCE_DIR}/third_party/pico-sdk/lib/tinyusb/lib/fatfs/source/ff.c
        ${CMAKE_SOURCE_DIR}/third_party/pico-sdk/lib/tinyusb/lib/fatfs/source/ffsystem.c
        ${CMAKE_SOURCE_DIR}/third_party/pico-sdk/lib/tinyusb/lib/fatfs/source/ffunicode.c
    )
endif()

if(PICO_BUILD)
    list(APPEND DRIVER_SOURCES 
        i2c/pico_i2c.c
        temperature/pico_temp.c
        uart/pico_usart.c
        payload/sd_spi.c
    )
else()
    list(APPEND DRIVER_SOURCES 
        i2c/host_i2c.c
        temperature/host_temp.c
    )

    list(APPEND DRIVER_SOURCES host_spi_gpio_stubs.c)
    list(APPEND DRIVER_SOURCES payload/host_sd_spi_stubs.c)
    # When building on macOS we also need a tiny stub for the CSP USART
    # API.  The real Linux driver is disabled in the third-party library,
    # so without this the host firmware would fail to link.
    if(APPLE)
        list(APPEND DRIVER_SOURCES csp_macos_stub.c)
    endif()
endif()


add_library(drivers_lib ${DRIVER_SOURCES})

target_include_directories(drivers_lib PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../config
    ${CMAKE_SOURCE_DIR}/config
    ${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/libcsp/include
    ${CMAKE_BINARY_DIR}/third_party/libcsp/include
)

target_include_directories(drivers_lib PRIVATE
    ${CMAKE_SOURCE_DIR}/third_party/pico-sdk/lib/tinyusb/lib/fatfs/source
)

if(DEFINED ENV{FATFS_INCLUDE})
    target_include_directories(drivers_lib PRIVATE $ENV{FATFS_INCLUDE})
endif()

if(PICO_BUILD)
    target_include_directories(drivers_lib PRIVATE
        ${PICO_SDK_PATH}/src/rp2_common/hardware_spi/include
    )
endif()


if(PICO_BUILD)
    target_link_libraries(drivers_lib PUBLIC 
        pico_stdlib 
        hardware_i2c 
        hardware_adc
        hardware_spi
        hardware_uart
        FreeRTOS-Kernel
        csp
    )
endif()
