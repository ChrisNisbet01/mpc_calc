cmake_minimum_required(VERSION 3.14)
project(mpc_calc C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_FIND_LIBRARY_SUFFIXES ".a" ".so") # Prioritize static libraries

# --- Fetch and build mpc library ---
include(FetchContent)

FetchContent_Declare(
  mpc
  GIT_REPOSITORY https://github.com/orangeduck/mpc.git
  GIT_TAG        master
)
FetchContent_MakeAvailable(mpc)


# Build mpc using its Makefile
add_custom_target(build_mpc
    COMMAND ${CMAKE_COMMAND} -E chdir ${mpc_SOURCE_DIR} make clean
    COMMAND ${CMAKE_COMMAND} -E chdir ${mpc_SOURCE_DIR} make libs
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/include/mpc
    COMMAND ${CMAKE_COMMAND} -E copy ${mpc_SOURCE_DIR}/mpc.h ${CMAKE_BINARY_DIR}/include/mpc/mpc.h
    BYPRODUCTS ${mpc_SOURCE_DIR}/build/libmpc.a
    COMMENT "Building mpc library and copying headers"
)

set(MPC_LIBRARY ${mpc_SOURCE_DIR}/build/libmpc.a)
set(MPC_INCLUDE_DIR ${CMAKE_BINARY_DIR}/include)

# --- Source code ---
add_subdirectory(src)

# --- Unit Tests ---
option(WITH_UNIT_TESTS "Build unit tests" OFF)

if(WITH_UNIT_TESTS)
  message(STATUS "Unit tests will be included in the build")
  enable_language(CXX) # Tests are in C++

  # CppUTest
  set(CPPUTEST_USE_GCOV OFF)
  set(CPPUTEST_USE_MEM_LEAK_DETECTION OFF)
  set(CPPUTEST_USE_VALGRIND OFF)
  set(CPPUTEST_USE_VLD OFF)
  set(CPPUTEST_USE_SANITIZERS OFF)
  set(CPPUTEST_USE_COVERAGE OFF)
  set(CPPUTEST_USE_PCH OFF)
  set(CPPUTEST_USE_MODULES OFF)
  set(CPPUTEST_USE_TDD_MACROS OFF)
  set(CPPUTEST_USE_REAL_GCC_THR OFF)
  set(CPPUTEST_USE_REAL_MINGW_THR OFF)
  set(CPPUTEST_USE_REAL_PTHREAD OFF)
  set(CPPUTEST_USE_REAL_STD_THR OFF)
  set(CPPUTEST_USE_REAL_WIN_THR OFF)
  set(CPPUTEST_USE_EXTENSIONS ON)

  FetchContent_Declare(
    cpputest
    GIT_REPOSITORY https://github.com/cpputest/cpputest.git
    GIT_TAG        v4.0
  )
  FetchContent_MakeAvailable(cpputest)
  add_subdirectory(tests)
endif()
