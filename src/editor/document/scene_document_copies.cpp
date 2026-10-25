// Part of SceneDocument: duplicating a node with everything under it.

#include "editor/document/scene_document.hpp"

#include <algorithm>
#include <map>

#include "editor/document/gltf_json.hpp"
#include "editor/node_tree.hpp"

namespace redgem {
namespace {

using namespace gltf_json;

bool all_copied(const std::vector<int>& joints, const std::vector<int>& copy_of) {
  return std::all_of(joints.begin(), joints.end(),
                     [&copy_of](int joint) { return joint != kNoIndex && copy_of[joint] != kNoIndex; });
}

}  // namespace

int SceneDocument::duplicate(int index) { return duplicate(std::vector<int>{index}).front(); }

std::vector<int> SceneDocument::duplicate(const std::vector<int>& roots) {
  checkpoint();
  std::vector<int> copies;
  // (Copies go after every node there is: the numbers of the roots still to copy stay as they are.)
  for (const int root : roots) copies.push_back(copy_subtree(root));
  touch();
  return copies;
}

int SceneDocument::copy_subtree(int index) {
  const std::vector<int> source = node_tree::subtree(desc_.nodes, index);
  const int first_copy = node_count();
  std::vector<int> copy_of(desc_.nodes.size(), kNoIndex);
  for (std::size_t i = 0; i < source.size(); ++i) copy_of[source[i]] = first_copy + static_cast<int>(i);
  append_copies(source, copy_of);
  // Each copy is a node of its own in the file too, and a character copied with its bones gets
  // its own skeleton (its animation is not copied: the copy keeps the pose it was copied in).
  give_copies_own_source_nodes(first_copy, source.size());
  give_copies_own_skins(first_copy, source.size(), copy_of);
  return first_copy;
}

void SceneDocument::append_copies(const std::vector<int>& source, const std::vector<int>& copy_of) {
  std::vector<NodeDesc>& nodes = desc_.nodes;
  const int root = source.front();
  for (const int original : source) {
    NodeDesc copy = nodes[original];
    copy.name = unique_name(copy.name);
    copy.parent = original == root ? copy.parent : copy_of[copy.parent];
    for (int& child : copy.children) child = copy_of[child];
    nodes.push_back(std::move(copy));  // names stay unique because each copy is added before the next
  }
  const int first_copy = copy_of[root];
  if (nodes[first_copy].parent != kNoIndex) nodes[nodes[first_copy].parent].children.push_back(first_copy);
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
