#pragma once

#include <optional>

#include "core/math.hpp"
#include "scene/scene.hpp"
#include "scene/scene_desc.hpp"
#include "scene/transform.hpp"

namespace redgem {

struct PickRay {
  Vec3 origin;
  Vec3 direction;  // normalized
};

// Distance along the ray to the box `local` placed by `world` (rotation and scale included).
std::optional<float> intersect_ray_box(const PickRay& ray, const Transform& world, const Bounds& local);

// Closest visible object with a mesh hit by the ray, or kNoObject.
ObjectId pick_object(const Scene& scene, const SceneDesc& desc, const PickRay& ray);

// World-space center and bounding radius of an object (mesh bounds, or a small sphere without mesh).
void object_extent(const Scene& scene, const SceneDesc& desc, ObjectId id, Vec3& center, float& radius);

}  // namespace redgem
