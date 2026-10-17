#pragma once

// What the renderer works out before it draws, as the values the graphics card takes: colors and
// matrices, the lights' arrays, the corners of a particle, the size of a
// picture. No window and no graphics card needed, so it can be tested. (Values handed to raylib
// keep raylib's types.) What is worth drawing is in visibility.hpp; the views captured before the
// picture are in capture_views.hpp.

#include <raylib.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "core/math.hpp"
#include "render/scene_lights.hpp"
#include "render/shaders/uniforms.hpp"
#include "scene/scene_desc.hpp"
#include "scene/transform.hpp"

namespace redgem::render_math {

inline constexpr int kMatrixFloats = 16;  // a 4x4 matrix
inline constexpr int kColorChannels = 4;  // red, green, blue, alpha
inline constexpr int kQuadCorners = 4;
inline constexpr float kOn = 1.0f;  // shader switches passed as floats
inline constexpr float kOff = 0.0f;
inline constexpr float kSquareCorners = 0.0f;  // roundness of a rectangle without rounded corners
inline constexpr float kFullyRound = 1.0f;     // roundness whose corners meet: the short side is a half circle
inline constexpr float kDiameterPerRadius = 2.0f;
inline constexpr Vector3 kUpAxis{0.0f, 1.0f, 0.0f};
inline constexpr Vector3 kForwardAxis{0.0f, 0.0f, 1.0f};

// --- Conversions -----------------------------------------------------------------------------

::Color to_raylib(redgem::Color color);
Vector3 to_raylib(Vec3 v);
Vec3 to_vec3(Vector3 v);
// A color's four channels as the 0..1 numbers shaders take.
std::array<float, kColorChannels> color_floats(::Color color);
// A glTF matrix (16 floats, column by column) as a raylib Matrix (whose fields are declared row by row).
Matrix from_column_major(const std::array<float, kMatrixFloats>& m);
Matrix model_matrix(const Transform& t);

// --- Small decisions -------------------------------------------------------------------------

// The graphics card's name out of the line raylib logs about it ("... Renderer: <name>"), or
// nothing for any other line.
std::optional<std::string> graphics_card_name(const std::string& trace_line);
// A target `factor` times smaller than `size` pixels, never smaller than one pixel.
int downscaled(int size, int factor);

// --- Lights ----------------------------------------------------------------------------------

// The point lights of a frame, laid out as the lit shader's arrays.
struct PointLightUniforms {
  Vector3 positions[shaders::kMaxPointLights] = {};
  Vector3 colors[shaders::kMaxPointLights] = {};
  float ranges[shaders::kMaxPointLights] = {};
  int count = 0;
};
// Lights beyond the shader's limit are left out.
PointLightUniforms pack_point_lights(const std::vector<PointLight>& lights);

// --- Particles -------------------------------------------------------------------------------

// The camera's right and up: what a square has to lie along to face it.
struct Facing {
  Vec3 right;
  Vec3 up;
};
Facing facing(Vec3 eye, Vec3 target, Vec3 up);
// Texture coordinates of a particle's corners, in the order particle_corners gives them.
inline constexpr float kQuadU[kQuadCorners] = {0.0f, 1.0f, 1.0f, 0.0f};
inline constexpr float kQuadV[kQuadCorners] = {1.0f, 1.0f, 0.0f, 0.0f};
// One particle: a square `size` meters across facing the camera, turned by `angle` radians
// around the line of sight.
std::array<Vec3, kQuadCorners> particle_corners(Vec3 position, float size, float angle, const Facing& view);
// What particles are drawn in batches of: the plain dots (a null picture) first, then each
// picture once, in the order given; and the batch of each of `pictures`. Both are filled anew.
template <typename Picture>
void picture_batches(const std::vector<const Picture*>& pictures, std::vector<const Picture*>& batches,
                     std::vector<std::size_t>& batch_of) {
  batches.assign(1, nullptr);
  batch_of.clear();
  for (const Picture* picture : pictures) {
    auto found = std::find(batches.begin(), batches.end(), picture);
    if (found == batches.end()) found = batches.insert(batches.end(), picture);
    batch_of.push_back(static_cast<std::size_t>(found - batches.begin()));
  }
}

// --- What games draw over the picture --------------------------------------------------------

struct Size {
  float width = 0.0f;
  float height = 0.0f;
};
// The size an image is drawn at, in pixels. A missing size (zero or less) follows the picture's
// own shape; with both missing it is drawn as large as it is.
Size image_size(int asked_width, int asked_height, int picture_width, int picture_height);
// How round a rectangle's corners are, as raylib counts it: 0 (square) to 1 (the short side is a
// half circle), from a corner radius in pixels.
float corner_roundness(float radius, float width, float height);

}  // namespace redgem::render_math
