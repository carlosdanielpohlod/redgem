add_executable(redgem_tests
  test_main.cpp
  unit/math_test.cpp
  unit/fixed_clock_test.cpp
  unit/log_test.cpp
  unit/files_test.cpp
  unit/embed_test.cpp
  unit/platform_test.cpp
  unit/property_test.cpp
  unit/props_test.cpp
  unit/extras_parser_test.cpp
  unit/transform_test.cpp
  unit/scene_test.cpp
  unit/gltf_loader_test.cpp
  unit/gltf_loader_safety_test.cpp
  unit/safe_path_test.cpp
  unit/image_header_test.cpp
  unit/body_config_test.cpp
  unit/input_test.cpp
  unit/script_naming_test.cpp
  unit/cli_test.cpp
  unit/editor_setup_test.cpp
  unit/mesh_builder_test.cpp
  unit/scene_document_test.cpp
  unit/scene_tree_test.cpp
  unit/gltf_json_test.cpp
  unit/model_import_test.cpp
  unit/editor_logic_test.cpp
  unit/editor_camera_test.cpp
  unit/script_files_test.cpp
  unit/primitives_test.cpp
  unit/project_files_test.cpp
  unit/environment_test.cpp
  unit/gpu_select_test.cpp
  unit/culling_test.cpp
  unit/render_math_test.cpp
  unit/shader_source_test.cpp
  unit/terrain_test.cpp
  unit/animation_test.cpp
  unit/user_settings_test.cpp
  unit/save_store_test.cpp
  unit/xml_writer_test.cpp
  unit/touch_controls_test.cpp
  unit/android_package_test.cpp
  unit/game_settings_test.cpp
  unit/frame_buffer_test.cpp
  unit/wire_test.cpp
  unit/lan_discovery_test.cpp
  unit/particles_test.cpp
  unit/animation_player_test.cpp
  unit/camera_ray_test.cpp
  integration/physics_world_test.cpp
  integration/vehicle_test.cpp
  integration/ruby_vm_test.cpp
  integration/engine_headless_test.cpp
  integration/scene_script_test.cpp
  integration/ui_input_test.cpp
  integration/gameplay_api_test.cpp
  integration/network_session_test.cpp
  integration/multiplayer_test.cpp
  integration/relay_test.cpp
  integration/editor_session_test.cpp
  integration/game_project_test.cpp)
