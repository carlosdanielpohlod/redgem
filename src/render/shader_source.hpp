#pragma once

// Prepares the GLSL in render/shaders/ for the graphics card that will compile it. Text only: no
// window needed, so it can be tested.

#include <string>

namespace redgem::shaders {

// The line every shader in render/shaders/ starts from: desktop OpenGL 3.3.
inline constexpr const char* kDesktopVersionLine = "#version 330";
// What phones (OpenGL ES 3) want instead: their own version, and how precise numbers are (a
// picture's light is brighter than white: a sampler of low precision would clip it).
inline constexpr const char* kMobileHeader =
    "#version 300 es\nprecision highp float;\nprecision highp int;\nprecision highp sampler2D;\n"
    "precision highp samplerCube;";
// What the shaders and the engine agree on, defined for every shader from the engine's own
// numbers: how wide the texture of the frame's matrices is, and the size of the lights' arrays.
inline constexpr const char* kTexelRowName = "TEXELS_PER_ROW";
inline constexpr const char* kPointLightLimitName = "MAX_POINT_LIGHTS";

// `source` with its version line replaced by `header` followed by those two numbers. What comes
// before the version line goes (ES wants the version on the very first line). A source without
// the version line is returned as it is.
std::string with_header(const std::string& source, const std::string& header, int texels_per_row, int max_point_lights);

// The shaders are written for desktop OpenGL 3.3. Phones speak OpenGL ES 3: the same language
// with another first line, and no say on how precise numbers are unless told. Either way the
// shader learns here how wide a row of the frame's matrices is and how many point lights a
// surface may have.
std::string for_this_gpu(const char* source);

}  // namespace redgem::shaders
