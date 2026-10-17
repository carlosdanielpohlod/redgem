#pragma once

// What differs between the graphics APIs the engine draws with: desktop OpenGL 3.3 on computers,
// OpenGL ES 3 on phones. Everything else in the renderer is the same code for both; this is the
// one place that asks which it is.

#include "render/shader_source.hpp"

namespace redgem::gpu_profile {

#if defined(GRAPHICS_API_OPENGL_ES3)
// What every shader starts with: phones want their own version, and to be told how precise numbers are.
inline constexpr const char* kShaderHeader = shaders::kMobileHeader;
#else
inline constexpr const char* kShaderHeader = shaders::kDesktopVersionLine;
#endif

}  // namespace redgem::gpu_profile
