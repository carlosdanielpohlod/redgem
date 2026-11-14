// Part of SceneDocument: duplicating a node with everything under it.

#include "editor/document/scene_document.hpp"

#include <algorithm>
#include <map>

#include "editor/document/gltf_json.hpp"
#include "editor/node_tree.hpp"

namespace redgem {
namespace {

using namespace gltf_json;

// Whether `node` is one of the nodes copied.
bool copied(int node, const std::vector<int>& copy_of) { return node != kNoIndex && copy_of[node] != kNoIndex; }

// A copy whose original's parent is `parent` is a copied root under that same parent: it has
// one, and it was not copied.
bool copied_root_under_parent(int parent, const std::vector<int>& copy_of) {
  return parent != kNoIndex && !copied(parent, copy_of);
}

// The number each of `source` gets as a copy (they are added from `first` on, in order), by node.
std::vector<int> copy_numbers(const std::vector<int>& source, int first, std::size_t nodes) {
  std::vector<int> copy_of(nodes, kNoIndex);
  for (std::size_t i = 0; i < source.size(); ++i) copy_of[source[i]] = first + static_cast<int>(i);
  return copy_of;
}

std::vector<int> copies_of(const std::vector<int>& originals, const std::vector<int>& copy_of) {
  std::vector<int> copies;
  copies.reserve(originals.size());
  for (const int original : originals) copies.push_back(copy_of[original]);
  return copies;
}

// The copied roots join the parents of their originals.
void attach_copied_roots(std::vector<NodeDesc>& nodes, const std::vector<int>& source,
                         const std::vector<int>& copy_of) {
  for (const int original : source) {
    const int copy = copy_of[original];
    if (copied_root_under_parent(nodes[original].parent, copy_of)) nodes[nodes[copy].parent].children.push_back(copy);
  }
}

bool all_copied(const std::vector<int>& joints, const std::vector<int>& copy_of) {
  return std::all_of(joints.begin(), joints.end(), [&copy_of](int joint) { return copied(joint, copy_of); });
}

// `copy_of` without the copied roots: a copy is put where it is wanted, and the keys of its
// original's root would pull it back onto the original (as Scene.spawn leaves a copy's root).
std::vector<int> animated_copies(const std::vector<int>& roots, std::vector<int> copy_of) {
  for (const int root : roots) copy_of[root] = kNoIndex;
  return copy_of;
}

// Whether a node and its copy both have a node of the file behind them.
bool both_in_file(int source_node, int copy_source_node) {
  return source_node != kNoIndex && copy_source_node != kNoIndex;
}

// The source node of each copied node that has one -> the source node of its copy.
std::map<int, int> copied_sources(const std::vector<NodeDesc>& nodes, const std::vector<int>& source,
                                  const std::vector<int>& copy_of) {
  std::map<int, int> sources;
  for (const int original : source) {
    if (!copied(original, copy_of)) continue;
    const int from = nodes[original].source_node;
    const int to = nodes[copy_of[original]].source_node;
    if (both_in_file(from, to)) sources[from] = to;
  }
  return sources;
}

}  // namespace

int SceneDocument::duplicate(int index) { return duplicate(std::vector<int>{index}).front(); }

std::vector<int> SceneDocument::duplicate(const std::vector<int>& roots) {
  checkpoint();
  // All the subtrees are copied as one, so that a skinned mesh and its bones copied side by side
  // still find each other in the copy.
  const std::vector<int> source = node_tree::subtrees(desc_.nodes, roots);
  const int first_copy = node_count();
  const std::vector<int> copy_of = copy_numbers(source, first_copy, desc_.nodes.size());
  append_copies(source, copy_of);
  // Each copy is a node of its own in the file too, a character copied with its bones gets its
  // own skeleton, and the clips that move the originals move the copies.
  give_copies_own_source_nodes(first_copy, source.size());
  give_copies_own_skins(first_copy, source.size(), copy_of);
  give_copies_own_channels(source, animated_copies(roots, copy_of));
  touch();
  return copies_of(roots, copy_of);
}

void SceneDocument::append_copies(const std::vector<int>& source, const std::vector<int>& copy_of) {
  std::vector<NodeDesc>& nodes = desc_.nodes;
  for (const int original : source) {
    NodeDesc copy = nodes[original];
    copy.stable_id = fresh_id();
    copy.name = unique_name(copy.name);
    // (A copied root stays under the parent of its original; the others go under their parent's copy.)
    if (copied(copy.parent, copy_of)) copy.parent = copy_of[copy.parent];
    for (int& child : copy.children) child = copy_of[child];
    nodes.push_back(std::move(copy));  // names stay unique because each copy is added before the next
  }
  attach_copied_roots(nodes, source, copy_of);
}

void SceneDocument::give_copies_own_source_nodes(int first_copy, std::size_t count) {
  for (std::size_t i = 0; i < count; ++i) {
    NodeDesc& copy = desc_.nodes[first_copy + static_cast<int>(i)];
    if (!in_range(copy.source_node, source_nodes_.size())) continue;
    source_nodes_.push_back(source_nodes_[copy.source_node]);
    copy.source_node = last_index(source_nodes_);
  }
}

void SceneDocument::give_copies_own_skins(int first_copy, std::size_t count, const std::vector<int>& copy_of) {
  std::map<int, int> copied_skins;
  for (std::size_t i = 0; i < count; ++i) {
    NodeDesc& copy = desc_.nodes[first_copy + static_cast<int>(i)];
    if (!in_range(copy.skin, desc_.skins.size())) continue;
    if (copied_skins.count(copy.skin) == 0) {
      const std::optional<int> skin = copy_skin(copy.skin, copy_of);
      if (!skin) continue;  // bones outside the copy stay shared with the original
      copied_skins[copy.skin] = *skin;
    }
    copy.skin = copied_skins[copy.skin];
  }
}

std::optional<int> SceneDocument::copy_skin(int original, const std::vector<int>& copy_of) {
  SkinDesc skin = desc_.skins[original];
  if (!all_copied(skin.joints, copy_of)) return std::nullopt;
  nlohmann::json json = source_skins_[original];
  std::vector<int> joint_sources = point_at_copies(skin, json, copy_of);
  json.erase(kSkeleton);
  desc_.skins.push_back(std::move(skin));
  source_skins_.push_back(std::move(json));
  skin_joint_sources_.push_back(std::move(joint_sources));
  return static_cast<int>(desc_.skins.size()) - 1;
}

std::vector<int> SceneDocument::point_at_copies(SkinDesc& skin, nlohmann::json& json,
                                                const std::vector<int>& copy_of) const {
  std::vector<int> joint_sources;
  for (std::size_t j = 0; j < skin.joints.size(); ++j) {
    skin.joints[j] = copy_of[skin.joints[j]];
    joint_sources.push_back(desc_.nodes[skin.joints[j]].source_node);
    json[kJoints][j] = joint_sources.back();
  }
  return joint_sources;
}

void SceneDocument::give_copies_own_channels(const std::vector<int>& source, const std::vector<int>& copy_of) {
  const std::map<int, int> sources = copied_sources(desc_.nodes, source, copy_of);
  for (nlohmann::json& animation : source_animations_) add_channels_for_copies(animation, sources);
  for (std::size_t a = 0; a < desc_.animations.size(); ++a) copy_described_channels(a, copy_of);
}

void SceneDocument::copy_described_channels(std::size_t animation, const std::vector<int>& copy_of) {
  std::vector<AnimationChannel>& channels = desc_.animations[animation].channels;
  const std::size_t count = channels.size();  // (the copies go after the channels there were)
  for (std::size_t c = 0; c < count; ++c) {
    if (!copied(channels[c].node, copy_of)) continue;
    AnimationChannel copy = channels[c];
    copy.node = copy_of[copy.node];
    channel_node_sources_[animation].push_back(desc_.nodes[copy.node].source_node);
    channels.push_back(std::move(copy));
  }
}

}  // namespace redgem
