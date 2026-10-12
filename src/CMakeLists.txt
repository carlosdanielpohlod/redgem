# Ruby prelude embedded into the binary (order matters: later files build on earlier ones).
# Only what a game runs on: what tests are written with lives in ruby/testing and src/testing.
set(REDGEM_PRELUDE_SOURCES
  ${CMAKE_SOURCE_DIR}/ruby/engine/kernel.rb
  ${CMAKE_SOURCE_DIR}/ruby/engine/vector3.rb
  ${CMAKE_SOURCE_DIR}/ruby/engine/game_object.rb
  ${CMAKE_SOURCE_DIR}/ruby/engine/behaviour.rb
  ${CMAKE_SOURCE_DIR}/ruby/engine/scene_script.rb
  ${CMAKE_SOURCE_DIR}/ruby/engine/ui.rb
  ${CMAKE_SOURCE_DIR}/ruby/engine/game_menu.rb
  ${CMAKE_SOURCE_DIR}/ruby/engine/audio.rb
  ${CMAKE_SOURCE_DIR}/ruby/engine/particles.rb
  ${CMAKE_SOURCE_DIR}/ruby/engine/animation.rb
  ${CMAKE_SOURCE_DIR}/ruby/engine/network.rb)
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
# is. A template is a game folder; the program knows none of its files by name.
file(GLOB_RECURSE REDGEM_TEMPLATE_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/templates/*)
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

# The engine is a few libraries, one on top of the other, and each program links only the ones it
# is made of: what players receive (redgem-player) has none of the tools in it, and what runs on a
# server (redgem-relay) has nothing that draws.
#   redgem_core     what everything shares
#   redgem_net      sockets, messages, the relay: no window, no scripts
#   redgem_runtime  everything a game needs to run
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
  core/files.cpp
  core/safe_path.cpp
  core/math.cpp
  core/fixed_clock.cpp)
target_link_libraries(redgem_core PUBLIC m)
if(ANDROID)
  # Where a phone shows a program's messages (see core/platform.cpp).
  target_link_libraries(redgem_core PUBLIC log)
endif()

redgem_add_library(redgem_net
  net/frame_buffer.cpp
  net/socket.cpp
  net/wire.cpp
  net/connection.cpp
  net/network_session.cpp
  net/lan_discovery.cpp
  net/relay_server.cpp
  net/relay_program.cpp)
target_link_libraries(redgem_net PUBLIC redgem_core nlohmann_json::nlohmann_json)
if(WIN32)
  target_link_libraries(redgem_net PUBLIC ws2_32)
endif()

redgem_add_library(redgem_runtime
  ${REDGEM_PRELUDE_HEADER}
  scene/property.cpp
  scene/extras_parser.cpp
  scene/transform.cpp
  scene/scene_desc.cpp
  scene/gltf_checks.cpp
  scene/gltf_loader.cpp
  scene/cgltf_impl.cpp
  scene/scene.cpp
  scene/animation.cpp
  scene/animation_player.cpp
  scene/camera_ray.cpp
  scene/particles.cpp
  scene/terrain.cpp
  scene/primitives.cpp
  physics/body_config.cpp
  physics/jolt/jolt_shapes.cpp
  physics/jolt/jolt_vehicle.cpp
  physics/jolt/jolt_physics_world.cpp
  audio/audio.cpp
  input/keys.cpp
  input/input.cpp
  input/raylib_input_source.cpp
  input/touch_controls.cpp
  input/touch_input_source.cpp
  storage/save_store.cpp
  script/script_naming.cpp
  script/ruby_vm.cpp
  script/script_host.cpp
  script/bindings/bindings.cpp
  script/bindings/binding_helpers.cpp
  script/bindings/scene_bindings.cpp
  script/bindings/physics_bindings.cpp
  script/bindings/runtime_bindings.cpp
  script/bindings/input_bindings.cpp
  script/bindings/animation_bindings.cpp
  script/bindings/particles_bindings.cpp
  script/bindings/save_bindings.cpp
  script/bindings/network_bindings.cpp
  render/mesh_builder.cpp
  render/image_header.cpp
  render/renderer.cpp
  render/renderer_setup.cpp
  render/renderer_scene.cpp
  render/renderer_passes.cpp
  render/render_math.cpp
  render/shader_source.cpp
  render/environment.cpp
  render/gpu_select.cpp
  render/window.cpp
  render/ui_font.cpp
  render/culling.cpp
  app/game_settings.cpp
  app/bundled_game.cpp
  app/save_location.cpp
  app/user_settings.cpp
  app/presenter.cpp
  app/frame_painter.cpp
  app/window_presenter.cpp
  app/engine.cpp)
# (The prelude is the runtime's alone: no other library is given the folder it is generated in.)
target_include_directories(redgem_runtime PRIVATE ${REDGEM_GENERATED_DIR})
target_link_libraries(redgem_runtime PUBLIC redgem_net raylib Jolt mruby_lib cgltf_headers nlohmann_json::nlohmann_json m)
if(WIN32)
  # mruby's socket and time gems use Windows' own libraries.
  target_link_libraries(redgem_runtime PUBLIC ws2_32)
endif()

if(NOT ANDROID)
  redgem_add_library(redgem_editor
    editor/glb_file.cpp
    editor/glb_compact.cpp
    editor/gltf_json.cpp
    editor/model_import.cpp
    editor/scene_document.cpp
    editor/editor_camera.cpp
    editor/editor_logic.cpp
    editor/edit_stream.cpp
    editor/picking.cpp
    editor/editor_session.cpp
    editor/console_log.cpp
    editor/editor_app.cpp
    editor/script_files.cpp
    editor/script_editor.cpp
    editor/script_workspace.cpp
    editor/project_files.cpp
    editor/ui/script_editor_panel.cpp
    editor/ui/hierarchy_panel.cpp
    editor/ui/inspector_panel.cpp
    editor/ui/console_panel.cpp
    editor/ui/project_panel.cpp
    editor/ui/create_menu.cpp
    editor/ui/style.cpp
    editor/ui/interface.cpp
    editor/ui/embedded_fonts.cpp)
  target_link_libraries(redgem_editor PUBLIC redgem_runtime imgui_stack)

  redgem_add_library(redgem_project
    ${REDGEM_TEMPLATES_HEADER}
    ${REDGEM_LICENSES_HEADER}
    project/notices.cpp
    project/game_templates.cpp
    project/game_build.cpp
    project/android_package.cpp
    project/recent_games.cpp
    project/project_hub.cpp
    project/editor_setup.cpp
    project/cli.cpp)
  # (The templates and the licenses are the tools' alone, as the prelude is the runtime's.)
  target_include_directories(redgem_project PRIVATE ${REDGEM_PROJECT_GENERATED_DIR})
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
  # packed in the same APK (see app/android_main.cpp). Its name is the one the packer writes into
  # the app (app/android_bundle.hpp).
  add_library(redgem SHARED app/android_main.cpp)
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
  add_executable(redgem_relay net/relay_main.cpp)
  target_link_libraries(redgem_relay PRIVATE redgem_net)
  set_target_properties(redgem_relay PROPERTIES OUTPUT_NAME redgem-relay)
  redgem_finish_program(redgem_relay)

  # redgem: the editor, the project hub and the command line that makes and builds games.
  add_executable(redgem project/main.cpp)
  target_link_libraries(redgem PRIVATE redgem_project)
  # (On Windows the editor opens with no console window behind it.)
  set_target_properties(redgem PROPERTIES WIN32_EXECUTABLE ON)
  redgem_finish_program(redgem)

  install(TARGETS redgem redgem_player redgem_relay RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
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
set_source_files_properties(project/android_package.cpp PROPERTIES
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
