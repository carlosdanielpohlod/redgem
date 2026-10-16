// Part of SceneDocument: keeping the undo history within its limits, and letting go of what no
// step of it can bring back (the meshes of deleted objects and undone imports, once their steps
// are gone).

#include "editor/document/scene_document.hpp"

#include <algorithm>
#include <cstdint>
#include <functional>

#include "editor/document/glb_compact.hpp"

namespace redgem {

bool SceneDocument::history_too_long() {
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
  const std::vector<bool> in_use = meshes_in_use();
  std::transform(reachable.begin(), reachable.end(), in_use.begin(), reachable.begin(), std::logical_or<>());
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
