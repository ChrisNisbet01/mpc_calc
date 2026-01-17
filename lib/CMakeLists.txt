cmake_minimum_required(VERSION 3.16)
project(formula_parser C)

include(FetchContent)

FetchContent_Declare(
    mpc
    GIT_REPOSITORY https://github.com/orangeduck/mpc.git
    GIT_TAG master
)
FetchContent_GetProperties(mpc)
if(NOT mpc_POPULATED)
    FetchContent_Populate(mpc)
endif()

add_custom_target(build_mpc
    COMMAND ${CMAKE_COMMAND} -E chdir ${mpc_SOURCE_DIR} make clean
    COMMAND ${CMAKE_COMMAND} -E chdir ${mpc_SOURCE_DIR} make CFLAGS=-fPIC libs
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/include/mpc
    COMMAND ${CMAKE_COMMAND} -E copy ${mpc_SOURCE_DIR}/mpc.h ${CMAKE_BINARY_DIR}/include/mpc/mpc.h
    BYPRODUCTS ${mpc_SOURCE_DIR}/build/libmpc.a
    COMMENT "Building mpc library and copying headers"
)

add_library(formula_parser_static STATIC
    formula_parser.c
)

add_dependencies(formula_parser_static build_mpc)

target_include_directories(formula_parser_static PUBLIC
    $<INSTALL_INTERFACE:include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
    ${CMAKE_BINARY_DIR}/include
)

target_link_libraries(formula_parser_static PUBLIC
    ${mpc_SOURCE_DIR}/build/libmpc.a
)

add_library(formula_parser_shared SHARED
    formula_parser.c
)

add_dependencies(formula_parser_shared build_mpc)

target_include_directories(formula_parser_shared PUBLIC
    $<INSTALL_INTERFACE:include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
    ${CMAKE_BINARY_DIR}/include
)

target_link_libraries(formula_parser_shared PUBLIC
    ${mpc_SOURCE_DIR}/build/libmpc.a
)

install(TARGETS formula_parser_static formula_parser_shared
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

install(
    DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../include/
    DESTINATION include
)
