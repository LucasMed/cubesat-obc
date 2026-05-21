add_library(post_lib
    post.c
)

target_include_directories(post_lib
    PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/../../../include
)

if(PICO_BUILD)
    target_link_libraries(post_lib PUBLIC
        FreeRTOS-Kernel
        core_lib
        log_lib
        m
    )
else()
    target_link_libraries(post_lib PUBLIC
        core_lib
        log_lib
        m
    )
endif()
