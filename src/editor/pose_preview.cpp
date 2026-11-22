#include "editor/pose_preview.hpp"

#include <algorithm>

#include "editor/node_tree.hpp"
#include "scene/animation/animation.hpp"

namespace redgem::pose_preview {
namespace {

constexpr const char* kUnnamedClip = "Clip ";

void add_skin_joints(const SceneDesc& desc, int node, std::set<int>& nodes) {
  const int skin = desc.nodes[static_cast<std::size_t>(node)].skin;
  if (!is_index_of(skin, desc.skins.size())) return;
  for (int joint : desc.skins[static_cast<std::size_t>(skin)].joints) {
    if (joint != kNoIndex) nodes.insert(joint);
  }
}

bool moves_any(const AnimationDesc& clip, const std::set<int>& nodes) {
  return std::any_of(clip.channels.begin(), clip.channels.end(),
                     [&nodes](const AnimationChannel& channel) { return nodes.count(channel.node) > 0; });
}

bool has_object_and_clip(const SceneDesc& desc, const Pose& pose) {
  return is_index_of(pose.object, desc.nodes.size()) && is_index_of(pose.clip, desc.animations.size());
}

}  // namespace

std::set<int> posed_nodes(const SceneDesc& desc, int object) {
  std::set<int> nodes;
  if (!is_index_of(object, desc.nodes.size())) return nodes;
  for (int node : node_tree::subtree(desc.nodes, object)) {
    nodes.insert(node);
    add_skin_joints(desc, node, nodes);
  }
  return nodes;
}

std::vector<int> clips_moving(const SceneDesc& desc, int object) {
  const std::set<int> nodes = posed_nodes(desc, object);
  std::vector<int> clips;
  for (std::size_t clip = 0; clip < desc.animations.size(); ++clip) {
    if (moves_any(desc.animations[clip], nodes)) clips.push_back(static_cast<int>(clip));
  }
  return clips;
}

std::string clip_name(const SceneDesc& desc, int clip) {
  const std::string& name = desc.animations.at(static_cast<std::size_t>(clip)).name;
  return name.empty() ? kUnnamedClip + std::to_string(clip) : name;
}

float within_clip(const AnimationDesc& clip, float seconds) { return std::clamp(seconds, 0.0f, clip.duration); }

bool fits(const SceneDesc& desc, const Pose& pose) {
  if (!has_object_and_clip(desc, pose)) return false;
  return moves_any(desc.animations[static_cast<std::size_t>(pose.clip)], posed_nodes(desc, pose.object));
}

void show(const SceneDesc& desc, const Pose& pose, Scene& scene) {
  if (!fits(desc, pose)) return;
  const AnimationDesc& clip = desc.animations[static_cast<std::size_t>(pose.clip)];
  const std::set<int> nodes = posed_nodes(desc, pose.object);
  PoseOptions options;
  options.loop = false;
  options.offsets = false;
  options.wanted = [&nodes](ObjectId id) { return nodes.count(id) > 0; };
  apply_animation(clip, within_clip(clip, pose.seconds), scene, options);
}

}  // namespace redgem::pose_preview
