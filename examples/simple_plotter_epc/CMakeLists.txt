cmake_minimum_required(VERSION 3.16)
project(simple_plotter_epc C)

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

set(FONT_HEADER_PATH "${CMAKE_CURRENT_BINARY_DIR}/font_data.h")

add_custom_command(
    OUTPUT ${FONT_HEADER_PATH}
    COMMAND ./create_font_header iosevka-regular.ttf ${FONT_HEADER_PATH}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/fonts
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/fonts/iosevka-regular.ttf
    COMMENT "Generating font_data.h from iosevka-regular.ttf"
)

add_custom_target(
    generate_font_header_epc ALL
    DEPENDS ${FONT_HEADER_PATH}
)

add_executable(simple_plotter_epc
    main.c
    ast_builder.c
    ast_evaluator.c
    function_definitions.c
    grammar.c
)
# Ensure the generated header is created before simple_plotter_epc is built
add_dependencies(simple_plotter_epc generate_font_header_epc)


target_link_libraries(simple_plotter_epc PRIVATE
    easy_pc
    raylib
    m
)

target_include_directories(simple_plotter_epc PRIVATE
    ${raygui_SOURCE_DIR}/src
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR} # For ast.h, ast_builder.h, ast_evaluator.h, function_definitions.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../easy_pc/include # For easy_mpc.h
)

target_compile_features(simple_plotter_epc PRIVATE c_std_11) # Enable C11 features
