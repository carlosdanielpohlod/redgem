cmake_minimum_required(VERSION 3.24)
project(redgem VERSION 0.5.0 LANGUAGES C CXX)

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
# The continuous integration turns this on: a warning in the engine's own code (src, tests) fails
# the build. The libraries it is built from keep theirs as warnings.
option(REDGEM_WARNINGS_AS_ERRORS "Treat compiler warnings in the engine's own code as errors" OFF)

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

# Sanitizers, for whole builds (the libraries CMake builds too, so that what they do on the
# engine's threads is seen; mruby, which rake builds, is not instrumented): a comma-separated list
# for -fsanitize, such as "address,undefined" or "thread". The continuous integration runs the
# tests in such builds (see tools/sanitizers/).
set(REDGEM_SANITIZERS "" CACHE STRING "Sanitizers to build everything with (-fsanitize=<list>), or empty")
if(REDGEM_SANITIZERS)
  # (Frames kept, for whole backtraces. What UBSan finds stays recoverable, so that its suppression
  # file can pass over a library's findings; halt_on_error stops the program at any other.)
  add_compile_options(-fsanitize=${REDGEM_SANITIZERS} -fno-omit-frame-pointer)
  add_link_options(-fsanitize=${REDGEM_SANITIZERS})
endif()

# Fuzzing (tests/fuzz): with clang, every library is built for libFuzzer to follow which code an
# input reaches, and the fuzz targets are built as fuzzers. Used with sanitizers, which catch what
# an input does wrong (REDGEM_SANITIZERS=address,undefined).
option(REDGEM_FUZZ "Build the fuzz targets with libFuzzer (clang)" OFF)
if(REDGEM_FUZZ)
  if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(FATAL_ERROR "REDGEM_FUZZ needs clang (libFuzzer): configure with -DCMAKE_CXX_COMPILER=clang++")
  endif()
  # (Their runtime is also what every program built for fuzzing links its coverage counters to.)
  if(NOT REDGEM_SANITIZERS)
    message(FATAL_ERROR "REDGEM_FUZZ needs sanitizers: configure with -DREDGEM_SANITIZERS=address,undefined")
  endif()
  add_compile_options(-fsanitize=fuzzer-no-link)
endif()

include(Dependencies)

# (Set after the dependencies: only the targets made from here on, the engine's, take it.)
set(CMAKE_COMPILE_WARNING_AS_ERROR ${REDGEM_WARNINGS_AS_ERRORS})

add_subdirectory(src)

if(REDGEM_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
