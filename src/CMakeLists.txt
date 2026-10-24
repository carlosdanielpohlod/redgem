# Ruby prelude embedded into the binary (order matters: later files build on earlier ones).
# Only what a game runs on: what tests are written with lives in ruby/testing and src/testing.
set(REDGEM_PRELUDE_SOURCES
  ${CMAKE_SOURCE_DIR}/ruby/engine/kernel.rb
  ${CMAKE_SOURCE_DIR}/ruby/engine/vector3.rb
  ${CMAKE_SOURCE_DIR}/ruby/engine/game_object.rb
  ${CMAKE_SOURCE_DIR}/ruby/engine/components.rb
  ${CMAKE_SOURCE_DIR}/ruby/engine/scene.rb
  ${CMAKE_SOURCE_DIR}/ruby/engine/behaviour.rb
  ${CMAKE_SOURCE_DIR}/ruby/engine/behaviour_timers.rb
  ${CMAKE_SOURCE_DIR}/ruby/engine/scene_script.rb
  ${CMAKE_SOURCE_DIR}/ruby/engine/i18n.rb
  ${CMAKE_SOURCE_DIR}/ruby/engine/ui.rb
  ${CMAKE_SOURCE_DIR}/ruby/engine/ui_widgets.rb
  ${CMAKE_SOURCE_DIR}/ruby/engine/game_menu.rb
  ${CMAKE_SOURCE_DIR}/ruby/engine/game_menu_screen.rb
  ${CMAKE_SOURCE_DIR}/ruby/engine/game_menu_settings.rb
  ${CMAKE_SOURCE_DIR}/ruby/engine/settings.rb
  ${CMAKE_SOURCE_DIR}/ruby/engine/audio.rb
  ${CMAKE_SOURCE_DIR}/ruby/engine/particles.rb
  ${CMAKE_SOURCE_DIR}/ruby/engine/scatter.rb
  ${CMAKE_SOURCE_DIR}/ruby/engine/animation.rb
  ${CMAKE_SOURCE_DIR}/ruby/engine/animation_graph.rb
  ${CMAKE_SOURCE_DIR}/ruby/engine/network.rb
  ${CMAKE_SOURCE_DIR}/ruby/engine/network_said.rb
  ${CMAKE_SOURCE_DIR}/ruby/engine/network_spawns.rb
  ${CMAKE_SOURCE_DIR}/ruby/engine/network_spawn_requests.rb
  ${CMAKE_SOURCE_DIR}/ruby/engine/network_resends.rb
  ${CMAKE_SOURCE_DIR}/ruby/engine/network_sync.rb
  ${CMAKE_SOURCE_DIR}/ruby/engine/network_snapshots.rb
  ${CMAKE_SOURCE_DIR}/ruby/engine/network_approval.rb
  ${CMAKE_SOURCE_DIR}/ruby/engine/http.rb)
set(REDGEM_GENERATED_DIR ${CMAKE_BINARY_DIR}/generated_include)
set(REDGEM_PRELUDE_HEADER ${REDGEM_GENERATED_DIR}/generated/ruby_prelude.hpp)
add_custom_command(
  OUTPUT ${REDGEM_PRELUDE_HEADER}
  COMMAND ${CMAKE_COMMAND} -DOUTPUT=${REDGEM_PRELUDE_HEADER} "-DSOURCES=${REDGEM_PRELUDE_SOURCES}"
          -P ${CMAKE_SOURCE_DIR}/cmake/EmbedRuby.cmake
  DEPENDS ${REDGEM_PRELUDE_SOURCES} ${CMAKE_SOURCE_DIR}/cmake/EmbedRuby.cmake
  COMMENT "Embedding the Ruby prelude"
  VERBATIM)

