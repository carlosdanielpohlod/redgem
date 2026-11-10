#pragma once
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/TransformedShape.h>

#include "physics/debug_geometry.hpp"

namespace redgem::jolt {
// Walks leaf shapes, including decorators and compounds. Append after skipping triangles already
// sampled in an earlier pass; false when the output ceiling left triangles out.
bool append_debug_shape(const JPH::TransformedShape& shape, const PhysicsDebugTriangle& metadata, std::size_t skip,
                        std::size_t limit, PhysicsDebugGeometry& geometry);
}  // namespace redgem::jolt
