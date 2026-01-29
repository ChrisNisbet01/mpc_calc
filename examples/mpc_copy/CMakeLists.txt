cmake_minimum_required(VERSION 3.16)
project(mpc_copy C)

add_executable(mpc_copy
    main.c
)

target_compile_options(mpc_copy PRIVATE
    -Wall -Wextra
)

target_link_libraries(mpc_copy PUBLIC
    formula_parser_static
    mpc_lib
)
