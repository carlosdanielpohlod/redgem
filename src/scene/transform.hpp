#pragma once

#include "core/math.hpp"

namespace redgem {

// Translation / rotation / scale of one object, without the hierarchy.
struct Transform {
  Vec3 position = Vec3::zero();
  Quat rotation = Quat::identity();
  Vec3 scale = Vec3::one();

  Vec3 forward() const { return rotate(rotation, Vec3::forward()); }
  Vec3 right() const { return rotate(rotation, Vec3::right()); }
  Vec3 up() const { return rotate(rotation, Vec3::up()); }
};

// World transform of a child given its parent's world transform.
// Non-uniform parent scale combined with child rotation is approximated (no shear), as in most engines.
Transform combine(const Transform& parent_world, const Transform& child_local);

// Local transform that produces `world` under `parent_world`. Inverse of combine().
Transform relative_to(const Transform& parent_world, const Transform& world);

// Splits a column-major 4x4 TRS matrix (glTF / OpenGL layout) into a Transform (no shear).
Transform transform_from_matrix(const float* column_major);

}  // namespace redgem
