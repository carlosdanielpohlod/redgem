// Part of SceneDocument: edits of what is there. Names, placements, properties, lights, cameras
// and materials of nodes; removing nodes and moving them under other ones.

#include "editor/document/scene_document.hpp"

#include <algorithm>

#include "editor/document/material_textures.hpp"
#include "editor/node_tree.hpp"

namespace redgem {
namespace {

using namespace node_tree;

constexpr int kFirstCopySuffix = 1;  // "Crate.001" is the first name tried after "Crate"

}  // namespace

void SceneDocument::set_name(int index, const std::string& name) {
  checkpoint();
  desc_.nodes.at(index).name = name;
  touch();
}

void SceneDocument::set_local(int index, const Transform& local, bool record) {
  if (record) checkpoint();
  desc_.nodes.at(index).local = local;
  touch();
}

void SceneDocument::set_prop(int index, const std::string& key, const PropertyValue& value, bool record) {
  if (record) checkpoint();
  desc_.nodes.at(index).props[key] = value;
  touch();
}

void SceneDocument::remove_prop(int index, const std::string& key, bool record) {
  if (record) checkpoint();
  desc_.nodes.at(index).props.erase(key);
  touch();
}

void SceneDocument::set_light(int index, const LightDesc& light, bool record) {
  NodeDesc& node = desc_.nodes.at(index);
  if (!node.light) return;
  if (record) checkpoint();
  node.light = light;
  touch();
}

void SceneDocument::set_camera(int index, const CameraDesc& camera, bool record) {
  NodeDesc& node = desc_.nodes.at(index);
  if (!node.camera) return;
  if (record) checkpoint();
  node.camera = camera;
  touch();
}

int SceneDocument::material_of(int node) const {
  const NodeDesc& n = desc_.nodes.at(node);
  if (n.mesh == kNoIndex) return kNoIndex;
  if (desc_.meshes.at(n.mesh).primitives.empty()) return kNoIndex;
  return desc_.meshes.at(n.mesh).primitives.front().material;
}

int SceneDocument::material_users(int material) const {
  int users = 0;
  for (int i = 0; i < node_count(); ++i) users += material_of(i) == material ? 1 : 0;
  return users;
}

void SceneDocument::set_material(int index, const MaterialDesc& material, bool record) {
  if (record) checkpoint();  // (an image added for this edit is held through it: see unshown_images_)
  desc_.materials.at(index) = material;
  for (const int image : material_textures::images_of(material)) unshown_images_.erase(image);
  touch();
}

std::string SceneDocument::unique_name(const std::string& base) const {
  auto taken = [this](const std::string& name) {
    return std::any_of(desc_.nodes.begin(), desc_.nodes.end(), [&](const NodeDesc& n) { return n.name == name; });
  };
  if (!taken(base)) return base;
  for (int suffix = kFirstCopySuffix;; ++suffix) {
    const std::string candidate = numbered_name(base, suffix);
    if (!taken(candidate)) return candidate;
  }
}

void SceneDocument::remove(int index) { remove(std::vector<int>{index}); }

void SceneDocument::remove(const std::vector<int>& roots) {
  checkpoint();
  const std::vector<int> kept = order_without(node_count(), subtrees(desc_.nodes, roots));
  desc_.nodes = nodes_in_order(desc_.nodes, kept);
  refresh_node_references();
  touch();
}

bool SceneDocument::can_reparent(int index, int new_parent) const {
  if (!has_node(index)) return false;
  if (!can_be_parent(new_parent)) return false;
  const std::vector<int> moving = subtree(desc_.nodes, index);
  const bool into_itself = std::find(moving.begin(), moving.end(), new_parent) != moving.end();
  return !into_itself && desc_.nodes.at(index).parent != new_parent;
}

int SceneDocument::reparent(int index, int new_parent, bool record) {
  if (!can_reparent(index, new_parent)) return index;
  if (record) checkpoint();
  std::vector<NodeDesc>& nodes = desc_.nodes;
  const Transform local = local_under(nodes, index, new_parent);
  // Rebuild the list: everything else in order, then the moved subtree last (after its new parent).
  const std::vector<int> order = order_with_last(node_count(), subtree(nodes, index));
  const std::vector<int> position = positions_in(order, nodes.size());
  std::vector<NodeDesc> reordered = nodes_in_order(nodes, order);
  const int moved = position[index];
  move_under(reordered, moved, new_parent == kNoIndex ? kNoIndex : position[new_parent], local);
  nodes = std::move(reordered);
  refresh_node_references();
  touch();
  return moved;
}

}  // namespace redgem
