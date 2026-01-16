add_executable(mpc_calc_app main.c)
add_dependencies(mpc_calc_app build_mpc)
target_include_directories(mpc_calc_app PRIVATE ${MPC_INCLUDE_DIR} ${raygui_SOURCE_DIR}/src)
target_link_libraries(mpc_calc_app PRIVATE ${MPC_LIBRARY} formula_parser_lib raylib)

# Add the new parser as a separate library for testing
add_library(formula_parser_lib STATIC formula_parser.c)
target_include_directories(formula_parser_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
add_dependencies(formula_parser_lib build_mpc)
target_link_libraries(formula_parser_lib PRIVATE ${MPC_LIBRARY} m)
target_include_directories(formula_parser_lib PRIVATE ${MPC_INCLUDE_DIR}) # mpc.h is needed by parser
