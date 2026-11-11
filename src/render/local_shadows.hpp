#pragma once

// Shadows of point and spot lights. A few of the lights that shine on a frame cast them, each into
// tiles of one depth atlas: a spot into one tile (its view down the cone), a point into six (the
// faces of a cube around it). Which lights, which tiles, and how each tile looks out are worked out
// here, with no window and no graphics card, so they can be tested; the lit shader works out the
// same views (LOCAL_* in lit_shadows.hpp).

#include <cstddef>
#include <vector>

#include "core/math.hpp"
#include "render/scene_lights.hpp"

namespace redgem::render_math {

inline constexpr int kMaxLocalShadows = 4;          // MAX_LOCAL_SHADOWS in the lit shader
inline constexpr int kLocalShadowAtlasSize = 2048;  // texels along each side of the atlas
inline constexpr int kLocalTilesAcross = 4;         // LOCAL_TILES_ACROSS in the lit shader
inline constexpr int kLocalShadowTiles = kLocalTilesAcross * kLocalTilesAcross;
inline constexpr int kCubeFaces = 6;              // the tiles of a point light's shadow
inline constexpr int kSpotTiles = 1;              // the tiles of a spot light's shadow
inline constexpr float kLocalShadowNear = 0.05f;  // meters from the light depth starts
// Radians off its axis a spot's shadow reaches at most: a perspective view cannot open to a right
// angle. Past it, a wider spot lights without shadows.
inline constexpr float kWidestSpotShadow = 1.3f;
// A view looking this straight up or down (the cosine with the vertical) takes +Z as its up, not
// +Y (LOCAL_POLE in the lit shader).
inline constexpr float kStraightUpOrDown = 0.99f;

// One light's shadow this frame: which of the frame's lights casts it, and its tiles in the atlas.
struct LocalShadow {
  std::size_t light = 0;
  int first_tile = 0;
  int tiles = kSpotTiles;
};

int tiles_of(const PointLight& light);  // kSpotTiles or kCubeFaces

// The lights that cast shadows this frame, taken from `lights` in their order (the frame's lights,
// those that matter most to the viewer first): those whose `cast_shadow` is on, at most `budget`,
// as long as their tiles fit in the atlas (a point that no longer fits leaves room for a spot).
std::vector<LocalShadow> pick_local_shadows(const std::vector<PointLight>& lights, int budget);

// The lights in the order the lit shader takes them: those that cast a shadow first (the lit
// shader finds a light's shadow by its place), in the order of `shadows`, then the others in
// theirs. Each shadow is pointed at its light's new place.
std::vector<PointLight> shadowed_first(const std::vector<PointLight>& lights, std::vector<LocalShadow>& shadows);

// How one tile looks out from its light: along `forward`, with `up` as its vertical.
struct ShadowFace {
  Vec3 forward;
  Vec3 up;
};
Vec3 shadow_up(Vec3 forward);
// Face `face` of a cube around a point light: +X, -X, +Y, -Y, +Z, -Z.
ShadowFace cube_face(int face);
ShadowFace spot_face(const PointLight& spot);
// The tangent of half a spot's shadow view, wide enough for its cone (a cube face's is 1).
float spot_spread(const PointLight& spot);

// A tile of the atlas: its corner and size in the atlas's texture coordinates.
struct ShadowTile {
  float x = 0.0f;
  float y = 0.0f;
  float size = 0.0f;
};
ShadowTile shadow_tile(int tile);

}  // namespace redgem::render_math
