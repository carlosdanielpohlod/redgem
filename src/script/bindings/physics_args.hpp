#pragma once

// What the physics natives read from Ruby besides numbers and vectors: collision layers by name,
// the objects a query ignores, and the shape a query sweeps or tests. Checked as every native's
// arguments are (see binding_helpers.hpp).

#include <mruby.h>

#include <cstddef>
#include <vector>

#include "physics/physics_world.hpp"

namespace redgem::binding {

// One of the game's collision layers, by name (a Symbol or a String): an ArgumentError naming the
// layers there are for any other name.
std::size_t layer_arg(mrb_state* mrb, mrb_value name);
// The layers of an Array of names; every layer for nil.
LayerMask layer_mask_arg(mrb_state* mrb, mrb_value names);
// A layer as scripts see it: its name, as a Symbol.
mrb_value layer_value(mrb_state* mrb, std::size_t layer);

// The objects of an Array of GameObjects that are still there (a destroyed one ignores nothing).
std::vector<ObjectId> objects_arg(mrb_state* mrb, mrb_value objects);

// A query's shape: `kind` is :sphere, :box or :capsule; `size` is [radius, _, _] for a sphere,
// the box's size, or [radius, height, _] for a capsule; `rotation` is in degrees (nil: none).
QueryShape query_shape_arg(mrb_state* mrb, mrb_value kind, mrb_value size, mrb_value rotation);

}  // namespace redgem::binding
