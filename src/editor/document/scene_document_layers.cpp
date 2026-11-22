// Part of SceneDocument: the ground layers of terrains, and the weights the Paint brush paints into
// them (kept in the undo steps, shared by the steps that did not change them).

#include <algorithm>
#include <set>

#include "editor/document/scene_document.hpp"
#include "scene/props.hpp"

namespace redgem {
namespace {

// The layers file an object names, when it has a mesh to lay them over.
std::optional<std::string> layers_named(const NodeDesc& node) {
  if (node.mesh == kNoIndex) return std::nullopt;
  return get_string(node.props, props::kTerrainLayers);
}

bool held(const std::vector<TerrainLayers>& terrains, const std::string& name) {
  return std::any_of(terrains.begin(), terrains.end(), [&](const TerrainLayers& t) { return t.name == name; });
}

// Whether a copy an undo step took is the weights a terrain holds now.
bool still_current(const SplatMap& splat, std::uint64_t version, const std::vector<std::uint8_t>* copy) {
  return copy != nullptr && version == splat.version;
}

// The terrain of `terrains` that `matches` (const or not), or null.
template <typename Terrains, typename Match>
auto find_layers(Terrains& terrains, Match matches) -> decltype(&terrains.front()) {
  const auto found = std::find_if(terrains.begin(), terrains.end(), matches);
  return found == terrains.end() ? nullptr : &*found;
}

using Copies = std::set<const std::vector<std::uint8_t>*>;

template <typename Steps>
void add_copies(const Steps& steps, Copies& copies) {
  for (const auto& step : steps) {
    for (const auto& [name, copy] : step.weights) copies.insert(copy.get());
  }
}

}  // namespace

std::vector<std::string> SceneDocument::missing_terrain_layers() const {
  std::set<std::string> missing;
  for (const NodeDesc& node : desc_.nodes) {
    const auto name = layers_named(node);
    if (name && !held(desc_.terrains, *name)) missing.insert(*name);
  }
  return {missing.begin(), missing.end()};
}

void SceneDocument::hold_terrain_layers(TerrainLayers layers) {
  if (held(desc_.terrains, layers.name)) return;
  desc_.terrains.push_back(std::move(layers));
  relink_terrain_layers();
}

void SceneDocument::relink_terrain_layers() {
  for (TerrainLayers& layers : desc_.terrains) {
    const auto named = std::find_if(desc_.nodes.begin(), desc_.nodes.end(),
                                    [&](const NodeDesc& node) { return layers_named(node) == layers.name; });
    layers.node = named == desc_.nodes.end() ? kNoTerrainNode : static_cast<int>(named - desc_.nodes.begin());
  }
}

const TerrainLayers* SceneDocument::terrain_layers(int node) const {
  if (!has_node(node)) return nullptr;
  return find_layers(desc_.terrains, [node](const TerrainLayers& layers) { return layers.node == node; });
}

bool SceneDocument::paint_layer(int node, const LayerBrush& brush) {
  if (!has_node(node)) return false;
  TerrainLayers* layers =
      find_layers(desc_.terrains, [node](const TerrainLayers& terrain) { return terrain.node == node; });
  if (layers == nullptr) return false;
  const MeshDesc& mesh = desc_.meshes.at(static_cast<std::size_t>(desc_.nodes[static_cast<std::size_t>(node)].mesh));
  if (brush_layer(*layers, mesh.bounds, brush) == 0) return false;
  unsaved_weights_.insert(layers->name);
  touch();
  return true;
}

std::vector<const TerrainLayers*> SceneDocument::unsaved_weights() const {
  std::vector<const TerrainLayers*> unsaved;
  for (const TerrainLayers& layers : desc_.terrains) {
    if (unsaved_weights_.count(layers.name) != 0) unsaved.push_back(&layers);
  }
  return unsaved;
}

std::map<std::string, SceneDocument::KeptWeights> SceneDocument::kept_weights() const {
  std::map<std::string, KeptWeights> kept;
  for (const TerrainLayers& layers : desc_.terrains) {
    WeightsCopy& copy = weights_copies_[layers.name];
    if (!still_current(layers.splat, copy.version, copy.weights.get()))
      copy = {layers.splat.version, std::make_shared<const std::vector<std::uint8_t>>(layers.splat.weights)};
    kept.emplace(layers.name, copy.weights);
  }
  return kept;
}

TerrainLayers* SceneDocument::held_layers(const std::string& name) {
  return find_layers(desc_.terrains, [&](const TerrainLayers& layers) { return layers.name == name; });
}

void SceneDocument::restore_weights(const std::map<std::string, KeptWeights>& weights) {
  for (const auto& [name, copy] : weights) {
    TerrainLayers* layers = held_layers(name);
    if (layers == nullptr) continue;
    const WeightsCopy& last = weights_copies_[name];
    const bool unchanged = last.weights == copy && still_current(layers->splat, last.version, copy.get());
    if (!unchanged && layers->splat.weights != *copy) {
      layers->splat.weights = *copy;
      ++layers->splat.version;
      unsaved_weights_.insert(name);
    }
    weights_copies_[name] = {layers->splat.version, copy};
  }
}

std::size_t SceneDocument::undo_weights_bytes() const {
  Copies copies;
  add_copies(undo_, copies);
  add_copies(redo_, copies);
  for (const TerrainLayers& layers : desc_.terrains) {  // a copy of the weights held now is not undo's
    const auto copy = weights_copies_.find(layers.name);
    if (copy == weights_copies_.end()) continue;
    if (still_current(layers.splat, copy->second.version, copy->second.weights.get()))
      copies.erase(copy->second.weights.get());
  }
  std::size_t bytes = 0;
  for (const auto* copy : copies) bytes += copy->size();
  return bytes;
}

}  // namespace redgem
