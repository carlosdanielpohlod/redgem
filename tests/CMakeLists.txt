add_executable(redgem_tests
  test_main.cpp
  unit/math_test.cpp
  unit/fixed_clock_test.cpp
  unit/log_test.cpp
  unit/property_test.cpp
  unit/extras_parser_test.cpp
  unit/transform_test.cpp
  unit/scene_test.cpp
  unit/gltf_loader_test.cpp
  unit/body_config_test.cpp
  unit/input_test.cpp
  unit/script_naming_test.cpp
  unit/cli_test.cpp
  unit/mesh_builder_test.cpp
  unit/scene_document_test.cpp
  unit/editor_camera_test.cpp
  unit/script_files_test.cpp
  unit/primitives_test.cpp
  unit/project_files_test.cpp
  unit/environment_test.cpp
  unit/gpu_select_test.cpp
  unit/culling_test.cpp
  unit/terrain_test.cpp
  unit/animation_test.cpp
  unit/user_settings_test.cpp
  unit/save_store_test.cpp
  unit/touch_controls_test.cpp
  unit/android_package_test.cpp
  unit/frame_buffer_test.cpp
  unit/particles_test.cpp
  unit/animation_player_test.cpp
  integration/physics_world_test.cpp
  integration/vehicle_test.cpp
  integration/ruby_vm_test.cpp
  integration/engine_headless_test.cpp
  integration/scene_script_test.cpp
  integration/ui_input_test.cpp
  integration/gameplay_api_test.cpp
  integration/network_session_test.cpp
  integration/multiplayer_test.cpp
  integration/editor_session_test.cpp
  integration/game_project_test.cpp)
target_link_libraries(redgem_tests PRIVATE redgem_engine doctest::doctest)
target_include_directories(redgem_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(redgem_tests PRIVATE REDGEM_FIXTURES_DIR="${CMAKE_SOURCE_DIR}/tests/fixtures")
target_compile_options(redgem_tests PRIVATE -Wall -Wextra -Wno-unused-parameter)

add_test(NAME cpp.unit_and_integration COMMAND redgem_tests)

# Ruby unit tests run inside the engine's own mruby.
file(GLOB REDGEM_RUBY_TESTS CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/ruby/*_test.rb)
foreach(test_file IN LISTS REDGEM_RUBY_TESTS)
  get_filename_component(test_name ${test_file} NAME_WE)
  add_test(NAME ruby.${test_name} COMMAND redgem --ruby-test ${test_file})
endforeach()

# Guards the harness itself: a failing Ruby test must make the process fail.
add_test(NAME ruby.harness_reports_failures
         COMMAND redgem --ruby-test ${CMAKE_CURRENT_SOURCE_DIR}/ruby_failing/failing_test.rb)
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
           COMMAND redgem ${CMAKE_SOURCE_DIR}/games/${game} --test ${test_file}
                   --screenshots ${REDGEM_SCREENSHOT_DIR}/${game})
  set_tests_properties(system.${game}.${test_name} PROPERTIES LABELS system RUN_SERIAL TRUE)
endforeach()

# Multiplayer: two copies of the game run at once and play together, each driven by its own test.
# tests/system_pairs/<game>/<name>_host_test.rb pairs up with <name>_guest_test.rb.
file(GLOB REDGEM_PAIRED_TESTS CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/system_pairs/*/*_host_test.rb)
foreach(host_test IN LISTS REDGEM_PAIRED_TESTS)
  get_filename_component(game_dir ${host_test} DIRECTORY)
  get_filename_component(game ${game_dir} NAME)
  get_filename_component(host_name ${host_test} NAME_WE)
  string(REGEX REPLACE "_host_test$" "" pair_name ${host_name})
  add_test(NAME system.${game}.${pair_name}_together
           COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/system_pairs/run_pair.sh $<TARGET_FILE:redgem>
                   ${CMAKE_SOURCE_DIR}/games/${game} ${host_test} ${game_dir}/${pair_name}_guest_test.rb
                   ${REDGEM_SCREENSHOT_DIR}/${game})
  set_tests_properties(system.${game}.${pair_name}_together PROPERTIES LABELS system RUN_SERIAL TRUE)
endforeach()

# Guards the system test harness: a failing assertion must make the game exit with an error.
add_test(NAME system.harness_reports_failures
         COMMAND redgem ${CMAKE_SOURCE_DIR}/games/platformer
                 --test ${CMAKE_CURRENT_SOURCE_DIR}/system_failing/failing_system_test.rb
                 --screenshots ${REDGEM_SCREENSHOT_DIR})
set_tests_properties(system.harness_reports_failures PROPERTIES LABELS system RUN_SERIAL TRUE
  PASS_REGULAR_EXPRESSION "system test failed: .*this must fail")

# The editor window itself, driven by a doctest program.
add_executable(redgem_editor_system_tests editor_system/editor_window_test.cpp)
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
               COMMAND redgem ${CMAKE_SOURCE_DIR}/games/${game} --test ${test_file}
                       --screenshots ${REDGEM_SCREENSHOT_DIR}/${game})
      set_tests_properties(system.${game}.${test_name} PROPERTIES LABELS system RUN_SERIAL TRUE)
    endforeach()
  endif()
endforeach()