target_link_libraries(redgem_tests PRIVATE redgem_engine doctest::doctest)
target_include_directories(redgem_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(redgem_tests PRIVATE REDGEM_FIXTURES_DIR="${CMAKE_SOURCE_DIR}/tests/fixtures")
target_compile_options(redgem_tests PRIVATE -Wall -Wextra -Wno-unused-parameter)

add_test(NAME cpp.unit_and_integration COMMAND redgem_tests)

# What tests are run with (src/testing, redgem-test) has tests of its own, in a program of their
# own: redgem_tests above links the engine alone, as the program games ship as does.
add_executable(redgem_testing_tests
  test_main.cpp
  testing/test_cli_test.cpp
  testing/test_saves_test.cpp
  testing/test_library_test.cpp)
target_link_libraries(redgem_testing_tests PRIVATE redgem_testing doctest::doctest)
target_include_directories(redgem_testing_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(redgem_testing_tests PRIVATE
  REDGEM_FIXTURES_DIR="${CMAKE_SOURCE_DIR}/tests/fixtures"
  REDGEM_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
target_compile_options(redgem_testing_tests PRIVATE -Wall -Wextra -Wno-unused-parameter)
add_test(NAME cpp.testing_library COMMAND redgem_testing_tests)

# The engine's layers only include what is below them (the table is in the script).
add_test(NAME layering.include_directions
         COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/include_directions.sh ${CMAKE_SOURCE_DIR}/src)
# Guards the check itself: a low layer that includes a higher one must fail it.
add_test(NAME layering.check_catches_upward_includes
         COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/include_directions.sh
                 ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/include_directions/src)
# (Matched on its message: a script that did not run must not count as "it failed as expected".)
set_tests_properties(layering.check_catches_upward_includes PROPERTIES
  PASS_REGULAR_EXPRESSION "forbidden include: src/core/upward.hpp includes render/picture.hpp")

# The program games ship as has no test code in it.
add_test(NAME app.ships_no_test_code
         COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/ships_no_test_code.sh $<TARGET_FILE:redgem>)

# Ruby unit tests run inside the engine's own mruby, by the test tool.
file(GLOB REDGEM_RUBY_TESTS CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/ruby/*_test.rb)
foreach(test_file IN LISTS REDGEM_RUBY_TESTS)
  get_filename_component(test_name ${test_file} NAME_WE)
  add_test(NAME ruby.${test_name} COMMAND redgem_test_tool --ruby ${test_file})
endforeach()

# Guards the harness itself: a failing Ruby test must make the process fail.
add_test(NAME ruby.harness_reports_failures
         COMMAND redgem_test_tool --ruby ${CMAKE_CURRENT_SOURCE_DIR}/ruby_failing/failing_test.rb)
# (Matched on its message: a crash or a missing binary must not count as "it failed as expected".)
set_tests_properties(ruby.harness_reports_failures PROPERTIES
  PASS_REGULAR_EXPRESSION "FAIL HarnessFailureTest.*1 tests, 1 failures")

# System tests open the real game window and drive it with simulated input.
# tests/system/<game>/*_test.rb runs against games/<game>.
set(REDGEM_SCREENSHOT_DIR ${CMAKE_BINARY_DIR}/screenshots)
file(GLOB REDGEM_SYSTEM_TESTS CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/system/*/*_test.rb)
foreach(test_file IN LISTS REDGEM_SYSTEM_TESTS)
  get_filename_component(test_name ${test_file} NAME_WE)
  get_filename_component(game_dir ${test_file} DIRECTORY)
  get_filename_component(game ${game_dir} NAME)
  add_test(NAME system.${game}.${test_name}
           COMMAND redgem_test_tool ${CMAKE_SOURCE_DIR}/games/${game} ${test_file}
                   --screenshots ${REDGEM_SCREENSHOT_DIR}/${game})
  set_tests_properties(system.${game}.${test_name} PROPERTIES LABELS system RUN_SERIAL TRUE)
endforeach()

# Multiplayer: two copies of the game run at once and play together, each driven by its own test.
# tests/system_pairs/<game>/<name>_host_test.rb pairs up with <name>_guest_test.rb. A pair whose
# name ends in _room also gets a relay to meet on (the relay is the engine's own program; the two
# games are run by the test tool).
set(REDGEM_TEST_RELAY_PORT 47841)
file(GLOB REDGEM_PAIRED_TESTS CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/system_pairs/*/*_host_test.rb)
foreach(host_test IN LISTS REDGEM_PAIRED_TESTS)
  get_filename_component(game_dir ${host_test} DIRECTORY)
  get_filename_component(game ${game_dir} NAME)
  get_filename_component(host_name ${host_test} NAME_WE)
  string(REGEX REPLACE "_host_test$" "" pair_name ${host_name})
  set(relay_port "")
  if(pair_name MATCHES "_room$")
    set(relay_port ${REDGEM_TEST_RELAY_PORT})
  endif()
  add_test(NAME system.${game}.${pair_name}_together
           COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/system_pairs/run_pair.sh $<TARGET_FILE:redgem>
                   $<TARGET_FILE:redgem_test_tool> ${CMAKE_SOURCE_DIR}/games/${game} ${host_test} ${game_dir}/${pair_name}_guest_test.rb
                   ${REDGEM_SCREENSHOT_DIR}/${game} ${relay_port})
  set_tests_properties(system.${game}.${pair_name}_together PROPERTIES LABELS system RUN_SERIAL TRUE)
endforeach()

# Guards the system test harness: a failing assertion must make the game exit with an error.
add_test(NAME system.harness_reports_failures
         COMMAND redgem_test_tool ${CMAKE_SOURCE_DIR}/games/platformer
                 ${CMAKE_CURRENT_SOURCE_DIR}/system_failing/failing_system_test.rb
                 --screenshots ${REDGEM_SCREENSHOT_DIR})
set_tests_properties(system.harness_reports_failures PROPERTIES LABELS system RUN_SERIAL TRUE
  PASS_REGULAR_EXPRESSION "system test failed: .*this must fail")

# The editor window itself, driven by a doctest program.
add_executable(redgem_editor_system_tests editor_system/editor_window_test.cpp editor_system/window_picture.cpp)
target_link_libraries(redgem_editor_system_tests PRIVATE redgem_engine doctest::doctest)
target_compile_definitions(redgem_editor_system_tests PRIVATE
  REDGEM_PLATFORMER_DIR="${CMAKE_SOURCE_DIR}/games/platformer"
  REDGEM_FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
  REDGEM_SCREENSHOT_DIR="${REDGEM_SCREENSHOT_DIR}")
add_test(NAME system.editor.window COMMAND redgem_editor_system_tests)
set_tests_properties(system.editor.window PROPERTIES LABELS system RUN_SERIAL TRUE)

# Games whose scene is generated from downloaded assets (not committed) are tested when present.
foreach(game showcase)
  if(EXISTS ${CMAKE_SOURCE_DIR}/games/${game}/scene.glb)
    file(GLOB optional_tests CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/system_optional/${game}/*_test.rb)
    foreach(test_file IN LISTS optional_tests)
      get_filename_component(test_name ${test_file} NAME_WE)
      add_test(NAME system.${game}.${test_name}
               COMMAND redgem_test_tool ${CMAKE_SOURCE_DIR}/games/${game} ${test_file}
                       --screenshots ${REDGEM_SCREENSHOT_DIR}/${game})
      set_tests_properties(system.${game}.${test_name} PROPERTIES LABELS system RUN_SERIAL TRUE)
    endforeach()
  endif()
endforeach()
