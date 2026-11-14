#include "scene/prefab_import.hpp"
#include <mbedtls/sha256.h>
#include <algorithm>
#include <array>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include "scene/props.hpp"

namespace redgem::prefabs {
namespace {
constexpr std::size_t kDigestBytes = 32;
constexpr int kHexDigits = 2;
std::string digest(const std::string& input) {
  std::array<unsigned char, kDigestBytes> result{};
  if (mbedtls_sha256(reinterpret_cast<const unsigned char*>(input.data()), input.size(), result.data(), 0) != 0)
    throw std::runtime_error("cannot calculate prefab identity");
  std::ostringstream text;
  text << std::hex << std::setfill('0');
  for (const auto byte : result) text << std::setw(kHexDigits) << static_cast<unsigned int>(byte);
  return text.str();
}
std::string identity_part(const std::string& text) { return std::to_string(text.size()) + ":" + text; }
int shifted(int index, int offset) { return index == kNoIndex ? kNoIndex : index + offset; }
bool has_identity(const SceneDesc& scene, const std::string& identity) {
  return std::any_of(scene.nodes.begin(), scene.nodes.end(),
                     [&identity](const NodeDesc& node) { return node.stable_id == identity; });
}
// What a node of the prefab is called in this instance (one outside the prefab keeps its identity).
std::string instanced_identity(const SceneDesc& source, const Instance& instance, const std::string& identity) {
  if (identity == source.nodes.front().stable_id) return instance.identity;
  return has_identity(source, identity) ? child_identity(instance, identity) : identity;
}
// A joint between parts of the prefab holds the parts of this instance.
void follow_joint_into_instance(PropertyMap& props, const SceneDesc& source, const Instance& instance) {
  const auto other = get_string(props, props::kJointTo);
  if (other && !other->empty()) props[props::kJointTo] = instanced_identity(source, instance, *other);
}
NodeDesc instance_node_desc(NodeDesc node, const SceneDesc& source, const Assets& assets, const Instance& instance) {
  node.parent = instance_node(node.parent, instance);
  for (auto& child : node.children) child = instance_node(child, instance);
  node.mesh = shifted(node.mesh, assets.meshes);
  node.skin = shifted(node.skin, instance.skins);
  node.stable_id = child_identity(instance, node.stable_id);
  node.source_node = node.source_camera = node.source_light = kNoIndex;
  follow_joint_into_instance(node.props, source, instance);
  return node;
}
void apply_overrides(PropertyMap& target, PropertyMap authored) {
  authored.erase(kFileProperty);
  while (!authored.empty()) {
    auto entry = authored.extract(authored.begin());
    target.erase(entry.key());
    target.insert(std::move(entry));
  }
}
bool finite(Vec3 value) { return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z); }
bool finite(const Transform& value) {
  const auto rotation = value.rotation;
  return finite(value.position) && finite(value.scale) && std::isfinite(rotation.x) && std::isfinite(rotation.y) &&
         std::isfinite(rotation.z) && std::isfinite(rotation.w);
}
NodeDesc instance_root(const SceneDesc& source, NodeDesc authored, const Assets& assets, const Instance& instance) {
  auto root = instance_node_desc(source.nodes.front(), source, assets, instance);
  root.name = std::move(authored.name);
  if (root.name.empty()) root.name = source.nodes.front().name;
  root.local = checked_placement(combine(authored.local, source.local));
  root.parent = authored.parent;
  root.stable_id = std::move(authored.stable_id);
  root.source_node = authored.source_node;
  root.children.insert(root.children.end(), authored.children.begin(), authored.children.end());
  root.props.erase(props::kPrefab);
  apply_overrides(root.props, std::move(authored.props));
  return root;
}
void append_skins(SceneDesc& target, const SceneDesc& source, const Instance& instance) {
  for (auto skin : source.skins) {
    for (auto& joint : skin.joints) joint = instance_node(joint, instance);
    target.skins.push_back(std::move(skin));
  }
}
}  // namespace
Transform checked_placement(const Transform& placement) {
  if (!finite(placement)) throw std::runtime_error("prefab placement transform is not finite");
  return placement;
}
int instance_node(int node, const Instance& instance) {
  if (node == kNoIndex) return kNoIndex;
  return node == 0 ? instance.root : instance.nodes + node - 1;
}
std::string child_identity(const Instance& instance, const std::string& source_identity) {
  return digest(identity_part(instance.file) + identity_part(instance.identity) + identity_part(source_identity));
}
Instance append_instance(SceneDesc& target, Source& source, int root, const std::string& file) {
  const Instance instance{root, static_cast<int>(target.nodes.size()), static_cast<int>(target.skins.size()),
                          target.nodes[root].stable_id, file};
  auto authored = std::move(target.nodes[root]);
  target.nodes[root] = instance_root(source.scene, std::move(authored), source.assets, instance);
  for (std::size_t i = 1; i < source.scene.nodes.size(); ++i)
    target.nodes.push_back(instance_node_desc(source.scene.nodes[i], source.scene, source.assets, instance));
  append_skins(target, source.scene, instance);
  append_animations(target, source, instance);
  return instance;
}
}  // namespace redgem::prefabs
