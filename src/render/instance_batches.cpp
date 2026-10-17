#include "render/instance_batches.hpp"

namespace redgem::render_math {
namespace {

// Whether the last batch is of `mesh`: its next copy goes in it.
bool goes_on(const std::vector<InstanceBatch>& batches, int mesh) {
  return !batches.empty() && batches.back().mesh == mesh;
}

}  // namespace

void InstanceBatches::start() {
  slots_.clear();
  batches_.clear();
}

InstanceBatch& InstanceBatches::batch_for(int mesh) {
  if (!goes_on(batches_, mesh)) batches_.push_back({mesh, slots_.size(), 0});
  return batches_.back();
}

void InstanceBatches::add(int mesh, std::size_t record) {
  ++batch_for(mesh).count;
  slots_.push_back(static_cast<float>(record));
}

void InstanceBatches::add_run(int mesh, std::size_t first, std::size_t count) {
  if (count == 0) return;  // (an empty batch would split a run of the mesh before it)
  batch_for(mesh).count += count;
  for (std::size_t record = first; record < first + count; ++record) slots_.push_back(static_cast<float>(record));
}

}  // namespace redgem::render_math
