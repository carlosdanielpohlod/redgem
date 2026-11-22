#pragma once

// The ground layers' shares under a point of the world: what a footstep or a scattered plant asks
// of a terrain with ground layers.

#include <optional>

#include "core/math.hpp"
#include "scene/ground/splat_map.hpp"
#include "scene/scene_desc.hpp"
#include "scene/transform.hpp"

namespace redgem {

// Each layer's share under `point` (in the world), seen from above, for a terrain drawn with `mesh`
// and placed at `world`. Nothing past the edges of its map, or for a terrain without weights.
std::optional<LayerWeights> ground_weights_at(const TerrainLayers& layers, const MeshDesc& mesh, const Transform& world,
                                              Vec3 point);

}  // namespace redgem
