#pragma once

// What the renderer works out before it draws: where things are, what is worth drawing, where the
// sun's shadow map goes, the corners of a particle. No window and no graphics card needed, so it
// can be tested. (Values handed to raylib keep raylib's types.)

#include <raylib.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "core/math.hpp"
#include "render/culling.hpp"
#include "render/environment.hpp"
#include "render/shaders.hpp"
#include "scene/particles.hpp"
#include "scene/scene.hpp"
#include "scene/scene_desc.hpp"

namespace redgem::render_math {

inline constexpr int kMatrixFloats = 16;  // a 4x4 matrix
inline constexpr int kColorChannels = 4;  // red, green, blue, alpha
inline constexpr int kQuadCorners = 4;
inline constexpr int kCubeFaces = 6;
inline constexpr float kOn = 1.0f;  // shader switches passed as floats
inline constexpr float kOff = 0.0f;
inline constexpr float kNoCutoff = 0.0f;  // an alpha test that lets everything through
inline constexpr float kNoWind = 0.0f;
inline constexpr float kSquareCorners = 0.0f;  // roundness of a rectangle without rounded corners
inline constexpr float kFullyRound = 1.0f;     // roundness whose corners meet: the short side is a half circle
inline constexpr float kDiameterPerRadius = 2.0f;
inline constexpr float kBlendAsCutoutThreshold = 0.35f;  // alpha below this is cut away on "blend" textures
// Parts that look smaller than this (radius over distance from the camera, radians) are skipped:
// a car's bolts at 50 m. Shadows of small things disappear sooner.
inline constexpr float kMinVisibleAngle = 0.0025f;
inline constexpr float kMinShadowAngle = 0.008f;
inline constexpr float kSkinnedBoundsMargin = 1.5f;  // animated limbs reach beyond the bind pose
// Sun shadows: one map covering a square of the world around where the camera looks.
inline constexpr int kShadowMapSize = 4096;    // texels along each side
inline constexpr float kShadowExtent = 90.0f;  // meters covered by the map (side of the square)
inline constexpr float kShadowFocusAhead =
    0.3f;  // the square is centered this fraction of its side ahead of the camera
inline constexpr float kShadowSunDistance =
    300.0f;  // meters back the sun's viewpoint sits (casters behind the camera count)
inline constexpr float kMinSunHeight =
    0.02f;  // below this (y of the direction to the sun) it is treated as set: no shadows
inline constexpr float kNoShadows = 0.0f;
inline constexpr float kNearlyVertical = 0.999f;  // |y| of a sun direction beyond which Y cannot be its "up"
inline constexpr Vector3 kUpAxis{0.0f, 1.0f, 0.0f};
inline constexpr Vector3 kForwardAxis{0.0f, 0.0f, 1.0f};
// Reflections: the surroundings of the camera, captured one cube face per frame.
inline constexpr float kReflectionEyeHeight = 1.2f;  // lowest world height (y, meters) the capture is taken from
inline constexpr float kReflectionsOn = 1.0f;
inline constexpr float kReflectionsOff = 0.0f;

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

// --- Materials -------------------------------------------------------------------------------

// How a material's see-through parts are drawn. There is no sorted transparency: cut-out
// materials (and textured "blend" ones, i.e. leaves and fences) are drawn with an alpha test.
struct AlphaTest {
  float cutoff = kNoCutoff;  // alpha below this is not drawn
  float sway = kNoWind;      // foliage sways in the wind; everything else stays still
  bool two_sided = false;    // drawn without backface culling
};
AlphaTest alpha_test(const MaterialDesc& material, float wind);

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

// --- Where things are ------------------------------------------------------------------------

// Where every object is in the world, by object id (destroyed objects keep what they had).
// Parents are created before their children, so a parent's place is reused instead of walking
// each object's chain of parents again.
void world_transforms(const Scene& scene, std::vector<Transform>& worlds);

// --- What is worth drawing -------------------------------------------------------------------

struct BoundingSphere {
  Vec3 center;
  float radius = 0.0f;
};
// The sphere around a mesh placed in the world.
BoundingSphere world_sphere(const Bounds& bounds, const Transform& world);
// A skinned mesh is placed by its skeleton, not by its own node: the sphere follows the first
// bone, with room for limbs that swing out of the bind pose's bounds.
BoundingSphere skinned_sphere(const Bounds& bounds, const Matrix& first_bone);
// The smallest angle (radians) a part may look to still be drawn: `asked` when positive, or the
// usual one of the pass.
float smallest_angle_drawn(float asked, bool depth_pass);
// False for objects that are never drawn: destroyed, hidden, without a (known) mesh, or prefabs
// outside the editor.
bool has_something_to_draw(const GameObject& object, bool show_prefabs, std::size_t mesh_count);

// One view of the scene: what it can see and who judges how big things look.
struct View {
  Frustum frustum;
  Vec3 viewer;
  float min_angle = 0.0f;
};
// True when the sphere is in the view, looks big enough and is within [from, to) meters.
bool worth_drawing(const View& view, const BoundingSphere& sphere, float from, float to);

// --- Sun shadows -----------------------------------------------------------------------------

// How dark the sun's shadows are: `asked`, or none once the sun has set.
float sun_shadow_strength(Vec3 to_sun, float asked);
// `value` moved down to a whole number of `step`s.
float snapped_down(float value, float step);
// The axis the shadow map's vertical follows. A sun straight overhead has no sideways "up":
// another axis gives the map its orientation.
Vector3 shadow_map_vertical(Vector3 sun_axis);
// The sun's viewpoint for the shadow map.
struct SunView {
  Vector3 position;
  Vector3 target;
  Vector3 up;
};
// Looks at a point ahead of the camera, moved in whole texels only: otherwise shadow edges
// shimmer as the camera moves.
SunView fit_sun_view(Vector3 camera_position, Vector3 camera_target, Vec3 to_sun);

// --- Reflections -----------------------------------------------------------------------------

// The view along one face of a cube around the camera.
struct CubeFace {
  Vector3 forward;
  Vector3 up;
};
inline constexpr CubeFace kCubeFaceViews[kCubeFaces] = {
    {{1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}}, {{-1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}},
    {{0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f}}, {{0.0f, 0.0f, -1.0f}, {0.0f, 1.0f, 0.0f}},
    {{0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}}, {{0.0f, -1.0f, 0.0f}, {0.0f, 0.0f, -1.0f}},
};
// The right and up of a face's picture, at right angles to its forward.
struct FaceAxes {
  Vector3 right;
  Vector3 up;
};
FaceAxes face_axes(const CubeFace& face);
// Where the surroundings are captured from: the camera, but not from below a height (a camera
// near the floor would mostly capture the floor).
Vector3 reflection_eye(Vector3 camera_position);
// The face captured after `face`: six frames make a full turn.
int face_after(int face);
// Reflections switch on once all six faces exist.
float reflection_strength(int faces_captured);

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
// picture once, in the order given.
template <typename Picture>
std::vector<const Picture*> picture_batches(const std::vector<const Picture*>& pictures) {
  std::vector<const Picture*> batches{nullptr};
  for (const Picture* picture : pictures) {
    if (std::find(batches.begin(), batches.end(), picture) == batches.end()) batches.push_back(picture);
  }
  return batches;
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
