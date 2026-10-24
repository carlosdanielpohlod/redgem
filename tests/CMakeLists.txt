add_executable(redgem_tests
  test_main.cpp
  unit/math_test.cpp
  unit/fixed_clock_test.cpp
  unit/stopwatch_test.cpp
  unit/log_test.cpp
  unit/files_test.cpp
  unit/command_line_test.cpp
  unit/names_test.cpp
  unit/texts_test.cpp
  unit/translation_keys_test.cpp
  unit/embed_test.cpp
  unit/platform_test.cpp
  unit/property_test.cpp
  unit/props_test.cpp
  unit/json_scanner_test.cpp
  unit/extras_parser_test.cpp
  unit/transform_test.cpp
  unit/scene_test.cpp
  unit/scene_identity_test.cpp
  unit/gltf_accessors_test.cpp
  unit/gltf_loader_test.cpp
  unit/gltf_loader_safety_test.cpp
  unit/spatial_audio_test.cpp
  unit/safe_path_test.cpp
  unit/image_header_test.cpp
  unit/webp_image_test.cpp
  unit/radiance_image_test.cpp
  unit/body_config_test.cpp
  unit/contact_events_test.cpp
  unit/input_test.cpp
  unit/input_bindings_test.cpp
  unit/input_bindings_store_test.cpp
  unit/script_naming_test.cpp
  unit/cli_test.cpp
  unit/recording_test.cpp
  unit/editor_setup_test.cpp
  unit/mesh_builder_test.cpp
  unit/mesh_pieces_test.cpp
  unit/scene_document_test.cpp
  unit/scene_tree_test.cpp
  unit/gltf_json_test.cpp
  unit/glb_geometry_test.cpp
  unit/glb_compact_test.cpp
  unit/glb_usage_test.cpp
  unit/undo_history_test.cpp
  unit/model_import_test.cpp
  unit/editor_logic_test.cpp
  unit/inspector_logic_test.cpp
  unit/hierarchy_rows_test.cpp
  unit/project_folders_test.cpp
  unit/theme_test.cpp
  unit/labels_test.cpp
  unit/edit_stream_test.cpp
  unit/inspector_typing_test.cpp
  unit/editor_camera_test.cpp
  unit/gated_input_source_test.cpp
  unit/gizmo_test.cpp
  unit/scene_overlays_test.cpp
  unit/script_files_test.cpp
  unit/primitives_test.cpp
  unit/file_drop_test.cpp
  unit/project_files_test.cpp
  unit/game_scenes_test.cpp
  unit/environment_test.cpp
  unit/scene_lights_test.cpp
  unit/gpu_select_test.cpp
  unit/window_test.cpp
  unit/display_settings_test.cpp
  unit/quality_test.cpp
  unit/culling_test.cpp
  unit/render_math_test.cpp
  unit/material_alpha_test.cpp
  unit/visibility_test.cpp
  unit/texel_rows_test.cpp
  unit/instance_records_test.cpp
  unit/scatter_test.cpp
  unit/capture_views_test.cpp
  unit/shadow_cascades_test.cpp
  unit/temporal_test.cpp
  unit/water_interactions_test.cpp
  unit/sky_light_test.cpp
  unit/shader_source_test.cpp
  unit/terrain_test.cpp
  unit/animation_test.cpp
  unit/user_settings_test.cpp
  unit/save_file_test.cpp
  unit/save_safety_test.cpp
  unit/save_database_test.cpp
  unit/xml_writer_test.cpp
  unit/touch_controls_test.cpp
  unit/touch_input_source_test.cpp
  unit/android_package_test.cpp
  unit/game_settings_test.cpp
  unit/json_file_test.cpp
  unit/game_build_test.cpp
  unit/game_identity_test.cpp
  unit/game_shipping_test.cpp
  unit/exe_icon_test.cpp
  unit/frame_buffer_test.cpp
  unit/wire_test.cpp
  unit/wire_codec_test.cpp
  unit/pose_codec_test.cpp
  unit/animation_codec_test.cpp
  unit/http_url_test.cpp
  unit/http_message_test.cpp
  unit/http_response_parser_test.cpp
  unit/lan_discovery_test.cpp
  unit/nearby_games_test.cpp
  unit/network_event_test.cpp
  unit/byte_allowance_test.cpp
  unit/relay_program_test.cpp
  unit/room_choice_test.cpp
  unit/tick_pacer_test.cpp
  unit/server_command_test.cpp
  unit/particles_test.cpp
  unit/particle_layout_test.cpp
  unit/animation_player_test.cpp
  unit/animation_graph_test.cpp
  unit/animation_graph_player_test.cpp
  unit/animation_graph_integration_test.cpp
  unit/graph_animation_codec_test.cpp
  unit/animation_sync_test.cpp
  unit/clip_clock_test.cpp
  unit/camera_ray_test.cpp
  unit/crash_report_test.cpp
  integration/physics_world_test.cpp
  integration/vehicle_test.cpp
  integration/ruby_vm_test.cpp
  integration/script_budget_test.cpp
  integration/crash_handling_test.cpp
  integration/engine_headless_test.cpp
  integration/scene_parts_test.cpp
  integration/presenter_test.cpp
  integration/water_interaction_test.cpp
  integration/scene_script_test.cpp
  integration/ui_input_test.cpp
  integration/input_actions_test.cpp
  integration/gameplay_api_test.cpp
  integration/scatter_test.cpp
  integration/native_arguments_test.cpp
  integration/game_translations_test.cpp
  integration/game_languages_test.cpp
  integration/message_values_test.cpp
  integration/connection_test.cpp
  integration/http_test_server.cpp
  integration/http_client_test.cpp
  integration/http_game_test.cpp
  integration/network_session_test.cpp
  integration/multiplayer_test.cpp
  integration/swordsman.cpp
  integration/relay_test.cpp
  integration/relay_approval_test.cpp
  integration/lobby_test.cpp
  integration/dedicated_server_test.cpp
  integration/connection_approval_test.cpp
  integration/editor_session_test.cpp
  integration/game_project_test.cpp
  integration/scene_tools_test.cpp
  integration/play_tools_test.cpp
  integration/api_reference_test.cpp
  integration/error_hints_test.cpp)
