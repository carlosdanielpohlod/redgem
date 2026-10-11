# Third-party dependencies, all pinned to exact release tags.
include(FetchContent)
include(ExternalProject)

set(REDGEM_RAYLIB_TAG  "5.5")
set(REDGEM_MRUBY_TAG   "3.4.0")
set(REDGEM_CGLTF_TAG   "v1.15")
set(REDGEM_JOLT_TAG    "v5.3.0")
set(REDGEM_DOCTEST_TAG "v2.4.11")
set(REDGEM_IMGUI_TAG   "v1.92.8-docking")
set(REDGEM_RLIMGUI_COMMIT  "1550009359ad975927f7f0e4a3f47e3f27123ea9")
set(REDGEM_IMGUIZMO_COMMIT "18cef5e031d8c6973d80284c67f60549fafd78c1")
set(REDGEM_JSON_VERSION "v3.12.0")

# --- X11 headers extracted locally by tools/setup/fetch_x11_headers.sh -------
set(REDGEM_LOCAL_X11_INCLUDE "${CMAKE_SOURCE_DIR}/.deps/x11/usr/include")
if(NOT WIN32 AND EXISTS "${REDGEM_LOCAL_X11_INCLUDE}/X11/extensions/Xrandr.h")
  message(STATUS "Using locally extracted X11 headers: ${REDGEM_LOCAL_X11_INCLUDE}")
  list(APPEND CMAKE_INCLUDE_PATH "${REDGEM_LOCAL_X11_INCLUDE}")
  include_directories(BEFORE SYSTEM "${REDGEM_LOCAL_X11_INCLUDE}")
endif()

# --- raylib ------------------------------------------------------------------
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
set(PLATFORM "Desktop" CACHE STRING "" FORCE)
set(OPENGL_VERSION "3.3" CACHE STRING "" FORCE)
# raylib bundles its own cgltf copy; the engine ships its own loader, so drop raylib's.
set(CUSTOMIZE_BUILD ON CACHE BOOL "" FORCE)
set(SUPPORT_FILEFORMAT_GLTF OFF CACHE BOOL "" FORCE)
# Scanned assets ship JPEG textures inside their .glb files.
set(SUPPORT_FILEFORMAT_JPG ON CACHE BOOL "" FORCE)
# HDRI skies (Radiance .hdr) for image based lighting.
set(SUPPORT_FILEFORMAT_HDR ON CACHE BOOL "" FORCE)
FetchContent_Declare(raylib
  GIT_REPOSITORY https://github.com/raysan5/raylib.git
  GIT_TAG ${REDGEM_RAYLIB_TAG}
  GIT_SHALLOW TRUE)

# --- cgltf (header only) -------------------------------------------------------
FetchContent_Declare(cgltf
  GIT_REPOSITORY https://github.com/jkuhlmann/cgltf.git
  GIT_TAG ${REDGEM_CGLTF_TAG}
  GIT_SHALLOW TRUE)

# --- Jolt Physics ----------------------------------------------------------------
set(TARGET_UNIT_TESTS OFF CACHE BOOL "" FORCE)
set(TARGET_HELLO_WORLD OFF CACHE BOOL "" FORCE)
set(TARGET_PERFORMANCE_TEST OFF CACHE BOOL "" FORCE)
set(TARGET_SAMPLES OFF CACHE BOOL "" FORCE)
set(TARGET_VIEWER OFF CACHE BOOL "" FORCE)
set(ENABLE_OBJECT_STREAM OFF CACHE BOOL "" FORCE)
set(CROSS_PLATFORM_DETERMINISTIC ON CACHE BOOL "" FORCE)
set(INTERPROCEDURAL_OPTIMIZATION OFF CACHE BOOL "" FORCE)
set(DEBUG_RENDERER_IN_DEBUG_AND_RELEASE OFF CACHE BOOL "" FORCE)
set(PROFILER_IN_DEBUG_AND_RELEASE OFF CACHE BOOL "" FORCE)
set(CPP_RTTI_ENABLED ON CACHE BOOL "" FORCE)
set(CPP_EXCEPTIONS_ENABLED ON CACHE BOOL "" FORCE)
if(WIN32)
  # Its warnings-as-errors trips on a newer compiler's notes; a dependency's warnings are not ours to fix.
  set(ENABLE_ALL_WARNINGS OFF CACHE BOOL "" FORCE)
endif()
FetchContent_Declare(JoltPhysics
  GIT_REPOSITORY https://github.com/jrouwe/JoltPhysics.git
  GIT_TAG ${REDGEM_JOLT_TAG}
  GIT_SHALLOW TRUE
  SOURCE_SUBDIR Build)

# --- doctest ------------------------------------------------------------------------
set(DOCTEST_NO_INSTALL ON CACHE BOOL "" FORCE)
FetchContent_Declare(doctest
  GIT_REPOSITORY https://github.com/doctest/doctest.git
  GIT_TAG ${REDGEM_DOCTEST_TAG}
  GIT_SHALLOW TRUE)

# --- mruby (built with its own rake build system) --------------------------------
FetchContent_Declare(mruby
  GIT_REPOSITORY https://github.com/mruby/mruby.git
  GIT_TAG ${REDGEM_MRUBY_TAG}
  GIT_SHALLOW TRUE)

