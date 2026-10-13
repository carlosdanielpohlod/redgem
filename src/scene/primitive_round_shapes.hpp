#pragma once

// The built-in shapes that go round the Y axis: 1 m across, the capsule 2 m tall.

#include "scene/scene_desc.hpp"

namespace redgem {

PrimitiveDesc make_sphere();
PrimitiveDesc make_cylinder();
PrimitiveDesc make_capsule();

}  // namespace redgem
