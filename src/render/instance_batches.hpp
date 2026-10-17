#pragma once

// The copies of each mesh a pass draws, gathered so that a mesh is drawn in one call per part for
// as many of its copies as come one after the other. What the graphics card reads for each copy is
// the number of its record (instance_records.hpp); a batch is a run of those numbers. Copies are
// drawn in the order they come, as one by one: surfaces that lie on each other (a window on a
// wall) are drawn over each other as before. Linked copies come one after the other in a level
// (Tree, Tree.001...), and so do scattered ones. No window and no graphics card needed, so it can
// be tested.

#include <cstddef>
#include <vector>

namespace redgem::render_math {

// Copies of one mesh: `count` record numbers from `first` on in InstanceBatches::slots.
struct InstanceBatch {
  int mesh = 0;
  std::size_t first = 0;
  std::size_t count = 0;
};

class InstanceBatches {
 public:
  // Starts a pass: what the last one gathered goes, the room it took stays (a frame allocates
  // nothing once its passes have run).
  void start();
  // A copy of mesh `mesh`, whose record is number `record`.
  void add(int mesh, std::size_t record);
  // `count` copies of `mesh` whose records follow each other from number `first` on.
  void add_run(int mesh, std::size_t first, std::size_t count);

  // Record numbers as the vertex shader takes them (floats hold every one there can be exactly).
  const std::vector<float>& slots() const { return slots_; }
  const std::vector<InstanceBatch>& batches() const { return batches_; }

 private:
  // The batch the next copy of `mesh` goes in: the last one when it is of that mesh, a new one
  // otherwise.
  InstanceBatch& batch_for(int mesh);

  std::vector<float> slots_;
  std::vector<InstanceBatch> batches_;
};

}  // namespace redgem::render_math
