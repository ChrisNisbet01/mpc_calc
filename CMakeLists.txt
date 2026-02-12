cmake_minimum_required(VERSION 3.16)
project(plotter_project C)

include(FetchContent)

include(CTest)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_subdirectory(../easy_pc ${CMAKE_CURRENT_BINARY_DIR}/easy_pc_build)
add_subdirectory(examples)

