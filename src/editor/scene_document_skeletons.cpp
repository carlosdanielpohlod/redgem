// Part of SceneDocument: skeletons and animations point at nodes, and nodes get renumbered.
// Their references are kept as source node numbers and translated whenever the nodes change.

#include "editor/scene_document.hpp"

#include "core/log.hpp"
#include "editor/gltf_json.hpp"

namespace redgem {
namespace {

using namespace gltf_json;

// The current index of each source node of a list (see current_index).
std::vector<int> current_indices(const std::vector<int>& current, const std::vector<int>& sources) {
  std::vector<int> indices;
  indices.reserve(sources.size());
  for (const int source : sources) indices.push_back(current_index(current, source));
  return indices;
}

// A source node no current node was found for yet.
bool unclaimed(const std::vector<int>& current, int source) {
  return in_range(source, current.size()) && current[source] == kNoIndex;
}

bool points_at_nodes(const SceneDesc& desc) { return !desc.skins.empty() || !desc.animations.empty(); }

}  // namespace

std::vector<int> SceneDocument::current_node_indices() const {
  std::vector<int> current(source_nodes_.size(), kNoIndex);
  for (int i = 0; i < node_count(); ++i) {
    const int source = desc_.nodes[i].source_node;
    // A duplicated node shares its source with the original: the first one keeps the skeleton.
    if (unclaimed(current, source)) current[source] = i;
  }
  return current;
}

int SceneDocument::source_of(int node) const { return node == kNoIndex ? kNoIndex : desc_.nodes.at(node).source_node; }

void SceneDocument::remember_node_references(std::size_t first_skin, std::size_t first_animation) {
  for (std::size_t s = first_skin; s < desc_.skins.size(); ++s) remember_joints(desc_.skins[s]);
  for (std::size_t a = first_animation; a < desc_.animations.size(); ++a) remember_channels(desc_.animations[a]);
}

void SceneDocument::remember_joints(const SkinDesc& skin) {
  std::vector<int> sources;
  sources.reserve(skin.joints.size());
  for (const int joint : skin.joints) sources.push_back(source_of(joint));
  skin_joint_sources_.push_back(std::move(sources));
}

void SceneDocument::remember_channels(const AnimationDesc& animation) {
  std::vector<int> sources;
  sources.reserve(animation.channels.size());
  for (const AnimationChannel& channel : animation.channels) sources.push_back(source_of(channel.node));
  channel_node_sources_.push_back(std::move(sources));
}

void SceneDocument::refresh_node_references() {
  if (!points_at_nodes(desc_)) return;
  const std::vector<int> current = current_node_indices();
  for (std::size_t s = 0; s < desc_.skins.size(); ++s) {
    desc_.skins[s].joints = current_indices(current, skin_joint_sources_[s]);
  }
  for (std::size_t a = 0; a < desc_.animations.size(); ++a) {
    const std::vector<int> nodes = current_indices(current, channel_node_sources_[a]);
    std::vector<AnimationChannel>& channels = desc_.animations[a].channels;
    for (std::size_t c = 0; c < channels.size(); ++c) channels[c].node = nodes[c];
  }
}

void SceneDocument::sync_skins_and_animations(nlohmann::json& json) const {
  const std::vector<int> current = current_node_indices();
  sync_skins(json, current);
  sync_animations(json, current);
}

// A skin that lost a joint (a deleted bone, or a whole deleted or undone character) is left out,
// and the meshes that used it are saved rigid; the others keep working.
void SceneDocument::sync_skins(nlohmann::json& json, const std::vector<int>& current) const {
  nlohmann::json skins = nlohmann::json::array();
  std::vector<int> written(source_skins_.size(), kNoIndex);  // skin index -> its index in the file
  for (std::size_t s = 0; s < source_skins_.size(); ++s) {
    std::optional<nlohmann::json> skin = skin_with_current_nodes(source_skins_[s], current);
    if (!skin) continue;
    written[s] = static_cast<int>(skins.size());
    skins.push_back(std::move(*skin));
  }
  sync_node_skins(json, written);
  set_unless_empty(json, kSkins, skins);
}

// Channels of deleted nodes are dropped.
void SceneDocument::sync_animations(nlohmann::json& json, const std::vector<int>& current) const {
  nlohmann::json animations = nlohmann::json::array();
  for (const nlohmann::json& animation : source_animations_) {
    std::optional<nlohmann::json> kept = animation_with_current_nodes(animation, current);
    if (kept) animations.push_back(std::move(*kept));
  }
  set_unless_empty(json, kAnimations, animations);
}

void SceneDocument::sync_node_skins(nlohmann::json& json, const std::vector<int>& written) const {
  for (int i = 0; i < node_count(); ++i) {
    const int skin = desc_.nodes[i].skin;
    const int in_file = in_range(skin, written.size()) ? written[skin] : kNoIndex;
    if (in_file != kNoIndex) {
      json[kNodes][i][kSkin] = in_file;
      continue;
    }
    if (skin != kNoIndex) log::warning("'" + desc_.nodes[i].name + "' lost part of its skeleton: saved without it");
    json[kNodes][i].erase(kSkin);
  }
}

}  // namespace redgem
