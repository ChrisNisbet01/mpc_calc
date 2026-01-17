cmake_minimum_required(VERSION 3.16)
project(simple_plotter C)

include(FetchContent)

FetchContent_Declare(
    raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG master
)

FetchContent_MakeAvailable(raylib)

FetchContent_Declare(
    raygui
    GIT_REPOSITORY https://github.com/raysan5/raygui.git
    GIT_TAG master
)
FetchContent_GetProperties(raygui)
if(NOT raygui_POPULATED)
    FetchContent_Populate(raygui)
endif()

add_executable(simple_plotter
    main.c
)

target_link_libraries(simple_plotter PRIVATE
    formula_parser_static
    raylib
    m
)

target_include_directories(simple_plotter PRIVATE
    ${raygui_SOURCE_DIR}/src
)
