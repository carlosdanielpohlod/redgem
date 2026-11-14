#include "render/instance_batches.hpp"

namespace redgem::render_math {
namespace {

// Scattered copies are placed with positive scales alone (see scene/scatter.hpp).
constexpr bool kScatteredMirrored = false;

// Whether the last batch is of `mesh`, mirrored as asked: its next copy goes in it.
bool goes_on(const std::vector<InstanceBatch>& batches, int mesh, bool mirrored) {
  return !batches.empty() && batches.back().mesh == mesh && batches.back().mirrored == mirrored;
}

}  // namespace

void InstanceBatches::start() {
  slots_.clear();
  batches_.clear();
}

InstanceBatch& InstanceBatches::batch_for(int mesh, bool mirrored) {
  if (!goes_on(batches_, mesh, mirrored)) batches_.push_back({mesh, slots_.size(), 0, mirrored});
  return batches_.back();
}

void InstanceBatches::add(int mesh, std::size_t record, bool mirrored) {
  ++batch_for(mesh, mirrored).count;
  slots_.push_back(static_cast<float>(record));
}

void InstanceBatches::add_run(int mesh, std::size_t first, std::size_t count) {
  if (count == 0) return;  // (an empty batch would split a run of the mesh before it)
  batch_for(mesh, kScatteredMirrored).count += count;
  for (std::size_t record = first; record < first + count; ++record) slots_.push_back(static_cast<float>(record));
}

}  // namespace redgem::render_math
