#include <algorithm>
#include <utility>

#include "core/log.hpp"
#include "core/safe_path.hpp"
#include "scene/prefab_import.hpp"

namespace redgem::prefabs {
namespace {
using Pending = std::vector<std::pair<int, std::string>>;  // node, its parent's path

bool contains(const std::vector<int>& values, int value) {
  return std::find(values.begin(), values.end(), value) != values.end();
}
const std::string& part_id(const SceneDesc& scene, int node, const std::map<int, std::string>& part_ids) {
  const auto found = part_ids.find(node);
  return found == part_ids.end() ? scene.nodes[node].stable_id : found->second;
}
void push_children(const SceneDesc& scene, int node, const std::string& path, Pending& pending) {
  const auto& children = scene.nodes[node].children;
  for (auto child = children.rbegin(); child != children.rend(); ++child) pending.emplace_back(*child, path);
}

// Marks `root` and everything under it.
void mark_subtree(const SceneDesc& scene, int root, std::vector<bool>& marked) {
  std::vector<int> pending{root};
  while (!pending.empty()) {
    const int node = pending.back();
    pending.pop_back();
    if (marked[node]) continue;
    marked[node] = true;
    for (const int child : scene.nodes[node].children) pending.push_back(child);
  }
}
// Node order with parents first: index order, a node whose parent comes later right after it.
class Ordering {
 public:
  Ordering(const SceneDesc& scene, std::vector<bool> dropped)
      : scene_(scene), dropped_(std::move(dropped)), placed_(scene.nodes.size()), waiting_(scene.nodes.size()) {}
  std::vector<int> run() {
    for (std::size_t i = 0; i < scene_.nodes.size(); ++i) visit(static_cast<int>(i));
    return std::move(order_);
  }

 private:
  void visit(int node) {
    if (dropped_[node]) return;
    const int parent = scene_.nodes[node].parent;
    if (parent != kNoIndex && !placed_[parent]) return waiting_[parent].push_back(node);
    place(node);
  }
  void place(int first) {
    std::vector<int> pending{first};
    while (!pending.empty()) {
      const int node = pending.back();
      pending.pop_back();
      placed_[node] = true;
      order_.push_back(node);
      pending.insert(pending.end(), waiting_[node].rbegin(), waiting_[node].rend());
    }
  }
  const SceneDesc& scene_;
  std::vector<bool> dropped_;
  std::vector<bool> placed_;
  std::vector<std::vector<int>> waiting_;
  std::vector<int> order_;
};
int remapped(int index, const std::vector<int>& remap) { return index == kNoIndex ? kNoIndex : remap[index]; }
void renumber_node(NodeDesc& node, const std::vector<int>& remap) {
  node.parent = remapped(node.parent, remap);
  std::vector<int> children;
  for (const int child : node.children) {
    if (remap[child] != kNoIndex) children.push_back(remap[child]);
  }
  node.children = std::move(children);
}
void renumber_references(SceneDesc& scene, const std::vector<int>& remap) {
  for (auto& skin : scene.skins)
    for (auto& joint : skin.joints) joint = remapped(joint, remap);
  for (auto& animation : scene.animations) {
    auto& channels = animation.channels;
    for (auto& channel : channels) channel.node = remapped(channel.node, remap);
    channels.erase(std::remove_if(channels.begin(), channels.end(), [](const auto& c) { return c.node == kNoIndex; }),
                   channels.end());
  }
}
void put_in_order(SceneDesc& scene, const std::vector<int>& order, const std::vector<int>& remap) {
  std::vector<NodeDesc> nodes;
  nodes.reserve(order.size());
  for (const int old : order) nodes.push_back(std::move(scene.nodes[old]));
  for (auto& node : nodes) renumber_node(node, remap);
  scene.nodes = std::move(nodes);
}
}  // namespace

PartPaths part_paths(const SceneDesc& scene, int placement, const std::vector<int>& own,
                     const std::map<int, std::string>& part_ids) {
  PartPaths paths;
  Pending pending;
  for (const int child : scene.nodes[placement].children) {
    if (!contains(own, child)) pending.emplace_back(child, std::string());
  }
  while (!pending.empty()) {
    const auto [node, parent] = pending.back();
    pending.pop_back();
    const auto path = child_path(parent, part_id(scene, node, part_ids));
    if (paths.emplace(path, node).second) push_children(scene, node, path, pending);
  }
  return paths;
}
std::vector<int> apply_part_overrides(SceneDesc& scene, const PartPaths& parts, const Overrides& overrides) {
  std::vector<int> removed;
  for (const auto& [path, change] : overrides) {
    const auto part = parts.find(path);
    if (part == parts.end()) {
      log::warning("prefab override names a part the file no longer has: " + printable(path));
      continue;
    }
    apply_override(scene.nodes[part->second], change);
    if (change.removed) removed.push_back(part->second);
  }
  return removed;
}
bool inside_any(const SceneDesc& scene, int part, const std::vector<int>& roots, int placement) {
  for (int node = part; node != placement && node != kNoIndex; node = scene.nodes[node].parent) {
    if (contains(roots, node)) return true;
  }
  return false;
}
Transform part_chain(const SceneDesc& scene, int placement, int part) {
  Transform chain = scene.nodes[part].local;
  for (int node = scene.nodes[part].parent; node != placement && node != kNoIndex; node = scene.nodes[node].parent)
    chain = combine(scene.nodes[node].local, chain);
  return chain;
}
void hang_under(SceneDesc& scene, int child, int part) {
  auto& siblings = scene.nodes[scene.nodes[child].parent].children;
  siblings.erase(std::remove(siblings.begin(), siblings.end(), child), siblings.end());
  scene.nodes[part].children.push_back(child);
  scene.nodes[child].parent = part;
}
std::vector<int> compact_nodes(SceneDesc& scene, const std::vector<int>& removed) {
  std::vector<bool> dropped(scene.nodes.size());
  for (const int root : removed) mark_subtree(scene, root, dropped);
  const auto order = Ordering(scene, std::move(dropped)).run();
  std::vector<int> remap(scene.nodes.size(), kNoIndex);
  for (std::size_t i = 0; i < order.size(); ++i) remap[order[i]] = static_cast<int>(i);
  put_in_order(scene, order, remap);
  renumber_references(scene, remap);
  return remap;
}
}  // namespace redgem::prefabs
