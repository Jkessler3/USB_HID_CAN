cmake_minimum_required(VERSION 3.16)
project(can_core C)

add_library(can_core STATIC
    src/can_core.c
)

target_include_directories(can_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
