# Ruby prelude embedded into the binary (order matters: later files build on earlier ones).
# Only what a game runs on: what tests are written with lives in ruby/testing and src/testing.
set(REDGEM_PRELUDE_SOURCES
  ${CMAKE_SOURCE_DIR}/ruby/engine/kernel.rb
  ${CMAKE_SOURCE_DIR}/ruby/engine/vector3.rb
  ${CMAKE_SOURCE_DIR}/ruby/engine/game_object.rb
  ${CMAKE_SOURCE_DIR}/ruby/engine/behaviour.rb
  ${CMAKE_SOURCE_DIR}/ruby/engine/scene_script.rb
  ${CMAKE_SOURCE_DIR}/ruby/engine/ui.rb
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

# Starting points for new games (templates/<name>/scripts/*.rb), embedded in the binary.
file(GLOB_RECURSE REDGEM_TEMPLATE_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/templates/*.rb)
set(REDGEM_TEMPLATES_HEADER ${REDGEM_GENERATED_DIR}/generated/templates.hpp)
add_custom_command(
  OUTPUT ${REDGEM_TEMPLATES_HEADER}
  COMMAND ${CMAKE_COMMAND} -DOUTPUT=${REDGEM_TEMPLATES_HEADER} -DROOT=${CMAKE_SOURCE_DIR}/templates
          -DNAME=kTemplateFiles "-DSOURCES=${REDGEM_TEMPLATE_SOURCES}" -P ${CMAKE_SOURCE_DIR}/cmake/EmbedFiles.cmake
  DEPENDS ${REDGEM_TEMPLATE_SOURCES} ${CMAKE_SOURCE_DIR}/cmake/EmbedFiles.cmake
  COMMENT "Embedding game templates"
  VERBATIM)

# The licenses that travel with every built game (see project/notices.hpp), embedded in the binary.
set(REDGEM_LICENSE_SOURCES ${CMAKE_SOURCE_DIR}/LICENSE ${CMAKE_SOURCE_DIR}/THIRD_PARTY_LICENSES.md)
set(REDGEM_LICENSES_HEADER ${REDGEM_GENERATED_DIR}/generated/licenses.hpp)
add_custom_command(
  OUTPUT ${REDGEM_LICENSES_HEADER}
  COMMAND ${CMAKE_COMMAND} -DOUTPUT=${REDGEM_LICENSES_HEADER} -DROOT=${CMAKE_SOURCE_DIR}
          -DNAME=kLicenseFiles "-DSOURCES=${REDGEM_LICENSE_SOURCES}" -P ${CMAKE_SOURCE_DIR}/cmake/EmbedFiles.cmake
  DEPENDS ${REDGEM_LICENSE_SOURCES} ${CMAKE_SOURCE_DIR}/cmake/EmbedFiles.cmake
  COMMENT "Embedding the licenses"
  VERBATIM)

add_library(redgem_engine STATIC
  ${REDGEM_PRELUDE_HEADER}
  ${REDGEM_TEMPLATES_HEADER}
  ${REDGEM_LICENSES_HEADER}
  core/log.cpp
  core/xml_writer.cpp
  core/platform.cpp
  core/files.cpp
  core/safe_path.cpp
  core/math.cpp
  core/fixed_clock.cpp
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
  net/frame_buffer.cpp
  net/socket.cpp
  net/wire.cpp
  net/network_session.cpp
  net/lan_discovery.cpp
  net/relay_server.cpp
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
  editor/glb_file.cpp
  editor/gltf_json.cpp
  editor/model_import.cpp
  editor/scene_document.cpp
  editor/editor_camera.cpp
  editor/editor_logic.cpp
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
  editor/ui/embedded_fonts.cpp
  app/game_settings.cpp
  app/bundled_game.cpp
  storage/save_store.cpp
  app/save_location.cpp
  app/user_settings.cpp
  app/presenter.cpp
  app/frame_painter.cpp
  app/window_presenter.cpp
  app/engine.cpp
  project/notices.cpp
  project/game_templates.cpp
  project/game_build.cpp
  project/android_package.cpp
  project/recent_games.cpp
  project/project_hub.cpp
  project/editor_setup.cpp
  project/cli.cpp)

target_include_directories(redgem_engine PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${REDGEM_GENERATED_DIR})
target_link_libraries(redgem_engine PUBLIC raylib Jolt mruby_lib cgltf_headers nlohmann_json::nlohmann_json imgui_stack m)
if(WIN32)
  # mruby's socket and time gems use Windows' own libraries.
  target_link_libraries(redgem_engine PUBLIC ws2_32)
endif()
target_compile_options(redgem_engine PRIVATE -Wall -Wextra -Wpedantic -Wno-unused-parameter)
include(Hardening)
redgem_harden(redgem_engine)

if(ANDROID)
  # On Android the engine is a library the system loads into a native activity; it plays the game
  # packed in the same APK (see app/android_main.cpp).
  add_library(redgem SHARED app/android_main.cpp)
  target_link_libraries(redgem PRIVATE redgem_engine android log GLESv3)
  target_include_directories(redgem SYSTEM PRIVATE ${ANDROID_NDK}/sources/android/native_app_glue)
  target_link_options(redgem PRIVATE -u ANativeActivity_onCreate)
else()
  add_executable(redgem project/main.cpp)
  target_link_libraries(redgem PRIVATE redgem_engine)
  # On Windows it is what games ship as: a windowed program, with no console window behind it.
  set_target_properties(redgem PROPERTIES WIN32_EXECUTABLE ON)
endif()
target_compile_options(redgem PRIVATE -Wall -Wextra -Wpedantic)
redgem_harden(redgem)
redgem_harden_program(redgem)

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
