add_library(adcs_lib momentum_dump.c)
target_include_directories(adcs_lib PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(adcs_lib PRIVATE m)
