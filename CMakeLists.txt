cmake_minimum_required(VERSION 3.24)
project(redgem VERSION 0.2.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
# Some pinned dependencies still declare very old minimum CMake versions.
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

option(REDGEM_BUILD_TESTS "Build the test suites" ON)

# The program is position independent (see cmake/Hardening.cmake), which every library linked
# into it has to be as well. Most Linux toolchains build that way by themselves; where one does
# not, everything is asked to.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND NOT DEFINED CMAKE_POSITION_INDEPENDENT_CODE)
  include(CheckCXXSourceCompiles)
  check_cxx_source_compiles("
#if !defined(__PIC__) && !defined(__PIE__)
#error not position independent by default
#endif
int main() { return 0; }" REDGEM_POSITION_INDEPENDENT_BY_DEFAULT)
  if(NOT REDGEM_POSITION_INDEPENDENT_BY_DEFAULT)
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
  endif()
endif()

include(Dependencies)

add_subdirectory(src)

if(REDGEM_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
