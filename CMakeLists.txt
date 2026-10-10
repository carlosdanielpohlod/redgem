cmake_minimum_required(VERSION 3.24)
project(redgem VERSION 0.1.0 LANGUAGES C CXX)

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

include(Dependencies)

add_subdirectory(src)

if(REDGEM_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
