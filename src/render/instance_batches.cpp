#include "render/instance_batches.hpp"

namespace redgem::render_math {

void InstanceBatches::start(std::size_t meshes) {
  for (const int mesh : order_) by_mesh_[static_cast<std::size_t>(mesh)].clear();
  if (by_mesh_.size() < meshes) by_mesh_.resize(meshes);
  order_.clear();
  slots_.clear();
  batches_.clear();
}

std::vector<float>& InstanceBatches::copies_of(int mesh) {
  std::vector<float>& copies = by_mesh_[static_cast<std::size_t>(mesh)];
  if (copies.empty()) order_.push_back(mesh);
  return copies;
}

void InstanceBatches::add(int mesh, std::size_t record) { copies_of(mesh).push_back(static_cast<float>(record)); }

void InstanceBatches::add_run(int mesh, std::size_t first, std::size_t count) {
  if (count == 0) return;  // (a mesh is in the order once it has a copy)
  std::vector<float>& copies = copies_of(mesh);
  for (std::size_t record = first; record < first + count; ++record) copies.push_back(static_cast<float>(record));
}

void InstanceBatches::lay_out() {
  for (const int mesh : order_) {
    const std::vector<float>& copies = by_mesh_[static_cast<std::size_t>(mesh)];
    batches_.push_back({mesh, slots_.size(), copies.size()});
    slots_.insert(slots_.end(), copies.begin(), copies.end());
  }
}

}  // namespace redgem::render_math
