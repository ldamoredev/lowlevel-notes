cmake_minimum_required(VERSION 3.16)

project(cmake_demo C)

add_library(message STATIC message.c)
target_include_directories(message PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_options(message PRIVATE -Wall -Wextra)

add_executable(demo main.c)
target_link_libraries(demo PRIVATE message)
target_compile_options(demo PRIVATE -Wall -Wextra)
