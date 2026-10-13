#pragma once

#include <string>
#include <vector>

#include "scene/scene_desc.hpp"
#include "scene/property.hpp"

namespace redgem {

enum class BodyType { None, Static, Dynamic, Kinematic, Character, Vehicle };
enum class ColliderShape { Box, Sphere, Capsule, Mesh };

// What a Blender object asks for through its custom properties.
struct BodyConfig {
  BodyType type = BodyType::None;
  ColliderShape collider = ColliderShape::Box;
  bool trigger = false;
  float mass = 0.0f;
  std::vector<std::string> warnings;

  bool wants_physics() const { return type != BodyType::None || trigger; }
};

// Rules:
//  - `trigger = true` creates a trigger volume (the body type is ignored, with a warning if set).
//  - `collider` without `body` means a static body.
//  - `body` without `collider` uses a box, or a capsule for characters.
//  - mesh colliders are only allowed on static bodies.
//  - vehicles always use a box chassis; their wheels are child objects with `wheel = true`.
BodyConfig body_config_from_props(const PropertyMap& props);

// Whether forces move a body of that type: dynamic bodies, and vehicles (whose chassis is one).
bool is_dynamic(BodyType type);

// What the `body` property calls a type (scene/props.hpp has the words).
const char* body_type_name(BodyType type);

// The box an object without a mesh (an empty) collides as and is drawn as: a small cube around it.
Bounds default_collider_bounds();

}  // namespace redgem
