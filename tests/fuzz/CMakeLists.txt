# Fuzz targets: the parsers of what comes from outside (scene files, pictures, messages from other
# machines, answers of web servers, the custom properties of a scene's objects), each fed made-up
# inputs by libFuzzer (see fuzz_target.hpp and docs/contributing.md).
#   redgem_fuzz_<name>         the fuzzer, built with REDGEM_FUZZ=ON (clang only)
#   redgem_fuzz_<name>_replay  the same target run on every file of its corpus, in every build: the
#                              seeds (from the tests' fixtures) and every input that once crashed
#                              it stay tested, as the ctest test fuzz.<name>
set(REDGEM_FUZZ_CORPORA ${CMAKE_CURRENT_SOURCE_DIR}/corpus)
# Every fuzzer, to build them alone (run_fuzzers.sh runs them).
if(REDGEM_FUZZ)
  add_custom_target(redgem_fuzz_targets)
endif()

# A program made of the target `name` (fuzz_<name>.cpp) on the engine's library `library`.
function(redgem_fuzz_program program name library)
  add_executable(${program} ${CMAKE_CURRENT_SOURCE_DIR}/fuzz_${name}.cpp ${ARGN})
  target_link_libraries(${program} PRIVATE ${library})
  target_include_directories(${program} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)
  target_compile_options(${program} PRIVATE -Wall -Wextra -Wpedantic -Wno-unused-parameter)
endfunction()

function(redgem_add_fuzz_target name library)
  redgem_fuzz_program(redgem_fuzz_${name}_replay ${name} ${library} ${CMAKE_CURRENT_SOURCE_DIR}/replay_main.cpp)
  add_test(NAME fuzz.${name} COMMAND redgem_fuzz_${name}_replay ${REDGEM_FUZZ_CORPORA}/${name})
  if(REDGEM_FUZZ)
    redgem_fuzz_program(redgem_fuzz_${name} ${name} ${library})
    # (libFuzzer brings the program's main.)
    target_link_options(redgem_fuzz_${name} PRIVATE -fsanitize=fuzzer)
    add_dependencies(redgem_fuzz_targets redgem_fuzz_${name})
  endif()
endfunction()

redgem_add_fuzz_target(gltf redgem_runtime)
redgem_add_fuzz_target(image_header redgem_core)
redgem_add_fuzz_target(wire redgem_net)
redgem_add_fuzz_target(http_response redgem_net)
redgem_add_fuzz_target(json_scanner redgem_runtime)
