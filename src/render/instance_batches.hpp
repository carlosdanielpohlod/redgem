#pragma once

// The copies of each mesh a pass draws, gathered by mesh so that a mesh is drawn in one call per
// part however many copies of it there are. What the graphics card reads for each copy is the
// number of its record (instance_records.hpp); a batch is a run of those numbers. No window and
// no graphics card needed, so it can be tested.

#include <cstddef>
#include <vector>

namespace redgem::render_math {

// The copies of one mesh: `count` record numbers from `first` on in InstanceBatches::slots.
struct InstanceBatch {
  int mesh = 0;
  std::size_t first = 0;
  std::size_t count = 0;
};

class InstanceBatches {
 public:
  // Starts a pass over a scene of `meshes` meshes: what the last pass gathered goes, the room it
  // took stays (a frame allocates nothing once its passes have run).
  void start(std::size_t meshes);
  // A copy of mesh `mesh`, whose record is number `record`.
  void add(int mesh, std::size_t record);
  // `count` copies of `mesh` whose records follow each other from number `first` on.
  void add_run(int mesh, std::size_t first, std::size_t count);
  // Lays the batches end to end, each mesh in the order its first copy came in: a scene's objects
  // are drawn in about the order they come in, as one by one.
  void lay_out();

  // Record numbers as the vertex shader takes them (floats hold every one there can be exactly).
  const std::vector<float>& slots() const { return slots_; }
  const std::vector<InstanceBatch>& batches() const { return batches_; }

 private:
  std::vector<float>& copies_of(int mesh);

  std::vector<std::vector<float>> by_mesh_;
  std::vector<int> order_;  // the meshes, as their first copy came in
  std::vector<float> slots_;
  std::vector<InstanceBatch> batches_;
};

}  // namespace redgem::render_math
