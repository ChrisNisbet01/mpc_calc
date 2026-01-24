cmake_minimum_required(VERSION 3.16)
project(mpc_calc C)

include(CTest)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_subdirectory(lib)
add_subdirectory(examples)

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
  set(CPPUTEST_MEM_LEAK_DETECTION_DISABLED OFF CACHE BOOL "" FORCE)

  FetchContent_Declare(
    cpputest
    GIT_REPOSITORY https://github.com/cpputest/cpputest.git
    GIT_TAG        v4.0
  )
  FetchContent_MakeAvailable(cpputest)
  add_subdirectory(tests)
endif()
