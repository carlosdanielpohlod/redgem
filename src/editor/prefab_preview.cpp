#include "editor/prefab_preview.hpp"

#include <algorithm>
#include <exception>
#include <stdexcept>
#include <utility>
#include "scene/prefab_import.hpp"
#include "scene/gltf/gltf_budget.hpp"
#include "core/log.hpp"
#include "scene/props.hpp"

namespace redgem {
namespace {
prefabs::Limits remaining_budget(const SceneDesc& document, prefabs::Limits limits) {
  const auto copied =
      prefabs::instance_bytes(document) + prefabs::asset_bytes(document) - gltf_budget::decoded_bytes(document.images);
  if (copied > limits.expanded_bytes) throw std::runtime_error("scene exceeds the prefab preview copy budget");
  limits.expanded_bytes -= copied;
  return limits;
}
bool matches_reference(const NodeDesc& node, const prefabs::Origin& origin) {
  const bool empty = node.mesh == kNoIndex && node.skin == kNoIndex && !node.camera && !node.light;
  return empty && get_string(node.props, prefabs::kFileProperty) == origin.reference;
}
void refresh_root(NodeDesc& target, const NodeDesc& authored, const prefabs::Origin& origin) {
  target.name = authored.name.empty() ? origin.name : authored.name;
  target.local = prefabs::inherited_local(origin, authored.local);
  target.props = prefabs::inherited_props(origin, authored.props);
  target.props.erase(props::kPrefabOverrides);
  target.props.erase(props::kPrefabParent);
  target.parent = authored.parent;
}
std::string overrides_of(const NodeDesc& node) { return get_string(node.props, props::kPrefabOverrides).value_or(""); }
std::string part_of(const NodeDesc& node) { return get_string(node.props, props::kPrefabParent).value_or(""); }
// The transform of the part an authored child hangs from (see prefabs::resolve), if it hangs from one.

}  // namespace
const Transform* PrefabPreview::attachment(int node) const {
  for (const auto& entry : origins_) {
    const auto found = entry.second.attached.find(node);
    if (found != entry.second.attached.end()) return &found->second;
  }
  return nullptr;
}
void PrefabPreview::adopt(PrefabPreview&& ready) {
  const auto next = generation_ + 1;
  *this = std::move(ready);
  generation_ = next;
}
PrefabPreview::Counts PrefabPreview::counts(const SceneDesc& scene) {
  return {scene.meshes.size(), scene.materials.size(), scene.images.size()};
}
void PrefabPreview::report(const std::string& error) {
  if (error_ != error && !error.empty()) log::error("prefab preview: " + error);
  error_ = error;
}
void PrefabPreview::load(const SceneDesc& document, const std::filesystem::path& game_dir,
                         const prefabs::Limits& limits) {
  if (prefabs::has_references(document)) {
    const auto budget = remaining_budget(document, limits);
    remember_references(document);
    resolved_ = std::make_unique<SceneDesc>(prefabs::resolve(document, game_dir, budget, &origins_));
  }
  report("");
}
void PrefabPreview::rebuild(const SceneDesc& document, const std::filesystem::path& game_dir,
                            const prefabs::Limits& limits) {
  const bool had_snapshot = resolved_ != nullptr;
  resolved_.reset();
  origins_.clear();
  references_.clear();
  links_.clear();
  base_counts_ = counts(document);
  try {
    load(document, game_dir, limits);
  } catch (const std::exception& error) {
    report(error.what());
  }
  if (had_snapshot || resolved_) ++generation_;
}
void PrefabPreview::remember_references(const SceneDesc& document) {
  for (std::size_t i = 0; i < document.nodes.size(); ++i) {
    const auto& node = document.nodes[i];
    const auto file = node.props.find(prefabs::kFileProperty);
    if (file != node.props.end()) references_.emplace(static_cast<int>(i), file->second);
    if (node.props.count(props::kPrefabOverrides) + node.props.count(props::kPrefabParent) != 0)
      links_.emplace(static_cast<int>(i), overrides_of(node) + '\n' + part_of(node));
  }
}
bool PrefabPreview::reference_unchanged(const NodeDesc& node, int id) const {
  const auto old = references_.find(id);
  const auto file = node.props.find(prefabs::kFileProperty);
  if (old == references_.end()) return file == node.props.end();
  return file != node.props.end() && file->second == old->second;
}
bool PrefabPreview::links_unchanged(const NodeDesc& node, int id) const {
  const auto old = links_.find(id);
  const auto now = overrides_of(node) + '\n' + part_of(node);
  return old == links_.end() ? now.size() == 1 : old->second == now;
}
bool PrefabPreview::refresh_plain_node(const NodeDesc& authored, int node) {
  auto& target = resolved_->nodes.at(static_cast<std::size_t>(node));
  target = authored;
  target.props.erase(props::kPrefabParent);
  const auto* chain = attachment(node);
  if (chain) target.local = combine(*chain, authored.local);
  return true;
}
bool PrefabPreview::refresh_root_node(const NodeDesc& authored, int node, const prefabs::Origin& origin) {
  if (!matches_reference(authored, origin)) return false;
  try {
    auto& target = resolved_->nodes.at(static_cast<std::size_t>(node));
    refresh_root(target, authored, origin);
    if (const auto* chain = attachment(node)) target.local = combine(*chain, target.local);
    return true;
  } catch (const std::exception&) {
    return false;  // Rebuild reports the failure and leaves a repairable authored document.
  }
}
bool PrefabPreview::refresh(const SceneDesc& document, int node) {
  if (!resolved_) return reference_unchanged(document.nodes.at(static_cast<std::size_t>(node)), node);
  const auto& authored = document.nodes.at(static_cast<std::size_t>(node));
  if (!links_unchanged(authored, node)) return false;
  const auto origin = origins_.find(node);
  if (origin != origins_.end()) return refresh_root_node(authored, node, origin->second);
  if (authored.props.count(prefabs::kFileProperty) != 0) return false;
  return refresh_plain_node(authored, node);
}
bool PrefabPreview::sync_assets(const SceneDesc& document, const std::vector<int>& changed_meshes) {
  if (!resolved_) return true;
  if (counts(document) != base_counts_) return false;
  std::copy(document.materials.begin(), document.materials.end(), resolved_->materials.begin());
  for (const int mesh : changed_meshes) resolved_->meshes.at(static_cast<std::size_t>(mesh)) = document.meshes.at(mesh);
  return sync_weights(document);
}
bool PrefabPreview::sync_weights(const SceneDesc& document) {
  if (resolved_->terrains.size() != document.terrains.size()) return false;
  for (std::size_t i = 0; i < document.terrains.size(); ++i) {
    const TerrainLayers& painted = document.terrains[i];
    TerrainLayers& shown = resolved_->terrains[i];
    if (shown.name != painted.name || shown.node != painted.node) return false;
    if (shown.splat.version != painted.splat.version) shown.splat = painted.splat;
  }
  return true;
}
}  // namespace redgem
