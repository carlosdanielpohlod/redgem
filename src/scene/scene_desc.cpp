#include "scene/scene_desc.hpp"

#include <algorithm>

namespace redgem {

void Bounds::include(Vec3 point) {
  if (!valid) {
    min = max = point;
    valid = true;
    return;
  }
  min = {std::min(min.x, point.x), std::min(min.y, point.y), std::min(min.z, point.z)};
  max = {std::max(max.x, point.x), std::max(max.y, point.y), std::max(max.z, point.z)};
}

int SceneDesc::find_node(const std::string& name) const {
  for (std::size_t i = 0; i < nodes.size(); ++i) {
    if (nodes[i].name == name) return static_cast<int>(i);
  }
  return kNoIndex;
}

}  // namespace redgem
