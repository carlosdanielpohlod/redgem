#pragma once

// A running game object's ground layers and mesh, for the natives that ask of the ground under a
// point (Terrain, Scatter).

#include <mruby.h>

#include "scene/scene.hpp"
#include "scene/ground/terrain_layers.hpp"
#include "scene/scene_desc.hpp"

namespace redgem {

// The ground layers of the object of the scene's file that `object` is (or copies); null when it
// has none.
TerrainLayers* terrain_layers_of(mrb_state* mrb, const GameObject& object);
// The mesh the object is drawn with; null when it has none in the scene.
const MeshDesc* terrain_mesh_of(mrb_state* mrb, const GameObject& object);

}  // namespace redgem