# Starting points for new games: every file of templates/<name>/ is built into the program as it
# is. A template is a game folder with a template.json; the program knows none of its files by
# name. (A folder without that file, such as a build made beside a template, is not one.)
file(GLOB REDGEM_TEMPLATE_INFOS CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/templates/*/template.json)
set(REDGEM_TEMPLATE_SOURCES "")
foreach(info IN LISTS REDGEM_TEMPLATE_INFOS)
  get_filename_component(template_dir ${info} DIRECTORY)
  file(GLOB_RECURSE template_files CONFIGURE_DEPENDS ${template_dir}/*)
  list(APPEND REDGEM_TEMPLATE_SOURCES ${template_files})
endforeach()
# (In a folder of their own, which only the tools' library is given.)
set(REDGEM_PROJECT_GENERATED_DIR ${CMAKE_BINARY_DIR}/generated_project_include)
set(REDGEM_TEMPLATES_HEADER ${REDGEM_PROJECT_GENERATED_DIR}/generated/templates.hpp)
add_custom_command(
  OUTPUT ${REDGEM_TEMPLATES_HEADER}
  COMMAND ${CMAKE_COMMAND} -DOUTPUT=${REDGEM_TEMPLATES_HEADER} -DROOT=${CMAKE_SOURCE_DIR}/templates
          -DNAME=template_files "-DSOURCES=${REDGEM_TEMPLATE_SOURCES}" -P ${CMAKE_SOURCE_DIR}/cmake/EmbedFolder.cmake
  DEPENDS ${REDGEM_TEMPLATE_SOURCES} ${CMAKE_SOURCE_DIR}/cmake/EmbedFolder.cmake
  COMMENT "Embedding game templates"
  VERBATIM)

# The licenses that travel with every built game (see project/notices.hpp), embedded in the binary.
set(REDGEM_LICENSE_SOURCES ${CMAKE_SOURCE_DIR}/LICENSE ${CMAKE_SOURCE_DIR}/THIRD_PARTY_LICENSES.md)
set(REDGEM_LICENSES_HEADER ${REDGEM_PROJECT_GENERATED_DIR}/generated/licenses.hpp)
add_custom_command(
  OUTPUT ${REDGEM_LICENSES_HEADER}
  COMMAND ${CMAKE_COMMAND} -DOUTPUT=${REDGEM_LICENSES_HEADER} -DROOT=${CMAKE_SOURCE_DIR}
          -DNAME=kLicenseFiles "-DSOURCES=${REDGEM_LICENSE_SOURCES}" -P ${CMAKE_SOURCE_DIR}/cmake/EmbedFiles.cmake
  DEPENDS ${REDGEM_LICENSE_SOURCES} ${CMAKE_SOURCE_DIR}/cmake/EmbedFiles.cmake
  COMMENT "Embedding the licenses"
  VERBATIM)

# The texts users read, in translation files: locales/<part>/<language>.json (see
# core/translations.hpp). Each part is built into the library that shows it, which adds it with a
# function of its own (add_<part>_texts), so that a program carries the texts of its parts and no
# others': the player has none of the editor's.
function(redgem_embed_texts target part)
  file(GLOB files CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/locales/${part}/*.json)
  set(folder ${CMAKE_BINARY_DIR}/generated_texts/${part})
  set(header ${folder}/generated/${part}_texts.hpp)
  add_custom_command(
    OUTPUT ${header}
    COMMAND ${CMAKE_COMMAND} -DOUTPUT=${header} -DROOT=${CMAKE_SOURCE_DIR}/locales/${part} -DNAME=${part}_texts
            "-DSOURCES=${files}" -P ${CMAKE_SOURCE_DIR}/cmake/EmbedFolder.cmake
    DEPENDS ${files} ${CMAKE_SOURCE_DIR}/cmake/EmbedFolder.cmake
    COMMENT "Embedding the ${part} texts"
    VERBATIM)
  target_sources(${target} PRIVATE ${header})
  target_include_directories(${target} PRIVATE ${folder})
endfunction()

# The engine is a few libraries, one on top of the other, and each program links only the ones it
# is made of: what players receive (redgem-player) has none of the tools in it, and what runs on a
# server (redgem-relay) has nothing that draws.
#   redgem_core     what everything shares
#   redgem_net      sockets, messages, the relay, web requests: no window, no scripts
#   redgem_runtime  everything a game needs to run
#   redgem_game_server  a game run on a server for players to join, in rooms (redgem-server)
#   redgem_editor   the editor, with the library its interface is drawn with
#   redgem_project  the tools around a game: templates, builds, the project hub, the command line
include(Hardening)

# One of the engine's libraries: its sources are compiled like all the engine's own code.
function(redgem_add_library name)
  add_library(${name} STATIC ${ARGN})
  target_include_directories(${name} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
  target_compile_options(${name} PRIVATE -Wall -Wextra -Wpedantic -Wno-unused-parameter)
  redgem_harden(${name})
endfunction()

# One of the engine's programs (or, on Android, the library the system loads).
function(redgem_finish_program target)
  target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
  redgem_harden(${target})
  redgem_harden_program(${target})
endfunction()

redgem_add_library(redgem_core
  core/log.cpp
  core/xml_writer.cpp
  core/platform.cpp
  core/platform_desktop.cpp
  core/platform_log.cpp
  core/files.cpp
  core/command_line.cpp
  core/names.cpp
  core/texts.cpp
  core/text_placeholders.cpp
  core/translations.cpp
  core/language_names.cpp
  core/filled_texts.cpp
  core/core_texts.cpp
  core/platform_language.cpp
  core/safe_path.cpp
  core/image_header.cpp
  core/float_image.cpp
  core/radiance_header.cpp
  core/radiance_pixels.cpp
  core/math.cpp
  core/fixed_clock.cpp
  core/stopwatch.cpp)
target_link_libraries(redgem_core PUBLIC nlohmann_json::nlohmann_json m)
redgem_embed_texts(redgem_core core)
if(ANDROID)
  # Where a phone shows a program's messages (see core/platform.cpp).
  target_link_libraries(redgem_core PUBLIC log)
endif()

redgem_add_library(redgem_net
  net/frame_buffer.cpp
  net/socket/socket_native.cpp
  net/socket/socket.cpp
  net/socket/socket_io.cpp
  net/socket/datagrams.cpp
  net/wire.cpp
  net/wire_checks.cpp
  net/wire_codec.cpp
  net/private_address.cpp
  net/decimal_text.cpp
  net/utf8_text.cpp
  net/pose_quantization.cpp
  net/pose_codec.cpp
  net/animation_codec.cpp
  net/graph_animation_codec.cpp
  net/pose_baselines.cpp
  net/connection.cpp
  net/network_session.cpp
  net/network_session_host.cpp
  net/network_session_guest.cpp
  net/network_session_server.cpp
  net/network_session_approval.cpp
  net/network_session_room_approval.cpp
  net/network_session_poses.cpp
  net/rooms/room_codes.cpp
  net/rooms/code_guessers.cpp
  net/rooms/lobby.cpp
  net/discovery/nearby_games.cpp
  net/discovery/lan_discovery.cpp
  net/rooms/relay_server.cpp
  net/rooms/relay_server_visitors.cpp
  net/rooms/relay_server_rooms.cpp
  net/rooms/relay_server_approval.cpp
  net/rooms/relay_program.cpp
  net/rooms/relay_texts.cpp
  net/http/http_url.cpp
  net/http/http_message.cpp
  net/http/http_chunks.cpp
  net/http/http_response_parser.cpp
  net/http/http_response_head.cpp
  net/http/http_trust.cpp
  net/http/http_trust_system.cpp
  net/http/http_stream.cpp
  net/http/http_tls.cpp
  net/http/http_exchange.cpp
  net/http/http_client.cpp)
target_link_libraries(redgem_net PUBLIC redgem_core nlohmann_json::nlohmann_json mbedtls)
redgem_embed_texts(redgem_net relay)
if(WIN32)
  # (crypt32: the Windows certificate store, which https:// answers are checked against.)
  target_link_libraries(redgem_net PUBLIC ws2_32 crypt32)
endif()

redgem_add_library(redgem_runtime
  ${REDGEM_PRELUDE_HEADER}
  scene/property.cpp
  scene/gltf/json_scanner.cpp
  scene/gltf/json_scanner_strings.cpp
  scene/gltf/extras_parser.cpp
  scene/transform.cpp
  scene/scene_desc.cpp
  scene/gltf/gltf_checks.cpp
  scene/gltf/gltf_budget.cpp
  scene/gltf/gltf_accessors.cpp
  scene/gltf/gltf_loader.cpp
  scene/gltf/gltf_reader_materials.cpp
  scene/gltf/gltf_reader_meshes.cpp
  scene/gltf/gltf_reader_nodes.cpp
  scene/gltf/gltf_reader_skins.cpp
  scene/gltf/gltf_reader_animations.cpp
  scene/gltf/cgltf_impl.cpp
  scene/scene.cpp
  scene/scene_from_desc.cpp
  scene/scene_copies.cpp
  scene/scatter.cpp
  scene/scatter_looks.cpp
  scene/animation/animation.cpp
  scene/animation/animation_blend.cpp
  scene/animation/animation_graph.cpp
  scene/animation/animation_graph_player.cpp
  scene/animation/clip_clock.cpp
  scene/animation/animation_player.cpp
  scene/animation/animation_player_characters.cpp
  scene/animation/animation_player_sync.cpp
  scene/animation/animation_player_posing.cpp
  scene/animation/animation_player_graph.cpp
  scene/camera_ray.cpp
  scene/particles.cpp
  scene/particle_look.cpp
  scene/water_geometry.cpp
  scene/water_interactions.cpp
  scene/shapes/terrain.cpp
  scene/shapes/terrain_sculpt.cpp
  scene/shapes/terrain_raycast.cpp
  scene/shapes/primitives.cpp
  scene/shapes/primitive_round_shapes.cpp
  physics/body_config.cpp
  physics/contact_events.cpp
  physics/jolt/jolt_runtime.cpp
  physics/jolt/jolt_shapes.cpp
  physics/jolt/jolt_vehicle.cpp
  physics/jolt/jolt_physics_world.cpp
  physics/jolt/jolt_physics_world_bodies.cpp
  physics/jolt/jolt_physics_world_characters.cpp
  physics/jolt/jolt_physics_world_vehicles.cpp
  physics/jolt/jolt_physics_world_contacts.cpp
  physics/jolt/jolt_physics_world_queries.cpp
  audio/audio.cpp
  input/keys.cpp
  input/devices.cpp
  input/input.cpp
  input/raylib_input_source.cpp
  input/touch_controls.cpp
  input/touch_input_source.cpp
  storage/saved_values.cpp
  storage/durable_file.cpp
  storage/save_file.cpp
  script/script_naming.cpp
  script/ruby_vm.cpp
  script/script_host.cpp
  script/script_host_scripts.cpp
  script/script_host_callbacks.cpp
  script/script_host_network.cpp
  script/script_host_http.cpp
  script/game_locales.cpp
  script/bindings/bindings.cpp
  script/bindings/binding_helpers.cpp
  script/bindings/game_object_bindings.cpp
  script/bindings/vector3_bindings.cpp
  script/bindings/transform_bindings.cpp
  script/bindings/scene_bindings.cpp
  script/bindings/scatter_bindings.cpp
  script/bindings/camera_bindings.cpp
  script/bindings/physics_bindings.cpp
  script/bindings/light_bindings.cpp
  script/bindings/runtime_bindings.cpp
  script/bindings/ui_bindings.cpp
  script/bindings/audio_bindings.cpp
  script/bindings/input_bindings.cpp
  script/bindings/animation_bindings.cpp
  script/bindings/animation_graph_bindings.cpp
  script/bindings/animation_graph_sync.cpp
  script/bindings/animation_sync_bindings.cpp
  script/bindings/particles_bindings.cpp
  script/bindings/particle_look_args.cpp
  script/bindings/save_bindings.cpp
  script/bindings/settings_bindings.cpp
  script/bindings/i18n_bindings.cpp
  script/bindings/message_values.cpp
  script/bindings/network_bindings.cpp
  script/bindings/network_search_bindings.cpp
  script/bindings/network_state_bindings.cpp
  script/bindings/network_pose_bindings.cpp
  script/bindings/network_approval_bindings.cpp
  script/bindings/http_bindings.cpp
  render/mesh_builder.cpp
  render/environment_quality.cpp
  render/sky_directions.cpp
  render/sky_harmonics.cpp
  render/half_float.cpp
  render/sky_sampling.cpp
  render/sky_cube.cpp
  render/sky_light.cpp
  render/sky_picture.cpp
  render/renderer_sky.cpp
  render/shadow_cascades.cpp
  render/temporal.cpp
  render/gl_extras.cpp
  render/frame_readback.cpp
  render/mesh_pieces.cpp
  render/texture_loading.cpp
  render/renderer.cpp
  render/renderer_pipeline.cpp
  render/renderer_targets.cpp
  render/renderer_assets.cpp
  render/renderer_pictures.cpp
  render/renderer_frame.cpp
  render/renderer_frame_texels.cpp
  render/renderer_environment.cpp
  render/renderer_objects.cpp
  render/renderer_see_through.cpp
  render/renderer_water.cpp
  render/water_ripple_packet.cpp
  render/renderer_water_ripples.cpp
  render/renderer_scatter.cpp
  render/renderer_temporal.cpp
  render/renderer_particles.cpp
  render/renderer_particle_glow.cpp
  render/renderer_shadows.cpp
  render/renderer_reflections.cpp
  render/renderer_post.cpp
  render/renderer_overlay.cpp
  render/mesh_pass.cpp
  render/particle_buffer.cpp
  render/shader_programs.cpp
  render/render_math.cpp
  render/material_alpha.cpp
  render/particle_layout.cpp
  render/visibility.cpp
  render/texel_rows.cpp
  render/instance_records.cpp
  render/instance_batches.cpp
  render/scatter_chunks.cpp
  render/capture_views.cpp
  render/shader_source.cpp
  render/environment.cpp
  render/scene_lights.cpp
  render/gpu_select.cpp
  render/window.cpp
  render/display_settings.cpp
  render/quality.cpp
  render/font_rasterizer.cpp
  render/game_fonts.cpp
  render/ui_font.cpp
  render/culling.cpp
  app/json_file.cpp
  app/android/android_settings.cpp
  app/game_settings.cpp
  app/bundled_game.cpp
  app/save_location.cpp
  app/user_settings.cpp
  app/game_display.cpp
  app/presenter.cpp
  app/frame_painter.cpp
  app/window_presenter.cpp
  app/recording.cpp
  app/video_recorder.cpp
  app/encoder_process.cpp
  app/engine.cpp
  app/engine_frame.cpp
  app/engine_scenes.cpp
  app/scene_bodies.cpp
  app/scene_water.cpp
  app/scene_files.cpp
  app/touch_circles.cpp
  app/game_texts.cpp)
redgem_embed_texts(redgem_runtime game)
# (The prelude is the runtime's alone: no other library is given the folder it is generated in.)
target_include_directories(redgem_runtime PRIVATE ${REDGEM_GENERATED_DIR})
target_link_libraries(redgem_runtime PUBLIC redgem_net raylib Jolt mruby_lib cgltf_headers nlohmann_json::nlohmann_json m)
target_link_libraries(redgem_runtime PRIVATE freetype)
if(WIN32)
  # mruby's socket and time gems use Windows' own libraries.
  target_link_libraries(redgem_runtime PUBLIC ws2_32)
endif()

if(NOT ANDROID)
  redgem_add_library(redgem_game_server
    server/room_choice.cpp
    server/tick_pacer.cpp
    server/game_server.cpp
    server/game_server_rooms.cpp
    server/server_command.cpp
    server/stop_signals.cpp
    server/server_program.cpp
    server/save_database.cpp
    server/room_slot.cpp
    server/room_thread.cpp
    server/server_texts.cpp)
  redgem_embed_texts(redgem_game_server server)
  target_link_libraries(redgem_game_server PUBLIC redgem_runtime sqlite3)

  redgem_add_library(redgem_editor
    editor/document/glb_file.cpp
    editor/document/glb_usage.cpp
    editor/document/glb_compact.cpp
    editor/document/gltf_json.cpp
    editor/document/gltf_json_entries.cpp
    editor/document/glb_geometry.cpp
    editor/document/model_import.cpp
    editor/document/model_merge.cpp
    editor/document/model_import_desc.cpp
    editor/node_tree.cpp
    editor/document/scene_document.cpp
    editor/document/scene_document_history.cpp
    editor/document/scene_document_memory.cpp
    editor/document/scene_document_history_bytes.cpp
    editor/document/scene_document_terrain.cpp
    editor/document/scene_document_edits.cpp
    editor/document/scene_document_copies.cpp
    editor/document/scene_document_create.cpp
    editor/document/scene_document_skeletons.cpp
    editor/document/scene_document_import.cpp
    editor/editor_camera.cpp
    editor/editor_logic.cpp
    editor/edit_stream.cpp
    editor/picking.cpp
    editor/editor_session.cpp
    editor/editor_session_create.cpp
    editor/editor_session_play.cpp
    editor/console_log.cpp
    editor/gizmo.cpp
    editor/scene_viewport.cpp
    editor/scene_overlays.cpp
    editor/editor_app.cpp
    editor/editor_frame.cpp
    editor/editor_commands.cpp
    editor/editor_input.cpp
    editor/editor_menus.cpp
    editor/editor_bars.cpp
    editor/editor_layout.cpp
    editor/editor_dialogs.cpp
    editor/editor_viewport.cpp
    editor/editor_scene_view.cpp
    editor/editor_scenes.cpp
    editor/scripts/script_files.cpp
    editor/scripts/script_editor.cpp
    editor/scripts/script_workspace.cpp
    editor/file_drop.cpp
    editor/project_files.cpp
    editor/game_scenes.cpp
    editor/ui/script_editor_panel.cpp
    editor/ui/hierarchy_panel.cpp
    editor/ui/hierarchy_rows.cpp
    editor/inspector_logic.cpp
    editor/ui/inspector_panel.cpp
    editor/ui/inspector_widgets.cpp
    editor/ui/inspector_object.cpp
    editor/ui/inspector_material.cpp
    editor/ui/inspector_light_camera.cpp
    editor/ui/inspector_terrain.cpp
    editor/ui/inspector_scripts.cpp
    editor/ui/inspector_physics.cpp
    editor/ui/inspector_properties.cpp
    editor/ui/console_panel.cpp
    editor/ui/project_panel.cpp
    editor/ui/project_folders.cpp
    editor/ui/new_script_form.cpp
    editor/ui/create_menu.cpp
    editor/ui/style.cpp
    editor/ui/labels.cpp
    editor/ui/theme.cpp
    editor/ui/fonts.cpp
    editor/ui/interface.cpp
    editor/ui/embedded_fonts.cpp
    editor/editor_texts.cpp)
  redgem_embed_texts(redgem_editor editor)
  target_link_libraries(redgem_editor PUBLIC redgem_runtime imgui_stack)

  redgem_add_library(redgem_project
    ${REDGEM_TEMPLATES_HEADER}
    ${REDGEM_LICENSES_HEADER}
    project/notices.cpp
    project/game_templates.cpp
    project/game_names.cpp
    project/build_record.cpp
  project/game_build.cpp
    project/game_build_targets.cpp
    project/game_build_files.cpp
    project/android/android_package.cpp
    project/android/android_package_names.cpp
    project/android/android_package_sdk.cpp
    project/android/android_package_id.cpp
    project/android/android_package_signing.cpp
    project/android/android_package_manifest.cpp
    project/android/android_package_stage.cpp
    project/android/android_package_tools.cpp
    project/recent_games.cpp
    project/project_hub.cpp
    project/project_hub_frame.cpp
    project/project_hub_card.cpp
    project/project_hub_panels.cpp
    project/editor_setup.cpp
    project/editor_run.cpp
    project/cli.cpp
    project/cli_options.cpp
    project/project_texts.cpp)
  redgem_embed_texts(redgem_project project)
  # (The templates and the licenses are the tools' alone, as the prelude is the runtime's.)
  target_include_directories(redgem_project PRIVATE ${REDGEM_PROJECT_GENERATED_DIR})
  # What `redgem --version` says: the version in project() at the top of the build.
  target_compile_definitions(redgem_project PRIVATE REDGEM_VERSION="${redgem_VERSION}")
  target_link_libraries(redgem_project PUBLIC redgem_editor)
endif()

# Where the programs are put by `cmake --install build --component redgem`: all in bin/, and the
# players built for other systems in bin/runtimes/<system>/. A build looks for its player beside
# the program that builds (see project/game_build.hpp), so an installation works as the build
# folder does. (The component keeps the engine's programs apart from what the libraries it is
# built on install of their own: their headers and archives, which nobody running RedGem needs.)
include(GNUInstallDirs)
set(REDGEM_INSTALL_COMPONENT redgem)

if(ANDROID)
  # On Android the player is a library the system loads into a native activity; it plays the game
  # packed in the same APK (see app/android/android_main.cpp). Its name is the one the packer writes into
  # the app (app/android/android_bundle.hpp).
  add_library(redgem SHARED app/android/android_main.cpp)
  target_link_libraries(redgem PRIVATE redgem_runtime android log GLESv3)
  target_include_directories(redgem SYSTEM PRIVATE ${ANDROID_NDK}/sources/android/native_app_glue)
  target_link_options(redgem PRIVATE -u ANativeActivity_onCreate)
  redgem_finish_program(redgem)
else()
  # redgem-player: what players receive. It plays the game in the folder beside it, and is made
  # of the runtime alone (tests/ships_no_tools.sh looks into the program that came out).
  add_executable(redgem_player app/player_main.cpp)
  target_link_libraries(redgem_player PRIVATE redgem_runtime)
  # On Windows a game is a windowed program, with no console window behind it.
  set_target_properties(redgem_player PROPERTIES OUTPUT_NAME redgem-player WIN32_EXECUTABLE ON)
  redgem_finish_program(redgem_player)

  # redgem-relay: where games played over the internet meet. It runs on servers: it has no window
  # and links nothing that opens one.
  add_executable(redgem_relay net/rooms/relay_main.cpp)
  target_link_libraries(redgem_relay PRIVATE redgem_net)
  set_target_properties(redgem_relay PROPERTIES OUTPUT_NAME redgem-relay)
  redgem_finish_program(redgem_relay)

  # redgem-server: a game run on a server with nobody at it, for players to join. It is the runtime
  # and no tools; it never opens a window, so it needs no display and no graphics card.
  add_executable(redgem_server server/server_main.cpp)
  target_link_libraries(redgem_server PRIVATE redgem_game_server)
  set_target_properties(redgem_server PROPERTIES OUTPUT_NAME redgem-server)
  redgem_finish_program(redgem_server)

  # redgem: the editor, the project hub and the command line that makes and builds games.
  add_executable(redgem project/main.cpp)
  target_link_libraries(redgem PRIVATE redgem_project)
  # (On Windows the editor opens with no console window behind it.)
  set_target_properties(redgem PROPERTIES WIN32_EXECUTABLE ON)
  redgem_finish_program(redgem)

  install(TARGETS redgem redgem_player redgem_relay redgem_server RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
          COMPONENT ${REDGEM_INSTALL_COMPONENT})
  # The players for other systems, when they were built (tools/windows/build_runtime.sh,
  # tools/android/build_runtime.sh put them beside this build's programs).
  install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/runtimes DESTINATION ${CMAKE_INSTALL_BINDIR}
          COMPONENT ${REDGEM_INSTALL_COMPONENT} USE_SOURCE_PERMISSIONS OPTIONAL)
endif()

# What games are tested with: a library and a program of their own (redgem-test), which nothing
# above links to. It is a tool for a developer's desk, so phones get none.
if(NOT ANDROID)
  add_subdirectory(testing)
endif()

# The oldest Android a game's app runs on is written once, in a file the build of the engine for
# Android (tools/android/build_runtime.sh) reads as well: the app's manifest and the engine inside
# it always agree.
file(STRINGS ${CMAKE_SOURCE_DIR}/tools/android/min_api.txt REDGEM_ANDROID_MIN_API LIMIT_COUNT 1)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/tools/android/min_api.txt)
set_source_files_properties(project/android/android_package_manifest.cpp PROPERTIES
  COMPILE_DEFINITIONS "REDGEM_ANDROID_MIN_API=${REDGEM_ANDROID_MIN_API}")

# The fonts are assembled into the binary (core/embed.hpp): rebuild when they change. The one
# texts are drawn with belongs to the renderer and is in every game; the code font is the editor's.
set(REDGEM_FONT_DIR ${CMAKE_SOURCE_DIR}/assets/editor/fonts)
set_source_files_properties(render/ui_font.cpp PROPERTIES
  COMPILE_DEFINITIONS "REDGEM_UI_FONT_FILE=\"${REDGEM_FONT_DIR}/InterVariable.ttf\""
  OBJECT_DEPENDS "${REDGEM_FONT_DIR}/InterVariable.ttf")
set_source_files_properties(editor/ui/embedded_fonts.cpp PROPERTIES
  COMPILE_DEFINITIONS "REDGEM_CODE_FONT_FILE=\"${REDGEM_FONT_DIR}/JetBrainsMono-Regular.ttf\""
  OBJECT_DEPENDS "${REDGEM_FONT_DIR}/JetBrainsMono-Regular.ttf")
