# What games are tested with. None of this is part of the engine, of the program games ship as
# (redgem-player) or of the one they are made with (redgem): neither target must ever link to
# redgem_testing (tests/ships_no_test_code.sh checks the programs that came out).

# The Ruby that tests are written with (order matters: the system test DSL uses the assertions,
# and what a test does at the controls is added to the class the DSL defines).
set(REDGEM_TESTING_RUBY_SOURCES
  ${CMAKE_SOURCE_DIR}/ruby/testing/test_case.rb
  ${CMAKE_SOURCE_DIR}/ruby/testing/system_test.rb
  ${CMAKE_SOURCE_DIR}/ruby/testing/system_test_controls.rb)
# A folder of its own, which only this library is given: the engine cannot include it by accident.
set(REDGEM_TESTING_GENERATED_DIR ${CMAKE_BINARY_DIR}/generated_testing_include)
set(REDGEM_TESTING_RUBY_HEADER ${REDGEM_TESTING_GENERATED_DIR}/generated/testing_ruby.hpp)
add_custom_command(
  OUTPUT ${REDGEM_TESTING_RUBY_HEADER}
  COMMAND ${CMAKE_COMMAND} -DOUTPUT=${REDGEM_TESTING_RUBY_HEADER} "-DSOURCES=${REDGEM_TESTING_RUBY_SOURCES}"
          -DNAMESPACE=redgem::testing::generated -DNAME=kTestingSources -DFOLDER=testing
          -P ${CMAKE_SOURCE_DIR}/cmake/EmbedRuby.cmake
  DEPENDS ${REDGEM_TESTING_RUBY_SOURCES} ${CMAKE_SOURCE_DIR}/cmake/EmbedRuby.cmake
  COMMENT "Embedding the Ruby tests are written with"
  VERBATIM)

add_library(redgem_testing STATIC
  ${REDGEM_TESTING_RUBY_HEADER}
  test_input_bindings.cpp
  test_notes.cpp
  frame_pictures.cpp
  test_library.cpp
  test_saves.cpp
  ruby_tests.cpp
  opened_classes.cpp
  test_stage.cpp
  system_test.cpp
  test_cli.cpp
  test_cli_options.cpp
  testing_texts.cpp)
redgem_embed_texts(redgem_testing testing)
target_include_directories(redgem_testing PRIVATE ${REDGEM_TESTING_GENERATED_DIR})
# (It plays games as the player does: the runtime is all it stands on.)
target_link_libraries(redgem_testing PUBLIC redgem_runtime)
target_compile_options(redgem_testing PRIVATE -Wall -Wextra -Wpedantic -Wno-unused-parameter)
redgem_harden(redgem_testing)

# redgem-test: runs a game's system tests and files of Ruby unit tests. Built beside redgem.
add_executable(redgem_test_tool main.cpp)
set_target_properties(redgem_test_tool PROPERTIES
  OUTPUT_NAME redgem-test
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/src)
target_link_libraries(redgem_test_tool PRIVATE redgem_testing)
target_compile_options(redgem_test_tool PRIVATE -Wall -Wextra -Wpedantic)
redgem_harden(redgem_test_tool)
redgem_harden_program(redgem_test_tool)
install(TARGETS redgem_test_tool RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        COMPONENT ${REDGEM_INSTALL_COMPONENT})
