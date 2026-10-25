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
  // Each copy is a node of its own in the file too, and a character copied with its bones gets
  // its own skeleton (its animation is not copied: the copy keeps the pose it was copied in).
  give_copies_own_source_nodes(first_copy, source.size());
  give_copies_own_skins(first_copy, source.size(), copy_of);
  touch();
  return copies_of(roots, copy_of);
}

void SceneDocument::append_copies(const std::vector<int>& source, const std::vector<int>& copy_of) {
  std::vector<NodeDesc>& nodes = desc_.nodes;
  for (const int original : source) {
    NodeDesc copy = nodes[original];
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

}  // namespace redgem
