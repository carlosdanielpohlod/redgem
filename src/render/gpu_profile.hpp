#pragma once

// What differs between the graphics APIs the engine draws with: desktop OpenGL 3.3 on computers,
// OpenGL ES 3 on phones. Everything else in the renderer is the same code for both; this is the
// one place that asks which it is.

#include "scene/scene_config.hpp"
#include "render/shader_source.hpp"

namespace redgem::gpu_profile {

#if defined(GRAPHICS_API_OPENGL_ES3)
// What every shader starts with: phones want their own version, and to be told how precise numbers are.
inline constexpr const char* kShaderHeader = shaders::kMobileHeader;
// How many joints the shader's bone array holds. (Phones allow a shader fewer values than
// computers do: fewer bones fit.)
inline constexpr int kMaxSkinJoints = 48;
#else
inline constexpr const char* kShaderHeader = shaders::kDesktopVersionLine;
inline constexpr int kMaxSkinJoints = 64;
#endif

// A scene is never loaded with more joints than it may have, whatever the graphics card holds.
static_assert(kMaxSkinJoints <= config::kMaxSkinJoints, "a shader cannot hold more joints than a skeleton has");

}  // namespace redgem::gpu_profile
