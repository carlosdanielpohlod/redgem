#include "render/shadow_cascades.hpp"

#include <raymath.h>

#include <algorithm>
#include <cmath>

namespace redgem::render_math {
namespace {

constexpr int kTilesAcross = 2;  // cascades share the map as a 2 x 2 grid of tiles
constexpr float kTileSize = 1.0f / static_cast<float>(kTilesAcross);
constexpr float kNearestSlice = 0.1f;  // meters: slices deepen from here (a camera's near plane may be closer)

Cascade single_map(const CascadeCamera& camera, Vec3 to_sun) {
  return {fit_sun_view(camera.position, camera.target, to_sun), kShadowExtent, {0.0f, 0.0f}, 1.0f};
}

Cascade slice_cascade(const CascadeCamera& camera, Vec3 to_sun, int index, float from, float to, int count) {
  const SliceSphere sphere = slice_sphere(camera, from, to);
  const float extent = kTwo * sphere.radius;
  const Vector3 center = Vector3Add(camera.position, Vector3Scale(camera.forward, sphere.center));
  const float texel = extent / static_cast<float>(cascade_texels(count));
  const int column = index % kTilesAcross;
  const int row = index / kTilesAcross;
  const Vector2 corner{static_cast<float>(column) * kTileSize, static_cast<float>(row) * kTileSize};
  return {snapped_sun_view(center, to_sun, texel), extent, corner, kTileSize};
}

}  // namespace

CascadeEnds cascade_ends(int count, float near, float distance) {
  near = std::max(near, kNearestSlice);
  CascadeEnds ends{};
  for (int i = 1; i <= count; ++i) {
    const float share = static_cast<float>(i) / static_cast<float>(count);
    const float even = near + (distance - near) * share;
    const float deeper = near * std::pow(distance / near, share);
    ends[static_cast<std::size_t>(i - 1)] = kCascadeSplitBlend * deeper + (1.0f - kCascadeSplitBlend) * even;
  }
  ends[static_cast<std::size_t>(count - 1)] = distance;
  return ends;
}

SliceSphere slice_sphere(const CascadeCamera& camera, float from, float to) {
  // How far a corner of the view lies from its middle, per meter along it.
  const float corner = camera.tan_half_fov * std::sqrt(1.0f + camera.aspect * camera.aspect);
  const float spread = corner * corner;
  const float center = std::min((from + to) * kHalf * (1.0f + spread), to);
  return {center, std::sqrt((to - center) * (to - center) + to * to * spread)};
}

Cascades fit_cascades(const CascadeCamera& camera, Vec3 to_sun, int count, float distance) {
  Cascades fitted;
  fitted.count = std::clamp(count, 1, kMaxShadowCascades);
  if (fitted.count == 1) {
    fitted.cascades[0] = single_map(camera, to_sun);
    return fitted;
  }
  const CascadeEnds ends = cascade_ends(fitted.count, camera.near, distance);
  float from = camera.near;
  for (int i = 0; i < fitted.count; ++i) {
    const float to = ends[static_cast<std::size_t>(i)];
    fitted.cascades[static_cast<std::size_t>(i)] = slice_cascade(camera, to_sun, i, from, to, fitted.count);
    from = to;
  }
  return fitted;
}

int cascade_texels(int count) { return count > 1 ? kShadowMapSize / kTilesAcross : kShadowMapSize; }

float cascade_scale(const Cascade& cascade, int count) {
  const float texel = cascade.extent / static_cast<float>(cascade_texels(count));
  return texel / (kShadowExtent / static_cast<float>(kShadowMapSize));
}

}  // namespace redgem::render_math
