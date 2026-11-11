#include "render/local_shadows.hpp"

#include <algorithm>
#include <array>
#include <cmath>

#include "render/render_config.hpp"

namespace redgem::render_math {
namespace {

static_assert(kMaxLocalShadows == config::kMostLocalShadows, "the quality's most is the shader's");

// Where each face of a cube looks, in the lit shader's order (cubeFace in lit_shadows.hpp).
constexpr std::array<Vec3, kCubeFaces> kFaceForwards = {Vec3{1.0f, 0.0f, 0.0f}, Vec3{-1.0f, 0.0f, 0.0f},
                                                        Vec3{0.0f, 1.0f, 0.0f}, Vec3{0.0f, -1.0f, 0.0f},
                                                        Vec3{0.0f, 0.0f, 1.0f}, Vec3{0.0f, 0.0f, -1.0f}};
constexpr Vec3 kPoleUp{0.0f, 0.0f, 1.0f};  // the up of a view looking straight up or down
// Radians past a spot's cone its shadow's view reaches, so that the filter's taps at the cone's
// edge still fall inside it.
constexpr float kSpotShadowMargin = 0.05f;

// Whether a light's shadow can be taken: it casts one, and its tiles fit in what is left of the
// atlas from `next_tile` on.
bool fits(const PointLight& light, int next_tile) {
  return light.cast_shadow && next_tile + tiles_of(light) <= kLocalShadowTiles;
}

}  // namespace

int tiles_of(const PointLight& light) { return light.spot ? kSpotTiles : kCubeFaces; }

std::vector<LocalShadow> pick_local_shadows(const std::vector<PointLight>& lights, int budget) {
  std::vector<LocalShadow> picked;
  const auto most = static_cast<std::size_t>(std::clamp(budget, 0, kMaxLocalShadows));
  int next_tile = 0;
  for (std::size_t i = 0; i < lights.size(); ++i) {
    if (picked.size() == most) break;
    if (!fits(lights[i], next_tile)) continue;
    picked.push_back({i, next_tile, tiles_of(lights[i])});
    next_tile += tiles_of(lights[i]);
  }
  return picked;
}

std::vector<PointLight> shadowed_first(const std::vector<PointLight>& lights, std::vector<LocalShadow>& shadows) {
  std::vector<PointLight> ordered;
  ordered.reserve(lights.size());
  std::vector<bool> taken(lights.size(), false);
  for (std::size_t slot = 0; slot < shadows.size(); ++slot) {
    ordered.push_back(lights[shadows[slot].light]);
    taken[shadows[slot].light] = true;
    shadows[slot].light = slot;
  }
  for (std::size_t i = 0; i < lights.size(); ++i) {
    if (!taken[i]) ordered.push_back(lights[i]);
  }
  return ordered;
}

Vec3 shadow_up(Vec3 forward) { return std::fabs(forward.y) > kStraightUpOrDown ? kPoleUp : Vec3::up(); }

ShadowFace cube_face(int face) {
  const Vec3 forward = kFaceForwards[static_cast<std::size_t>(std::clamp(face, 0, kCubeFaces - 1))];
  return {forward, shadow_up(forward)};
}

ShadowFace spot_face(const PointLight& spot) { return {spot.axis, shadow_up(spot.axis)}; }

float spot_spread(const PointLight& spot) {
  return std::tan(std::min(spot.outer_cone + kSpotShadowMargin, kWidestSpotShadow));
}

ShadowTile shadow_tile(int tile) {
  const float size = 1.0f / static_cast<float>(kLocalTilesAcross);
  const int column = tile % kLocalTilesAcross;
  const int row = tile / kLocalTilesAcross;
  return {static_cast<float>(column) * size, static_cast<float>(row) * size, size};
}

}  // namespace redgem::render_math