# --- Editor UI: Dear ImGui (docking), its raylib backend and ImGuizmo -----------------
FetchContent_Declare(imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui.git
  GIT_TAG ${REDGEM_IMGUI_TAG}
  GIT_SHALLOW TRUE)
FetchContent_Declare(rlimgui
  GIT_REPOSITORY https://github.com/raylib-extras/rlImGui.git
  GIT_TAG ${REDGEM_RLIMGUI_COMMIT})
FetchContent_Declare(imguizmo
  GIT_REPOSITORY https://github.com/CedricGuillemet/ImGuizmo.git
  GIT_TAG ${REDGEM_IMGUIZMO_COMMIT})

# --- JSON (editor saves the glTF JSON chunk) --------------------------------------------
FetchContent_Declare(nlohmann_json
  URL https://github.com/nlohmann/json/releases/download/${REDGEM_JSON_VERSION}/json.tar.xz)

FetchContent_MakeAvailable(raylib cgltf JoltPhysics doctest mruby nlohmann_json)
# Skinned meshes are deformed on the GPU: raylib then uploads bone ids / weights and bone matrices
# (off in customized builds unless asked for).
target_compile_definitions(raylib PRIVATE RL_SUPPORT_MESH_GPU_SKINNING)
if(WIN32)
  # This Jolt release leans on <type_traits> arriving through other headers, which the Windows
  # toolchain's standard library no longer does.
  target_compile_options(Jolt PUBLIC "SHELL:-include type_traits")
endif()
# These ship no (usable) CMake project: only fetch their sources.
foreach(dep imgui rlimgui imguizmo)
  FetchContent_GetProperties(${dep})
  if(NOT ${dep}_POPULATED)
    FetchContent_Populate(${dep})
  endif()
endforeach()

add_library(imgui_stack STATIC
  ${imgui_SOURCE_DIR}/imgui.cpp
  ${imgui_SOURCE_DIR}/imgui_draw.cpp
  ${imgui_SOURCE_DIR}/imgui_tables.cpp
  ${imgui_SOURCE_DIR}/imgui_widgets.cpp
  ${imgui_SOURCE_DIR}/imgui_demo.cpp
  ${imgui_SOURCE_DIR}/misc/cpp/imgui_stdlib.cpp
  ${rlimgui_SOURCE_DIR}/rlImGui.cpp
  ${imguizmo_SOURCE_DIR}/src/ImGuizmo.cpp)
target_include_directories(imgui_stack SYSTEM PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/misc/cpp ${rlimgui_SOURCE_DIR} ${imguizmo_SOURCE_DIR}/src)
target_compile_definitions(imgui_stack PUBLIC NO_FONT_AWESOME)
target_link_libraries(imgui_stack PUBLIC raylib)

add_library(cgltf_headers INTERFACE)
target_include_directories(cgltf_headers SYSTEM INTERFACE "${cgltf_SOURCE_DIR}")

find_program(REDGEM_RAKE rake REQUIRED)
set(REDGEM_MRUBY_CONFIG "${CMAKE_SOURCE_DIR}/cmake/mruby_build_config.rb")
set(REDGEM_MRUBY_BUILD_DIR "${CMAKE_BINARY_DIR}/mruby")
# Built for this machine ("host"); a Windows build also cross-compiles it (see the build config).
set(REDGEM_MRUBY_TARGET host)
set(REDGEM_MRUBY_ENV "")
if(WIN32)
  set(REDGEM_MRUBY_TARGET windows)
  get_filename_component(REDGEM_MINGW_BIN "${CMAKE_CXX_COMPILER}" DIRECTORY)
  set(REDGEM_MRUBY_ENV "REDGEM_MINGW_BIN=${REDGEM_MINGW_BIN}")
endif()
set(REDGEM_MRUBY_LIB "${REDGEM_MRUBY_BUILD_DIR}/${REDGEM_MRUBY_TARGET}/lib/libmruby.a")
cmake_host_system_information(RESULT REDGEM_CPU_COUNT QUERY NUMBER_OF_LOGICAL_CORES)

add_custom_command(
  OUTPUT "${REDGEM_MRUBY_LIB}"
  COMMAND ${CMAKE_COMMAND} -E env
          "MRUBY_CONFIG=${REDGEM_MRUBY_CONFIG}"
          "MRUBY_BUILD_DIR=${REDGEM_MRUBY_BUILD_DIR}"
          ${REDGEM_MRUBY_ENV}
          ${REDGEM_RAKE} -j${REDGEM_CPU_COUNT} all
  WORKING_DIRECTORY "${mruby_SOURCE_DIR}"
  DEPENDS "${REDGEM_MRUBY_CONFIG}"
  COMMENT "Building mruby ${REDGEM_MRUBY_TAG}"
  VERBATIM)
add_custom_target(mruby_build DEPENDS "${REDGEM_MRUBY_LIB}")

add_library(mruby_lib STATIC IMPORTED GLOBAL)
set_target_properties(mruby_lib PROPERTIES
  IMPORTED_LOCATION "${REDGEM_MRUBY_LIB}"
  INTERFACE_INCLUDE_DIRECTORIES "${mruby_SOURCE_DIR}/include"
  INTERFACE_COMPILE_DEFINITIONS "MRB_USE_CXX_EXCEPTION")
add_dependencies(mruby_lib mruby_build)
