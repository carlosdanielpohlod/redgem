// Part of SceneDocument: how much the undo history keeps, and letting go of what no step of it
// can bring back (the meshes of deleted objects and undone imports, once their steps are gone).

#include "editor/document/scene_document.hpp"

#include <algorithm>
#include <cstdint>

#include "editor/document/glb_compact.hpp"

namespace redgem {
namespace {

// Notes which meshes the nodes show.
void mark_meshes(const std::vector<NodeDesc>& nodes, std::vector<bool>& used) {
  for (const NodeDesc& node : nodes) {
    if (is_index_of(node.mesh, used.size())) used[static_cast<std::size_t>(node.mesh)] = true;
  }
}

std::size_t primitive_bytes(const PrimitiveDesc& part) {
  const std::size_t floats = part.positions.size() + part.normals.size() + part.texcoords.size() + part.weights.size();
  return floats * sizeof(float) + part.indices.size() * sizeof(std::uint32_t) + part.colors.size() + part.joints.size();
}

std::size_t mesh_bytes(const MeshDesc& mesh) {
  std::size_t bytes = 0;
  for (const PrimitiveDesc& part : mesh.primitives) bytes += primitive_bytes(part);
  return bytes;
}

// A mesh some step of the history shows, and nothing now: kept only for undo and redo.
bool kept_for_undo(bool in_history, bool in_use) { return in_history && !in_use; }

}  // namespace

std::size_t SceneDocument::bytes_of(const std::vector<Snapshot>& steps) {
  std::size_t bytes = 0;
  for (const Snapshot& step : steps) {
    bytes += step.nodes.size() * sizeof(NodeDesc) + step.materials.size() * sizeof(MaterialDesc);
    for (const auto& [mesh, shape] : step.shapes)
      bytes += (shape.positions.size() + shape.normals.size()) * sizeof(float);
  }
  return bytes;
}

std::vector<bool> SceneDocument::meshes_in_history() const {
  std::vector<bool> used(desc_.meshes.size(), false);
  for (const Snapshot& step : undo_) mark_meshes(step.nodes, used);
  for (const Snapshot& step : redo_) mark_meshes(step.nodes, used);
  return used;
}

std::size_t SceneDocument::history_bytes() const {
  std::size_t bytes = bytes_of(undo_) + bytes_of(redo_);
  const std::vector<bool> in_history = meshes_in_history();
  std::vector<bool> in_use(desc_.meshes.size(), false);
  mark_meshes(desc_.nodes, in_use);
  for (std::size_t mesh = 0; mesh < in_use.size(); ++mesh) {
    if (kept_for_undo(in_history[mesh], in_use[mesh])) bytes += mesh_bytes(desc_.meshes[mesh]);
  }
  return bytes;
}

bool SceneDocument::history_too_long() const {
  if (undo_.size() > kMaxUndoSteps) return true;
  if (undo_.size() <= 1) return false;  // the latest step stays, however big
  return history_bytes() > kMaxHistoryBytes;
}

void SceneDocument::trim_history() {
  while (history_too_long()) undo_.erase(undo_.begin());
  release_unreachable();
}

std::vector<int> SceneDocument::meshes_out_of_reach() const {
  std::vector<bool> reachable = meshes_in_history();
  mark_meshes(desc_.nodes, reachable);
  std::vector<int> out_of_reach;
  for (std::size_t mesh = 0; mesh < reachable.size(); ++mesh) {
    if (reachable[mesh]) continue;
    if (released_meshes_.count(static_cast<int>(mesh)) == 0) out_of_reach.push_back(static_cast<int>(mesh));
  }
  return out_of_reach;
}

void SceneDocument::release_unreachable() {
  const std::vector<int> out_of_reach = meshes_out_of_reach();
  if (out_of_reach.empty()) return;
  for (const int mesh : out_of_reach) release_mesh(mesh);
  const std::optional<std::vector<bool>> images_used =
      release_meshes(glb_, released_meshes_, {source_nodes_, source_skins_, source_animations_});
  if (images_used) release_images(*images_used);
}

void SceneDocument::release_mesh(int mesh) {
  std::vector<PrimitiveDesc>().swap(desc_.meshes[static_cast<std::size_t>(mesh)].primitives);
  changed_meshes_.erase(mesh);
  unsaved_meshes_.erase(mesh);
  released_meshes_.insert(mesh);
}

void SceneDocument::release_images(const std::vector<bool>& used) {
  const std::size_t images = std::min(desc_.images.size(), used.size());
  for (std::size_t image = 0; image < images; ++image) {
    if (!used[image]) std::vector<std::uint8_t>().swap(desc_.images[image].encoded);
  }
}

}  // namespace redgem
