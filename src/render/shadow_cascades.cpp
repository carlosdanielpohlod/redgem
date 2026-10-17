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

// One slice of the view: which cascade it is and how far along the view it goes, meters.
struct Slice {
  int index = 0;
  float from = 0.0f;
  float to = 0.0f;
};

Cascade slice_cascade(const CascadeCamera& camera, Vec3 to_sun, const Slice& slice, const Cascades& fitted) {
  const SliceSphere sphere = slice_sphere(camera, slice.from, slice.to);
  const float extent = kTwo * sphere.radius;
  const Vector3 center = Vector3Add(camera.position, Vector3Scale(camera.forward, sphere.center));
  const float texel = extent / static_cast<float>(cascade_texels(fitted.count));
  const int column = slice.index % kTilesAcross;
  const int row = slice.index / kTilesAcross;
  const Vector2 corner{static_cast<float>(column) * kTileSize, static_cast<float>(row) * kTileSize};
  return {snapped_sun_view(center, to_sun, texel, fitted.sun_distance), extent, corner, kTileSize};
}

// Far enough back for the sun to see every caster over the whole shadowed view: its sphere holds
// every slice's.
float sun_distance_for(const CascadeCamera& camera, float distance) {
  return std::max(kShadowSunDistance, slice_sphere(camera, camera.near, distance).radius + kCastersAbove);
}

// A cascade for each slice of the view, up to `distance` meters.
void fit_slices(Cascades& fitted, const CascadeCamera& camera, Vec3 to_sun, float distance) {
  const CascadeEnds ends = cascade_ends(fitted.count, camera.near, distance);
  float from = camera.near;
  for (int i = 0; i < fitted.count; ++i) {
    const float to = ends[static_cast<std::size_t>(i)];
    fitted.cascades[static_cast<std::size_t>(i)] = slice_cascade(camera, to_sun, {i, from, to}, fitted);
    from = to;
  }
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
  fitted.sun_distance = sun_distance_for(camera, distance);
  fit_slices(fitted, camera, to_sun, distance);
  return fitted;
}

int cascade_texels(int count) { return count > 1 ? kShadowMapSize / kTilesAcross : kShadowMapSize; }

float cascade_scale(const Cascade& cascade, int count) {
  const float texel = cascade.extent / static_cast<float>(cascade_texels(count));
  return texel / (kShadowExtent / static_cast<float>(kShadowMapSize));
}

}  // namespace redgem::render_math
