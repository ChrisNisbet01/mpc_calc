add_executable(mpc_calc_app main.c)
add_dependencies(mpc_calc_app build_mpc)
target_include_directories(mpc_calc_app PRIVATE ${MPC_INCLUDE_DIR})
target_link_libraries(mpc_calc_app PRIVATE ${MPC_LIBRARY})
