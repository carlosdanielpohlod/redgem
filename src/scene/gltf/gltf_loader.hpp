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

// Loads a .glb/.gltf file with cgltf, including node extras (Blender custom properties).
// `max_skin_joints` is how many joints of a skeleton whoever draws the scene can move (a
// graphics card's shader holds so many): vertices of the joints past it follow the last one
// kept, with a warning. Nobody can use more than a scene's skeleton may have.
LoadResult load_gltf_scene(const std::string& path, int max_skin_joints = config::kMaxSkinJoints);

}  // namespace redgem
