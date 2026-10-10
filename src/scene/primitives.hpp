#pragma once

#include <optional>
#include <string>
#include <vector>

#include "scene/scene_desc.hpp"

namespace redgem {

// Built-in shapes the editor can create: 1 m cube, 1 m sphere / cylinder,
// 10 m plane, 2 m tall capsule. All centered on the origin (the plane lies on y = 0), Y up.
enum class PrimitiveShape { Cube, Sphere, Plane, Cylinder, Capsule };

PrimitiveDesc make_primitive(PrimitiveShape shape);
const char* primitive_name(PrimitiveShape shape);
std::optional<PrimitiveShape> primitive_from_name(const std::string& name);
const std::vector<PrimitiveShape>& all_primitives();

// The physics collider that fits each shape.
const char* primitive_collider(PrimitiveShape shape);

}  // namespace redgem
