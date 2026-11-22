#include "scene/ground/ground_weights.hpp"

#include "scene/ground/ground_survey.hpp"

namespace redgem {
namespace {

constexpr float kMapStart = 0.0f;
constexpr float kMapEnd = 1.0f;

bool on_the_map(SplatSpot spot) {
  return spot.u >= kMapStart && spot.u <= kMapEnd && spot.v >= kMapStart && spot.v <= kMapEnd;
}

}  // namespace

std::optional<LayerWeights> ground_weights_at(const TerrainLayers& layers, const MeshDesc& mesh, const Transform& world,
                                              Vec3 point) {
  if (layers.splat.empty()) return std::nullopt;
  const Vec3 local = relative_to(world, Transform{point}).position;
  const SplatSpot spot = splat_spot(splat_area(mesh.bounds), local.x, local.z);
  if (!on_the_map(spot)) return std::nullopt;
  return splat_weights_at(layers.splat, spot.u, spot.v);
}

}  // namespace redgem
