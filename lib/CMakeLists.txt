cmake_minimum_required(VERSION 3.16)
project(formula_parser C)



add_library(formula_parser_static STATIC
    formula_parser.c
)
target_compile_options(formula_parser_static PRIVATE
    -Wall -Wextra
)

add_dependencies(formula_parser_static mpc_lib)

target_include_directories(formula_parser_static PUBLIC
    $<INSTALL_INTERFACE:include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
    ${CMAKE_BINARY_DIR}/include
)

target_link_libraries(formula_parser_static PUBLIC
    mpc_lib
)

add_library(formula_parser_shared SHARED
    formula_parser.c
)
target_compile_options(formula_parser_shared PRIVATE
    -Wall -Wextra
)

add_dependencies(formula_parser_shared mpc_lib)

target_include_directories(formula_parser_shared PUBLIC
    $<INSTALL_INTERFACE:include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
    ${CMAKE_BINARY_DIR}/include
)

target_link_libraries(formula_parser_shared PUBLIC
    mpc_lib
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