target_link_libraries(redgem_tests PRIVATE redgem_project redgem_game_server doctest::doctest)
target_include_directories(redgem_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
# A program that crashes on purpose, for the tests of crash reports (tests/tools/crash_helper.cpp).
add_executable(redgem_crash_helper tools/crash_helper.cpp)
target_link_libraries(redgem_crash_helper PRIVATE redgem_runtime)
# The tests of builds copy the real player, and look for it where a build does: beside redgem.
add_dependencies(redgem_tests redgem_player redgem redgem_crash_helper)
target_compile_definitions(redgem_tests PRIVATE
  REDGEM_FIXTURES_DIR="${CMAKE_SOURCE_DIR}/tests/fixtures"
  REDGEM_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
  REDGEM_TEMPLATES_DIR="${CMAKE_SOURCE_DIR}/templates"
  REDGEM_PLATFORMER_DIR="${CMAKE_SOURCE_DIR}/games/platformer"
  REDGEM_PLAYER_FILE="$<TARGET_FILE:redgem_player>"
  REDGEM_CRASH_HELPER_FILE="$<TARGET_FILE:redgem_crash_helper>"
  REDGEM_EDITOR_FILE="$<TARGET_FILE:redgem>")
target_compile_options(redgem_tests PRIVATE -Wall -Wextra -Wno-unused-parameter)

add_test(NAME cpp.unit_and_integration COMMAND redgem_tests)

# What tests are run with (src/testing, redgem-test) has tests of its own, in a program of their
# own: redgem_tests above links the engine and its tools, and nothing of what tests are run with.
add_executable(redgem_testing_tests
  testing/testing_test_main.cpp
  testing/test_cli_test.cpp
  testing/test_saves_test.cpp
  testing/frame_pictures_test.cpp
  testing/test_library_test.cpp
  testing/opened_classes_test.cpp
  testing/test_stage_test.cpp
  testing/game_test_suite_test.cpp)
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

# Neither the program games ship as, nor the one that runs them on a server, nor the one they are
# made with has test code in it.
add_test(NAME app.ships_no_test_code
         COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/ships_no_test_code.sh $<TARGET_FILE:redgem_player>
                 $<TARGET_FILE:redgem_server> $<TARGET_FILE:redgem>)
# Guards the check itself: the test tool, which is made of test code, must fail it.
add_test(NAME app.check_catches_test_code
         COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/ships_no_test_code.sh $<TARGET_FILE:redgem_test_tool>)
# (Matched on its message: a script that did not run must not count as "it failed as expected".)
set_tests_properties(app.check_catches_test_code PROPERTIES
  PASS_REGULAR_EXPRESSION "test code in .*redgem-test: \"SystemTest\"")

# The program games ship as has none of the tools games are made with in it (no editor, no project
# hub, no templates, no packer of phone apps, no relay), and the relay has nothing that draws.
add_test(NAME app.player_ships_no_tools
         COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/ships_no_tools.sh player $<TARGET_FILE:redgem_player>)
add_test(NAME app.relay_ships_no_game
         COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/ships_no_tools.sh relay $<TARGET_FILE:redgem_relay>)
add_test(NAME app.server_ships_no_tools
         COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/ships_no_tools.sh server $<TARGET_FILE:redgem_server>)
# Guards the check itself: redgem, which is the tools, must fail it, with every kind of tool
# found (in the order the script looks for them); so must the relay, for the one thing of its own
# that is no player's; and the player, which draws and runs scripts, is no relay.
add_test(NAME app.check_catches_tools
         COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/ships_no_tools.sh player $<TARGET_FILE:redgem>)
# (Matched on its messages: a script that did not run must not count as "it failed as expected".)
set_tests_properties(app.check_catches_tools PROPERTIES
  PASS_REGULAR_EXPRESSION "found in [^\n]*redgem: \"Dear ImGui\".*found in [^\n]*redgem: \"Hierarchy\".*found in [^\n]*redgem: \"Create game\".*found in [^\n]*redgem: \"apksigner\".*found in [^\n]*redgem: \"class Spinner < Behaviour\".*found in [^\n]*redgem: \"cannot import\".*found in [^\n]*redgem: \"--template\".*found in [^\n]*redgem: \"THIRD_PARTY_LICENSES\"")
add_test(NAME app.check_catches_the_relay
         COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/ships_no_tools.sh player $<TARGET_FILE:redgem_relay>)
set_tests_properties(app.check_catches_the_relay PROPERTIES
  PASS_REGULAR_EXPRESSION "found in [^\n]*redgem-relay: \"relay listening on port\"")
# ... and the player is no game server, nor the game server a relay.
add_test(NAME app.check_catches_the_server
         COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/ships_no_tools.sh player $<TARGET_FILE:redgem_server>)
set_tests_properties(app.check_catches_the_server PROPERTIES
  PASS_REGULAR_EXPRESSION "found in [^\n]*redgem-server: \"game server listening on port\"")
add_test(NAME app.check_catches_a_relay_in_the_server
         COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/ships_no_tools.sh server $<TARGET_FILE:redgem_relay>)
set_tests_properties(app.check_catches_a_relay_in_the_server PROPERTIES
  PASS_REGULAR_EXPRESSION "found in [^\n]*redgem-relay: \"relay listening on port\"")
add_test(NAME app.check_catches_a_game_in_the_relay
         COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/ships_no_tools.sh relay $<TARGET_FILE:redgem_player>)
set_tests_properties(app.check_catches_a_game_in_the_relay PROPERTIES
  PASS_REGULAR_EXPRESSION "found in .*redgem-player: \"raylib\"")

# The tools of redgem's command line that open a window or start another program (the others are
# tested in integration/scene_tools_test.cpp and play_tools_test.cpp): a picture of a game at a
# moment, and a game's own tests run by its check.
add_test(NAME system.tools.shot
         COMMAND redgem shot ${CMAKE_SOURCE_DIR}/games/platformer ${CMAKE_CURRENT_BINARY_DIR}/tools_shot.png --at 0.5)
set_tests_properties(system.tools.shot PROPERTIES LABELS system RUN_SERIAL TRUE
  PASS_REGULAR_EXPRESSION "saved [^\n]*tools_shot.png \\(512x288, scene main at 0.5 s\\)")
# (--at 0: the game as it starts, before its first step.)
add_test(NAME system.tools.shot_at_the_start
         COMMAND redgem shot ${CMAKE_SOURCE_DIR}/games/platformer ${CMAKE_CURRENT_BINARY_DIR}/tools_shot_start.png
                 --at 0 --size 64x36)
set_tests_properties(system.tools.shot_at_the_start PROPERTIES LABELS system RUN_SERIAL TRUE
  PASS_REGULAR_EXPRESSION "saved [^\n]*tools_shot_start.png \\(64x36, scene main at 0 s\\)")
add_test(NAME system.tools.shot_through_a_camera
         COMMAND redgem shot ${CMAKE_SOURCE_DIR}/games/platformer ${CMAKE_CURRENT_BINARY_DIR}/tools_shot_camera.png
                 --at 0 --size 64x36 --camera Nowhere)
set_tests_properties(system.tools.shot_through_a_camera PROPERTIES LABELS system RUN_SERIAL TRUE
  PASS_REGULAR_EXPRESSION "no camera called Nowhere; the cameras are: Camera")
# (The fixture's tests: a system test that passes and unit tests that fail.)
add_test(NAME system.tools.check_runs_the_tests
         COMMAND redgem check ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/games/checked --run-tests)
set_tests_properties(system.tools.check_runs_the_tests PROPERTIES LABELS system RUN_SERIAL TRUE
  PASS_REGULAR_EXPRESSION "FAIL tests/score_test.rb: FAIL ScoreTest#test_a_coin_is_worth_ten: [^\n]*expected 10, got 1.*tests: 1 passed, 1 failed")

# The relay's command line, as the program itself answers it.
add_test(NAME relay.help COMMAND redgem_relay --help)
set_tests_properties(relay.help PROPERTIES PASS_REGULAR_EXPRESSION "usage:.*redgem-relay \\[--port <number>\\]")
add_test(NAME relay.refuses_what_it_does_not_understand COMMAND redgem_relay --port many)
set_tests_properties(relay.refuses_what_it_does_not_understand PROPERTIES
  PASS_REGULAR_EXPRESSION "--port needs a number from 0 to 65535")

# The game server's command line, as the program itself answers it.
add_test(NAME server.help COMMAND redgem_server --help)
set_tests_properties(server.help PROPERTIES PASS_REGULAR_EXPRESSION "usage:.*redgem-server <game_dir>")
add_test(NAME server.refuses_what_it_does_not_understand COMMAND redgem_server ${CMAKE_SOURCE_DIR}/games/platformer --rooms many)
set_tests_properties(server.refuses_what_it_does_not_understand PROPERTIES
  PASS_REGULAR_EXPRESSION "--rooms needs a number from 1 to 64")

# Ruby unit tests run inside the engine's own mruby, by the test tool.
file(GLOB REDGEM_RUBY_TESTS CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/ruby/*_test.rb)
foreach(test_file IN LISTS REDGEM_RUBY_TESTS)
  get_filename_component(test_name ${test_file} NAME_WE)
  add_test(NAME ruby.${test_name} COMMAND redgem_test_tool --ruby ${test_file})
endforeach()

# Ruby unit tests of a game's own classes: tests/ruby_games/<game>/*_test.rb run against games/<game>.
file(GLOB REDGEM_GAME_RUBY_TESTS CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/ruby_games/*/*_test.rb)
foreach(test_file IN LISTS REDGEM_GAME_RUBY_TESTS)
  get_filename_component(test_name ${test_file} NAME_WE)
  get_filename_component(game_dir ${test_file} DIRECTORY)
  get_filename_component(game ${game_dir} NAME)
  add_test(NAME ruby.${game}.${test_name}
           COMMAND redgem_test_tool --ruby ${test_file} --game ${CMAKE_SOURCE_DIR}/games/${game})
endforeach()
# Without its game, such a file knows none of the game's classes; and --game wants a game.
add_test(NAME ruby.game_classes_need_their_game
         COMMAND redgem_test_tool --ruby ${CMAKE_CURRENT_SOURCE_DIR}/ruby_games/platformer/mover_test.rb)
set_tests_properties(ruby.game_classes_need_their_game PROPERTIES
  PASS_REGULAR_EXPRESSION "uninitialized constant Mover")
add_test(NAME ruby.game_needs_scripts
         COMMAND redgem_test_tool --ruby ${CMAKE_CURRENT_SOURCE_DIR}/ruby_games/platformer/mover_test.rb
                 --game ${CMAKE_SOURCE_DIR}/docs)
set_tests_properties(ruby.game_needs_scripts PROPERTIES PASS_REGULAR_EXPRESSION "has no scripts folder")

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

# The templates: a game is made from each as `redgem new` makes it, and played by the tests of
# tests/system_templates/<template>/. A template with no test of its own stops the build.
file(GLOB REDGEM_TEMPLATES CONFIGURE_DEPENDS LIST_DIRECTORIES true ${CMAKE_SOURCE_DIR}/templates/*)
foreach(template_dir IN LISTS REDGEM_TEMPLATES)
  get_filename_component(template ${template_dir} NAME)
  file(GLOB template_tests CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/system_templates/${template}/*_test.rb)
  if(NOT template_tests)
    message(FATAL_ERROR "the template '${template}' has no system test in tests/system_templates/${template}/")
  endif()
  foreach(test_file IN LISTS template_tests)
    get_filename_component(test_name ${test_file} NAME_WE)
    add_test(NAME system.template.${template}.${test_name}
             COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/system_templates/run_template.sh $<TARGET_FILE:redgem>
                     $<TARGET_FILE:redgem_test_tool> ${template} ${test_file}
                     ${REDGEM_SCREENSHOT_DIR}/template_${template})
    set_tests_properties(system.template.${template}.${test_name} PROPERTIES LABELS system RUN_SERIAL TRUE)
  endforeach()
endforeach()

# Multiplayer: two copies of the game run at once and play together, each driven by its own test.
# tests/system_pairs/<game>/<name>_host_test.rb pairs up with <name>_guest_test.rb. A pair whose
# name ends in _room also gets a relay to meet on (the relay is the engine's own program,
# redgem-relay; the two games are run by the test tool). Each run has a folder and ports of its
# own (see run_pair.sh).
file(GLOB REDGEM_PAIRED_TESTS CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/system_pairs/*/*_host_test.rb)
foreach(host_test IN LISTS REDGEM_PAIRED_TESTS)
  get_filename_component(game_dir ${host_test} DIRECTORY)
  get_filename_component(game ${game_dir} NAME)
  get_filename_component(host_name ${host_test} NAME_WE)
  string(REGEX REPLACE "_host_test$" "" pair_name ${host_name})
  set(with_relay "")
  if(pair_name MATCHES "_room$")
    set(with_relay relay)
  endif()
  add_test(NAME system.${game}.${pair_name}_together
           COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/system_pairs/run_pair.sh $<TARGET_FILE:redgem_relay>
                   $<TARGET_FILE:redgem_test_tool> ${CMAKE_SOURCE_DIR}/games/${game} ${host_test} ${game_dir}/${pair_name}_guest_test.rb
                   ${REDGEM_SCREENSHOT_DIR}/${game} ${with_relay})
  set_tests_properties(system.${game}.${pair_name}_together PROPERTIES LABELS system RUN_SERIAL TRUE)
endforeach()

# Online play with nobody hosting: a game runs on a dedicated server (redgem-server, with no
# display) and two copies of it join, each driven by its own test.
# tests/system_servers/games/<game>/<name>_first_test.rb pairs up with <name>_second_test.rb, and
# plays games/<game>; tests/system_servers/templates/<template>/ plays templates/<template> the
# same way. Each run has a folder and a port of its own (see run_with_server.sh).
foreach(kind games templates)
  file(GLOB server_tests CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/system_servers/${kind}/*/*_first_test.rb)
  foreach(first_test IN LISTS server_tests)
    get_filename_component(tests_dir ${first_test} DIRECTORY)
    get_filename_component(game ${tests_dir} NAME)
    get_filename_component(first_name ${first_test} NAME_WE)
    string(REGEX REPLACE "_first_test$" "" run_name ${first_name})
    set(test_name system.${kind}.${game}.${run_name}_on_a_server)
    add_test(NAME ${test_name}
             COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/system_servers/run_with_server.sh $<TARGET_FILE:redgem_server>
                     $<TARGET_FILE:redgem_test_tool> ${CMAKE_SOURCE_DIR}/${kind}/${game} ${first_test}
                     ${tests_dir}/${run_name}_second_test.rb ${REDGEM_SCREENSHOT_DIR}/${kind}_${game})
    set_tests_properties(${test_name} PROPERTIES LABELS system RUN_SERIAL TRUE)
  endforeach()
endforeach()

# Guards the system test harness: a failing assertion must make the game exit with an error.
add_test(NAME system.harness_reports_failures
         COMMAND redgem_test_tool ${CMAKE_SOURCE_DIR}/games/platformer
                 ${CMAKE_CURRENT_SOURCE_DIR}/system_failing/failing_system_test.rb
                 --screenshots ${REDGEM_SCREENSHOT_DIR})
set_tests_properties(system.harness_reports_failures PROPERTIES LABELS system RUN_SERIAL TRUE
  PASS_REGULAR_EXPRESSION "system test failed: .*this must fail")

# The editor window itself, driven by a doctest program.
add_executable(redgem_editor_system_tests editor_system/editor_test_main.cpp editor_system/editor_window_test.cpp
               editor_system/window_picture.cpp
               editor_system/window_mouse.cpp)
target_link_libraries(redgem_editor_system_tests PRIVATE redgem_project doctest::doctest)
# It stands where the editor does, beside the player: its Build finds the player as redgem's does.
add_dependencies(redgem_editor_system_tests redgem_player)
set_target_properties(redgem_editor_system_tests PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY $<TARGET_FILE_DIR:redgem_player>)
target_compile_definitions(redgem_editor_system_tests PRIVATE
  REDGEM_PLATFORMER_DIR="${CMAKE_SOURCE_DIR}/games/platformer"
  REDGEM_FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
  REDGEM_SCREENSHOT_DIR="${REDGEM_SCREENSHOT_DIR}"
  REDGEM_PLAYER_FILE="$<TARGET_FILE:redgem_player>")
add_test(NAME system.editor.window COMMAND redgem_editor_system_tests)
set_tests_properties(system.editor.window PROPERTIES LABELS system RUN_SERIAL TRUE)

# What the renderer draws, in a real window, read back as pixels: scenes made in code.
add_executable(redgem_render_system_tests render_system/render_test_main.cpp render_system/render_system_test.cpp
               render_system/animation_graph_render_test.cpp
               render_system/specular_brdf_render_test.cpp
               render_system/render_look_test.cpp render_system/water_render_test.cpp render_system/scatter_lod_render_test.cpp render_system/particle_render_test.cpp
               render_system/see_through_render_test.cpp render_system/temporal_render_test.cpp
               render_system/temporal_depth_render_test.cpp
               render_system/webp_render_test.cpp
               render_system/game_font_render_test.cpp
               render_system/missing_picture_render_test.cpp
               render_system/recording_render_test.cpp
               render_system/display_render_test.cpp
               render_system/render_stage.cpp)
target_link_libraries(redgem_render_system_tests PRIVATE redgem_runtime doctest::doctest)
target_include_directories(redgem_render_system_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(redgem_render_system_tests PRIVATE REDGEM_SCREENSHOT_DIR="${REDGEM_SCREENSHOT_DIR}"
  REDGEM_TEMPLATES_DIR="${CMAKE_SOURCE_DIR}/templates"
  REDGEM_FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
add_test(NAME system.render.pictures COMMAND redgem_render_system_tests)
set_tests_properties(system.render.pictures PROPERTIES LABELS system RUN_SERIAL TRUE)

# Games whose scene is generated from downloaded assets (not committed) are tested when it is
# there, and reported as skipped when it is not: a test that did not run never counts as passed.
set(REDGEM_RUN_IF_BUILT ${CMAKE_CURRENT_SOURCE_DIR}/system_optional/run_if_built.sh)
set(REDGEM_SKIPPED_EXIT_CODE 77)  # what run_if_built.sh ends with when the scene is missing
foreach(game showcase)
  file(GLOB optional_tests CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/system_optional/${game}/*_test.rb)
  foreach(test_file IN LISTS optional_tests)
    get_filename_component(test_name ${test_file} NAME_WE)
    add_test(NAME system.${game}.${test_name}
             COMMAND ${REDGEM_RUN_IF_BUILT} ${CMAKE_SOURCE_DIR}/games/${game}/scene.glb
                     $<TARGET_FILE:redgem_test_tool> ${CMAKE_SOURCE_DIR}/games/${game} ${test_file}
                     --screenshots ${REDGEM_SCREENSHOT_DIR}/${game})
    set_tests_properties(system.${game}.${test_name} PROPERTIES LABELS system RUN_SERIAL TRUE
      SKIP_RETURN_CODE ${REDGEM_SKIPPED_EXIT_CODE})
  endforeach()
endforeach()
# Guards the skipping itself: without the scene the test is not run and says so, with the exit
# code that means "skipped"; with it, the test is run.
add_test(NAME system.optional_tests_without_their_scene_are_skipped
         COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/system_optional/skips_without_scene.sh
                 ${REDGEM_RUN_IF_BUILT} ${REDGEM_SKIPPED_EXIT_CODE})

# The tools that are scripts.
add_test(NAME tools.blender_runs_in_a_home_of_its_own
         COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/tools/blender_home.sh ${CMAKE_SOURCE_DIR}/tools/blender/run.sh)
# The manual's index for coding assistants (llms.txt), which the docs build writes.
add_test(NAME tools.llms_txt COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/tools/llms_txt_test.py)

# How a dedicated server holds up under players (see load/load_test.cpp): bots join a room of the
# platformer on redgem-server and play, and what it costs is measured, a tool for whoever works on
# the network code. load.short is a short run of it, whose numbers the CI prints. (Linux only: it
# reads the server's processor time from /proc.)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  add_executable(redgem_load_test load/load_test.cpp load/load_bot.cpp load/load_report.cpp)
  target_link_libraries(redgem_load_test PRIVATE redgem_net)
  target_include_directories(redgem_load_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
  add_test(NAME load.short
           COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/load/run_load_test.sh $<TARGET_FILE:redgem_server>
                   $<TARGET_FILE:redgem_load_test> ${CMAKE_SOURCE_DIR}/games/platformer --seconds 5 4 8 15)
  set_tests_properties(load.short PROPERTIES LABELS load RUN_SERIAL TRUE)
endif()

# How long a frame of a field of grass and trees takes to draw (see benchmark/field_benchmark.cpp):
# a tool for whoever works on the renderer, built with the tests and never run by them.
add_executable(redgem_field_benchmark
  benchmark/field_benchmark.cpp
  benchmark/field_scene.cpp
  benchmark/field_placements.cpp
  benchmark/field_textures.cpp)
target_link_libraries(redgem_field_benchmark PRIVATE redgem_runtime)
target_include_directories(redgem_field_benchmark PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# How long the scripts and the scene take a frame, and whether that grows while a game spawns
# and destroys objects (see benchmark/scripting_benchmark.cpp): built with the tests, never run by them.
add_executable(redgem_scripting_benchmark benchmark/scripting_benchmark.cpp)
target_link_libraries(redgem_scripting_benchmark PRIVATE redgem_runtime)
target_compile_definitions(redgem_scripting_benchmark PRIVATE
  REDGEM_BENCHMARK_GAME="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/games/sandbox")
