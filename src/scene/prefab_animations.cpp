#include "scene/prefab_import.hpp"

#include <algorithm>
#include <stdexcept>

namespace redgem::prefabs {
namespace {
void register_animations(SceneDesc& target, Source& source, const Instance& instance) {
  for (const auto& animation : source.scene.animations) {
    const auto name = instance.file + "::" + animation.name;
    const bool collision = std::any_of(target.animations.begin(), target.animations.end(),
                                       [&name](const auto& clip) { return clip.name == name; });
    if (collision) throw std::runtime_error("prefab animation name collision: " + name);
    source.animations.push_back(target.animations.size());
    target.animations.push_back({name, animation.duration, {}});
  }
}
}  // namespace
void append_animations(SceneDesc& target, Source& source, const Instance& instance) {
  if (source.animations.empty()) register_animations(target, source, instance);
  for (std::size_t i = 0; i < source.scene.animations.size(); ++i) {
    for (auto channel : source.scene.animations[i].channels) {
      channel.node = instance_node(channel.node, instance);
      target.animations[source.animations[i]].channels.push_back(std::move(channel));
    }
  }
}
}  // namespace redgem::prefabs
