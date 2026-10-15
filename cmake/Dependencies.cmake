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
set(REDGEM_MBEDTLS_VERSION "3.6.7")
set(REDGEM_MBEDTLS_SHA256 "a7e8bcbec0e6f761b4af24f25677626b35f762f68eef79c08677a363212d11f6")

# --- X11 headers extracted locally by tools/setup/fetch_x11_headers.sh -------
set(REDGEM_LOCAL_X11_INCLUDE "${CMAKE_SOURCE_DIR}/.deps/x11/usr/include")
if(NOT WIN32 AND NOT ANDROID AND NOT APPLE AND EXISTS "${REDGEM_LOCAL_X11_INCLUDE}/X11/extensions/Xrandr.h")
  message(STATUS "Using locally extracted X11 headers: ${REDGEM_LOCAL_X11_INCLUDE}")
  list(APPEND CMAKE_INCLUDE_PATH "${REDGEM_LOCAL_X11_INCLUDE}")
  include_directories(BEFORE SYSTEM "${REDGEM_LOCAL_X11_INCLUDE}")
endif()

# --- raylib ------------------------------------------------------------------
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
if(ANDROID)
  # Phones and tablets: a native activity drawing with OpenGL ES 3.
  set(PLATFORM "Android" CACHE STRING "" FORCE)
  set(OPENGL_VERSION "ES 3.0" CACHE STRING "" FORCE)
else()
  set(PLATFORM "Desktop" CACHE STRING "" FORCE)
  set(OPENGL_VERSION "3.3" CACHE STRING "" FORCE)
endif()
# raylib bundles its own cgltf copy; the engine ships its own loader, so drop raylib's.
set(CUSTOMIZE_BUILD ON CACHE BOOL "" FORCE)
set(SUPPORT_FILEFORMAT_GLTF OFF CACHE BOOL "" FORCE)
# Scanned assets ship JPEG textures inside their .glb files.
set(SUPPORT_FILEFORMAT_JPG ON CACHE BOOL "" FORCE)
# HDRI skies (Radiance .hdr) for image based lighting.
set(SUPPORT_FILEFORMAT_HDR ON CACHE BOOL "" FORCE)
# On a Mac with no graphics card OpenGL can use, windows are drawn by Apple's software renderer
# (see the script).
set(REDGEM_RAYLIB_PATCH "")
if(APPLE)
  set(REDGEM_RAYLIB_PATCH PATCH_COMMAND ${CMAKE_COMMAND} -P ${CMAKE_SOURCE_DIR}/cmake/PatchGlfwForMac.cmake)
endif()
FetchContent_Declare(raylib
  GIT_REPOSITORY https://github.com/raysan5/raylib.git
  GIT_TAG ${REDGEM_RAYLIB_TAG}
  GIT_SHALLOW TRUE
  ${REDGEM_RAYLIB_PATCH})

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
if(WIN32 OR ANDROID OR APPLE)
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

# --- Mbed TLS (HTTPS for the games' HTTP; its release archive, which needs no code generation) ----
set(ENABLE_PROGRAMS OFF CACHE BOOL "" FORCE)
set(ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(MBEDTLS_FATAL_WARNINGS OFF CACHE BOOL "" FORCE)
set(USE_SHARED_MBEDTLS_LIBRARY OFF CACHE BOOL "" FORCE)
set(USE_STATIC_MBEDTLS_LIBRARY ON CACHE BOOL "" FORCE)
# Requests run on threads of their own, several at once (see the configuration file).
set(LINK_WITH_PTHREAD ON CACHE BOOL "" FORCE)
set(MBEDTLS_USER_CONFIG_FILE "${CMAKE_SOURCE_DIR}/cmake/mbedtls_user_config.h" CACHE FILEPATH "" FORCE)
FetchContent_Declare(mbedtls
  URL https://github.com/Mbed-TLS/mbedtls/releases/download/mbedtls-${REDGEM_MBEDTLS_VERSION}/mbedtls-${REDGEM_MBEDTLS_VERSION}.tar.bz2
  URL_HASH SHA256=${REDGEM_MBEDTLS_SHA256})

FetchContent_MakeAvailable(raylib cgltf JoltPhysics doctest mruby nlohmann_json mbedtls)
# Skinned meshes are deformed on the GPU: raylib then uploads bone ids / weights and bone matrices
# (off in customized builds unless asked for).
target_compile_definitions(raylib PRIVATE RL_SUPPORT_MESH_GPU_SKINNING)
if(ANDROID)
  # raylib asks for OpenGL ES 3 but calls one of its functions by the old extension's name (and
  # links against ES 2: the engine's own target adds ES 3, see src/CMakeLists.txt).
  target_compile_definitions(raylib PRIVATE glDrawBuffersEXT=glDrawBuffers)
endif()
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

# (Without imgui_demo.cpp: the editor shows none of the library's demonstration windows.)
add_library(imgui_stack STATIC
  ${imgui_SOURCE_DIR}/imgui.cpp
  ${imgui_SOURCE_DIR}/imgui_draw.cpp
  ${imgui_SOURCE_DIR}/imgui_tables.cpp
  ${imgui_SOURCE_DIR}/imgui_widgets.cpp
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
elseif(ANDROID)
  # One mruby per kind of processor (ANDROID_ABI), built with the NDK's compiler for it.
  set(REDGEM_MRUBY_TARGET android-${ANDROID_ABI})
  get_filename_component(REDGEM_NDK_BIN "${CMAKE_CXX_COMPILER}" DIRECTORY)
  set(REDGEM_MRUBY_ENV "REDGEM_NDK_BIN=${REDGEM_NDK_BIN}" "REDGEM_ANDROID_ABI=${ANDROID_ABI}"
                       "REDGEM_ANDROID_TARGET=${CMAKE_CXX_COMPILER_TARGET}")
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
