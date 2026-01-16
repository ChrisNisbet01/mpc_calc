enable_testing()

set(TEST_SRCS
    AllTests.cpp
    DummyTest.cpp
    FormulaParserTest.cpp
)

add_executable(mpc_calc_tests ${TEST_SRCS})

# Link to the targets created by CppUTest's CMakeLists.txt and our parser lib
target_link_libraries(mpc_calc_tests PRIVATE CppUTest CppUTestExt formula_parser_lib)

add_test(
    NAME RunMpcCalcTests
    COMMAND mpc_calc_tests
)