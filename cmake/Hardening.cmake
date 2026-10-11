# Protections that keep a memory mistake from becoming something worse, for the engine's own
# code only: the libraries it is built from keep the flags their authors chose. Everything is
# tried on the toolchain first, so each system gets what its compiler and linker really take.
#   redgem_harden(<target>)          how the target's sources are compiled
#   redgem_harden_program(<target>)  how the program (or, on Android, the library) is linked
include(CheckCXXCompilerFlag)
include(CheckCXXSourceCompiles)
include(CheckLinkerFlag)
include(CheckPIESupported)
include(CMakePushCheckState)

# A small program that copies text of a length only known when it runs: what the stack protector
# guards and what the fortified C functions check.
set(REDGEM_HARDENING_PROBE "
#include <cstdio>
#include <cstring>
int main(int argc, char** argv) {
  char text[64];
  std::snprintf(text, sizeof(text), \"%s\", argv[0]);
  std::memcpy(text, argv[0], static_cast<unsigned>(argc));
  return text[0] != 0 ? 0 : 1;
}")

# A guard value on the stack of functions that hold buffers. Tried as a whole program: some
# toolchains compile it and then miss the library that goes with it.
cmake_push_check_state(RESET)
set(CMAKE_REQUIRED_FLAGS "-fstack-protector-strong")
check_cxx_source_compiles("${REDGEM_HARDENING_PROBE}" REDGEM_HAS_STACK_PROTECTOR)
cmake_pop_check_state()

# Checked versions of memcpy, sprintf and the like. Several toolchains turn them on by
# themselves (some at a stricter level than 2): theirs is left alone.
cmake_push_check_state(RESET)
set(CMAKE_REQUIRED_FLAGS "-O2")
check_cxx_source_compiles("
#ifdef _FORTIFY_SOURCE
#error the toolchain defines it already
#endif
int main() { return 0; }" REDGEM_FORTIFY_NOT_PRESET)
set(CMAKE_REQUIRED_FLAGS "-O2 -D_FORTIFY_SOURCE=2")
check_cxx_source_compiles("${REDGEM_HARDENING_PROBE}" REDGEM_FORTIFY_WORKS)
cmake_pop_check_state()

check_cxx_compiler_flag(-fPIE REDGEM_HAS_PIE_FLAG)
# Tables of addresses made read-only once the program is loaded, and a stack no code runs from.
check_linker_flag(CXX "-Wl,-z,relro,-z,now" REDGEM_LINKS_RELRO_NOW)
check_linker_flag(CXX "-Wl,-z,noexecstack" REDGEM_LINKS_NOEXECSTACK)
if(WIN32)
  # Loaded at a different address every run (anywhere in 64 bits), with no code run from data.
  check_linker_flag(CXX "-Wl,--dynamicbase,--nxcompat,--high-entropy-va" REDGEM_LINKS_WINDOWS_MITIGATIONS)
endif()

function(redgem_harden target)
  if(REDGEM_HAS_STACK_PROTECTOR)
    target_compile_options(${target} PRIVATE -fstack-protector-strong)
    if(WIN32)
      # (Where the guard lives in a library of its own, this is what links it in.)
      target_link_options(${target} PUBLIC -fstack-protector-strong)
    endif()
  endif()
  # Only in optimized builds: without optimization the checks cannot be made, and the C library says so.
  if(REDGEM_FORTIFY_NOT_PRESET AND REDGEM_FORTIFY_WORKS)
    target_compile_definitions(${target} PRIVATE
      $<$<OR:$<CONFIG:Release>,$<CONFIG:RelWithDebInfo>,$<CONFIG:MinSizeRel>>:_FORTIFY_SOURCE=2>)
  endif()
  if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    # The standard library's containers check their indexes (libstdc++; other libraries ignore it).
    target_compile_definitions(${target} PRIVATE _GLIBCXX_ASSERTIONS)
    # Code that runs wherever it is loaded, as a position independent program needs.
    if(REDGEM_HAS_PIE_FLAG)
      target_compile_options(${target} PRIVATE -fPIE)
    endif()
  endif()
endfunction()

function(redgem_harden_program target)
  if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    # Loaded at a different address every run. (Android's library is position independent by nature.)
    check_pie_supported(OUTPUT_VARIABLE pie_report LANGUAGES CXX)
    if(CMAKE_CXX_LINK_PIE_SUPPORTED)
      set_target_properties(${target} PROPERTIES POSITION_INDEPENDENT_CODE ON)
    else()
      message(STATUS "redgem: no position independent executable with this toolchain: ${pie_report}")
    endif()
  endif()
  if(REDGEM_LINKS_RELRO_NOW AND NOT WIN32)
    target_link_options(${target} PRIVATE "-Wl,-z,relro,-z,now")
  endif()
  if(REDGEM_LINKS_NOEXECSTACK AND NOT WIN32)
    target_link_options(${target} PRIVATE "-Wl,-z,noexecstack")
  endif()
  if(WIN32 AND REDGEM_LINKS_WINDOWS_MITIGATIONS)
    target_link_options(${target} PRIVATE "-Wl,--dynamicbase,--nxcompat,--high-entropy-va")
  endif()
endfunction()
