enable_testing()

set(TEST_SRCS
    AllTests.cpp
    DummyTest.cpp
    FormulaParserTest.cpp
)

add_executable(mpc_calc_tests ${TEST_SRCS})

target_include_directories(mpc_calc_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_BINARY_DIR}/../include
)

# Link to the targets created by CppUTest's CMakeLists.txt and our parser lib
target_link_libraries(mpc_calc_tests PRIVATE CppUTest CppUTestExt formula_parser_static)

add_test(
    NAME RunMpcCalcTests
    COMMAND mpc_calc_tests
)