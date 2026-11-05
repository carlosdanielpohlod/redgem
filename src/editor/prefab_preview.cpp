#include "editor/prefab_preview.hpp"

#include <algorithm>
#include <exception>
#include <stdexcept>
#include "scene/prefab_import.hpp"
#include "scene/gltf/gltf_budget.hpp"
#include "core/log.hpp"

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
  target.parent = authored.parent;
}
}  // namespace
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
    const auto file = document.nodes[i].props.find(prefabs::kFileProperty);
    if (file != document.nodes[i].props.end()) references_.emplace(static_cast<int>(i), file->second);
  }
}
bool PrefabPreview::reference_unchanged(const NodeDesc& node, int id) const {
  const auto old = references_.find(id);
  const auto file = node.props.find(prefabs::kFileProperty);
  if (old == references_.end()) return file == node.props.end();
  return file != node.props.end() && file->second == old->second;
}
bool PrefabPreview::refresh_root_node(const NodeDesc& authored, int node, const prefabs::Origin& origin) {
  if (!matches_reference(authored, origin)) return false;
  try {
    refresh_root(resolved_->nodes.at(static_cast<std::size_t>(node)), authored, origin);
    return true;
  } catch (const std::exception&) {
    return false;  // Rebuild reports the failure and leaves a repairable authored document.
  }
}
bool PrefabPreview::refresh(const SceneDesc& document, int node) {
  if (!resolved_) return reference_unchanged(document.nodes.at(static_cast<std::size_t>(node)), node);
  const auto& authored = document.nodes.at(static_cast<std::size_t>(node));
  const auto origin = origins_.find(node);
  if (origin != origins_.end()) return refresh_root_node(authored, node, origin->second);
  if (authored.props.count(prefabs::kFileProperty) != 0) return false;
  resolved_->nodes.at(static_cast<std::size_t>(node)) = authored;
  return true;
}
bool PrefabPreview::sync_assets(const SceneDesc& document, const std::vector<int>& changed_meshes) {
  if (!resolved_) return true;
  if (counts(document) != base_counts_) return false;
  std::copy(document.materials.begin(), document.materials.end(), resolved_->materials.begin());
  for (const int mesh : changed_meshes) resolved_->meshes.at(static_cast<std::size_t>(mesh)) = document.meshes.at(mesh);
  return true;
}
}  // namespace redgem
