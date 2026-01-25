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

# Create mpc_lib as a proper CMake target
add_library(mpc_lib STATIC ${mpc_SOURCE_DIR}/mpc.c)
target_include_directories(mpc_lib PUBLIC ${mpc_SOURCE_DIR}) # Make mpc.h available
target_compile_options(mpc_lib PRIVATE -fPIC)

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
