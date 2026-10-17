#pragma once

#include <string>
#include <vector>

#include "scene/scene_config.hpp"
#include "scene/scene_desc.hpp"

namespace redgem {

struct LoadResult {
  bool ok = false;
  std::string error;
  std::vector<std::string> warnings;
  SceneDesc scene;
};

// Loads a .glb/.gltf file with cgltf, including node extras (Blender custom properties). A
// skeleton keeps its first config::kMaxSkinJoints joints, with a warning when it has more; a
// vertex that names a joint its skeleton does not have follows the skeleton's first.
LoadResult load_gltf_scene(const std::string& path);

}  // namespace redgem
